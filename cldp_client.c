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

/* Structure to track active nodes */
#define MAX_NODES 256
struct node_info {
    uint32_t ip;
    char hostname[256];
    time_t last_seen;
    int active;
};

struct node_info nodes[MAX_NODES];
int node_count = 0;

/* Find node by IP address */
int find_node(uint32_t ip) {
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].ip == ip) {
            return i;
        }
    }
    return -1;
}

/* Display node information */
void display_nodes() {
    printf("\n===== Active Nodes =====\n");
    time_t now = time(NULL);
    int active_count = 0;
    
    for (int i = 0; i < node_count; i++) {
        /* Consider nodes active if seen in the last 30 seconds */
        if (now - nodes[i].last_seen < 30) {
            char ip_str[INET_ADDRSTRLEN];
            struct in_addr ip_addr;
            ip_addr.s_addr = nodes[i].ip;
            inet_ntop(AF_INET, &ip_addr, ip_str, INET_ADDRSTRLEN);
            
            printf("Node %d: %s (%s)\n", i+1, ip_str, nodes[i].hostname);
            active_count++;
        }
    }
    
    if (active_count == 0) {
        printf("No active nodes found\n");
    }
    printf("========================\n\n");
}

/* Parse RESPONSE payload */
void parse_response(unsigned char *payload, int payload_len) {
    /* First byte is the query type */
    uint8_t query_type = payload[0];
    
    switch (query_type) {
        case QUERY_HOSTNAME: {
            char *hostname = (char *)(payload + 1);
            printf("  Hostname: %s\n", hostname);
            break;
        }
        case QUERY_SYSTIME: {
            uint64_t sys_time = *(uint64_t *)(payload + 1);
            char time_str[64];
            time_t time_val = (time_t)sys_time;
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&time_val));
            printf("  System Time: %s\n", time_str);
            break;
        }
        case QUERY_CPULOAD: {
            float *load = (float *)(payload + 1);
            printf("  CPU Load: %.2f, %.2f, %.2f (1, 5, 15 min averages)\n", 
                   load[0], load[1], load[2]);
            break;
        }
        default:
            printf("  Unknown query type: %d\n", query_type);
    }
}

