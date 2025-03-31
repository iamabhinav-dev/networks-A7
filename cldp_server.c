/*
=====================================
Assignment 7 Submission
Name: <Your_Name>
Roll number: <Your_Roll_Number>
=====================================
*/

#include "cldp_common.h"
#include <signal.h>

volatile int keep_running = 1;

/* Handle Ctrl+C */
void signal_handler(int sig) {
    keep_running = 0;
}

int main() {
    int sock_fd, recv_len;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    unsigned char buffer[1500]; /* Buffer for packet */
    
    /* Get local IP address */
    char hostname[256];
    struct hostent *host_entry;
    gethostname(hostname, sizeof(hostname));
    host_entry = gethostbyname(hostname);
    char *local_ip = inet_ntoa(*((struct in_addr *)host_entry->h_addr_list[0]));
    uint32_t local_ip_int = inet_addr(local_ip);
    
    printf("Server starting on %s\n", local_ip);
    
    /* Create raw socket */
    sock_fd = socket(AF_INET, SOCK_RAW, CLDP_PROTOCOL);
    if (sock_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    /* Set up signal handler */
    signal(SIGINT, signal_handler);
    
    /* Main server loop */
    uint16_t transaction_counter = 0;
    
    /* Set up HELLO broadcast timer */
    struct timeval last_hello_time, current_time;
    gettimeofday(&last_hello_time, NULL);
    
    while (keep_running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock_fd, &read_fds);
        
        /* Set timeout for select to 1 second */
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        /* Wait for incoming packets or timeout */
        int select_result = select(sock_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        /* Check if it's time to send a HELLO message (every 10 seconds) */
        gettimeofday(&current_time, NULL);
        if (current_time.tv_sec - last_hello_time.tv_sec >= 10) {
            /* Create HELLO packet */
            memset(buffer, 0, sizeof(buffer));
            
            /* Craft IP header */
            struct iphdr *ip_header = (struct iphdr *)buffer;
            
            /* Get broadcast address */
            struct sockaddr_in broadcast_addr;
            broadcast_addr.sin_family = AF_INET;
            broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
            
            /* Prepare CLDP header after IP header */
            struct cldp_header *cldp_hdr = (struct cldp_header *)(buffer + sizeof(struct iphdr));
            cldp_hdr->type = CLDP_HELLO;
            cldp_hdr->transaction_id = htons(transaction_counter++);
            cldp_hdr->reserved = 0;
            
            /* Add hostname as payload */
            char hostname[256];
            get_hostname(hostname, sizeof(hostname));
            int hostname_len = strlen(hostname) + 1; /* Include null terminator */
            cldp_hdr->payload_len = hostname_len;
            
            /* Copy hostname to payload area */
            memcpy(buffer + sizeof(struct iphdr) + sizeof(struct cldp_header), 
                   hostname, hostname_len);
            
            /* Set IP header */
            int total_len = sizeof(struct iphdr) + sizeof(struct cldp_header) + hostname_len;
            build_ip_header(ip_header, local_ip_int, broadcast_addr.sin_addr.s_addr, total_len);
            
            /* Set socket option to include IP header */
            int on = 1;
            if (setsockopt(sock_fd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
                perror("setsockopt() failed");
                exit(EXIT_FAILURE);
            }
            
            /* Send HELLO packet */
            if (sendto(sock_fd, buffer, total_len, 0, 
                       (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
                perror("sendto() failed");
                exit(EXIT_FAILURE);
            }
            
            printf("Sent HELLO broadcast\n");
            gettimeofday(&last_hello_time, NULL);
        }
        
        /* Check if we have received a packet */
        if (select_result > 0 && FD_ISSET(sock_fd, &read_fds)) {
            /* Receive packet */
            recv_len = recvfrom(sock_fd, buffer, sizeof(buffer), 0, 
                               (struct sockaddr *)&client_addr, &client_addr_len);
            
            if (recv_len < 0) {
                perror("recvfrom() failed");
                continue;
            }
            
            /* Process only if it's a CLDP packet */
            struct iphdr *ip_header = (struct iphdr *)buffer;
            
            /* Skip packets from ourselves */
            if (ip_header->saddr == local_ip_int) {
                continue;
            }
            
            /* Verify protocol is CLDP */
            if (ip_header->protocol != CLDP_PROTOCOL) {
                continue;
            }
            
            /* Process CLDP packet */
            struct cldp_header *cldp_hdr = (struct cldp_header *)(buffer + sizeof(struct iphdr));
            
            /* Handle based on message type */
            if (cldp_hdr->type == CLDP_QUERY) {
                printf("Received QUERY from %s\n", inet_ntoa(client_addr.sin_addr));
                
                /* Extract query type from payload */
                uint8_t query_type = *(uint8_t *)(buffer + sizeof(struct iphdr) + 
                                                  sizeof(struct cldp_header));
                
                /* Create response packet */
                unsigned char response[1500];
                memset(response, 0, sizeof(response));
                
                /* Craft IP header */
                struct iphdr *resp_ip_header = (struct iphdr *)response;
                
                /* Prepare CLDP header after IP header */
                struct cldp_header *resp_cldp_hdr = (struct cldp_header *)(response + sizeof(struct iphdr));
                resp_cldp_hdr->type = CLDP_RESPONSE;
                resp_cldp_hdr->transaction_id = cldp_hdr->transaction_id;  /* Same transaction ID */
                resp_cldp_hdr->reserved = 0;
                
                /* Offset for payload */
                int payload_offset = sizeof(struct iphdr) + sizeof(struct cldp_header);
                
                /* Set query type as first byte of payload */
                response[payload_offset] = query_type;
                int payload_len = 1;  /* Start with 1 for query type */
                
                /* Handle different query types */
                switch (query_type) {
                    case QUERY_HOSTNAME: {
                        char hostname[256];
                        get_hostname(hostname, sizeof(hostname));
                        int hostname_len = strlen(hostname) + 1;  /* Include null terminator */
                        memcpy(response + payload_offset + 1, hostname, hostname_len);
                        payload_len += hostname_len;
                        break;
                    }
                    case QUERY_SYSTIME: {
                        uint64_t sys_time = get_system_time();
                        memcpy(response + payload_offset + 1, &sys_time, sizeof(sys_time));
                        payload_len += sizeof(sys_time);
                        break;
                    }
                    case QUERY_CPULOAD: {
                        float load[3];
                        get_cpu_load(load);
                        memcpy(response + payload_offset + 1, load, sizeof(load));
                        payload_len += sizeof(load);
                        break;
                    }
                    default:
                        printf("Unknown query type: %d\n", query_type);
                        continue;  /* Skip unknown query types */
                }
                
                /* Set payload length in header */
                resp_cldp_hdr->payload_len = payload_len;
                
                /* Set IP header */
                int total_len = sizeof(struct iphdr) + sizeof(struct cldp_header) + payload_len;
                build_ip_header(resp_ip_header, local_ip_int, ip_header->saddr, total_len);
                
                /* Send response packet */
                if (sendto(sock_fd, response, total_len, 0, 
                           (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                    perror("sendto() failed for response");
                }
                
                printf("Sent RESPONSE to %s for query type %d\n", 
                       inet_ntoa(client_addr.sin_addr), query_type);
            }
            /* We could handle HELLO messages here to maintain a list of active nodes */
        }
    }
    
    /* Clean up */
    close(sock_fd);
    printf("Server terminated\n");
    
    return 0;
}