int main() {
    int sock_fd, recv_len;
    struct sockaddr_in server_addr, from_addr;
    socklen_t from_addr_len = sizeof(from_addr);
    unsigned char buffer[1500]; /* Buffer for packet */
    
    /* Get local IP address */
    char hostname[256];
    struct hostent *host_entry;
    gethostname(hostname, sizeof(hostname));
    host_entry = gethostbyname(hostname);
    char *local_ip = inet_ntoa(*((struct in_addr *)host_entry->h_addr_list[0]));
    uint32_t local_ip_int = inet_addr(local_ip);
    
    printf("Client starting on %s\n", local_ip);
    
    /* Create raw socket */
    sock_fd = socket(AF_INET, SOCK_RAW, CLDP_PROTOCOL);
    if (sock_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    /* Set socket option to include IP header */
    int on = 1;
    if (setsockopt(sock_fd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
        perror("setsockopt() failed");
        exit(EXIT_FAILURE);
    }
    
    /* Set up signal handler */
    signal(SIGINT, signal_handler);
    
    /* Initialize the main loop */
    uint16_t transaction_counter = 0;
    time_t last_query_time = 0;
    
    printf("CLDP Client Running...\n");
    printf("Type 'q' to query all nodes for information\n");
    printf("Type 'd' to display active nodes\n");
    printf("Type 'x' to exit\n\n");
    
    /* Set up non-blocking stdin */
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    
    while (keep_running) {
        /* Check user input */
        char input;
        if (read(STDIN_FILENO, &input, 1) > 0) {
            if (input == 'x') {
                break;
            } else if (input == 'd') {
                display_nodes();
            } else if (input == 'q') {
                /* Send queries to all active nodes */
                time_t now = time(NULL);
                for (int i = 0; i < node_count; i++) {
                    /* Only query nodes active in the last 30 seconds */
                    if (now - nodes[i].last_seen < 30) {
                        uint16_t transaction_id = transaction_counter++;
                        
                        /* Create queries for all supported metadata types */
                        uint8_t query_types[] = {QUERY_HOSTNAME, QUERY_SYSTIME, QUERY_CPULOAD};
                        int num_queries = sizeof(query_types) / sizeof(query_types[0]);
                        
                        for (int j = 0; j < num_queries; j++) {
                            /* Create QUERY packet */
                            memset(buffer, 0, sizeof(buffer));
                            
                            /* Craft IP header */
                            struct iphdr *ip_header = (struct iphdr *)buffer;
                            
                            /* Prepare CLDP header after IP header */
                            struct cldp_header *cldp_hdr = (struct cldp_header *)(buffer + sizeof(struct iphdr));
                            cldp_hdr->type = CLDP_QUERY;
                            cldp_hdr->transaction_id = htons(transaction_id);
                            cldp_hdr->reserved = 0;
                            
                            /* Add query type as payload */
                            buffer[sizeof(struct iphdr) + sizeof(struct cldp_header)] = query_types[j];
                            cldp_hdr->payload_len = 1;
                            
                            /* Set IP header */
                            int total_len = sizeof(struct iphdr) + sizeof(struct cldp_header) + 1;
                            build_ip_header(ip_header, local_ip_int, nodes[i].ip, total_len);
                            
                            /* Set up destination address */
                            struct sockaddr_in dest_addr;
                            dest_addr.sin_family = AF_INET;
                            dest_addr.sin_addr.s_addr = nodes[i].ip;
                            
                            /* Send QUERY packet */
                            if (sendto(sock_fd, buffer, total_len, 0, 
                                      (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
                                perror("sendto() failed for query");
                            }
                            
                            char ip_str[INET_ADDRSTRLEN];
                            struct in_addr ip_addr;
                            ip_addr.s_addr = nodes[i].ip;
                            inet_ntop(AF_INET, &ip_addr, ip_str, INET_ADDRSTRLEN);
                            printf("Sent QUERY type %d to %s\n", query_types[j], ip_str);
                        }
                    }
                }
                last_query_time = now;
            }
        }
        
        /* Check for incoming packets (non-blocking) */
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock_fd, &read_fds);
        
        /* Set timeout for select to 100ms */
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        
        int select_result = select(sock_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (select_result > 0) {
            /* Receive packet */
            recv_len = recvfrom(sock_fd, buffer, sizeof(buffer), 0, 
                               (struct sockaddr *)&from_addr, &from_addr_len);
            
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
            if (cldp_hdr->type == CLDP_HELLO) {
                /* Extract hostname from payload */
                char *remote_hostname = (char *)(buffer + sizeof(struct iphdr) + sizeof(struct cldp_header));
                
                /* Find or add node */
                int node_idx = find_node(ip_header->saddr);
                if (node_idx < 0) {
                    /* Add new node */
                    if (node_count < MAX_NODES) {
                        nodes[node_count].ip = ip_header->saddr;
                        strncpy(nodes[node_count].hostname, remote_hostname, sizeof(nodes[node_count].hostname) - 1);
                        nodes[node_count].last_seen = time(NULL);
                        nodes[node_count].active = 1;
                        
                        char ip_str[INET_ADDRSTRLEN];
                        struct in_addr ip_addr;
                        ip_addr.s_addr = ip_header->saddr;
                        inet_ntop(AF_INET, &ip_addr, ip_str, INET_ADDRSTRLEN);
                        printf("Discovered new node: %s (%s)\n", ip_str, remote_hostname);
                        
                        node_count++;
                    }
                } else {
                    /* Update existing node */
                    nodes[node_idx].last_seen = time(NULL);
                    strncpy(nodes[node_idx].hostname, remote_hostname, sizeof(nodes[node_idx].hostname) - 1);
                }
            } else if (cldp_hdr->type == CLDP_RESPONSE) {
                char ip_str[INET_ADDRSTRLEN];
                struct in_addr ip_addr;
                ip_addr.s_addr = ip_header->saddr;
                inet_ntop(AF_INET, &ip_addr, ip_str, INET_ADDRSTRLEN);
                
                printf("Received RESPONSE from %s:\n", ip_str);
                
                /* Extract and parse payload */
                unsigned char *payload = buffer + sizeof(struct iphdr) + sizeof(struct cldp_header);
                int payload_len = cldp_hdr->payload_len;
                
                /* Parse the response based on query type */
                parse_response(payload, payload_len);
            }
        }
    }
    
    /* Clean up */
    close(sock_fd);
    printf("Client terminated\n");
    
    return 0;
}