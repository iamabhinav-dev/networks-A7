#ifndef CLDP_COMMON_H
#define CLDP_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <fcntl.h>
#include <netdb.h>


/* Protocol constants */
#define CLDP_PROTOCOL 253     /* Custom protocol number */
#define CLDP_HELLO 0x01
#define CLDP_QUERY 0x02
#define CLDP_RESPONSE 0x03

/* Query types */
#define QUERY_HOSTNAME 0x01
#define QUERY_SYSTIME 0x02
#define QUERY_CPULOAD 0x03

/* Packet structure */
struct cldp_header {
    uint8_t type;             /* Message type */
    uint8_t payload_len;      /* Length of payload */
    uint16_t transaction_id;  /* Transaction ID */
    uint32_t reserved;        /* Reserved for future use */
};

/* Utility functions for packet handling */
/* Calculate IP checksum */
unsigned short calculate_checksum(unsigned short *addr, int len) {
    long sum = 0;
    unsigned short checksum;
    
    while (len > 1) {
        sum += *addr++;
        len -= 2;
    }
    
    if (len == 1) {
        sum += *(unsigned char*)addr;
    }
    
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    checksum = ~sum;
    
    return checksum;
}

/* Function to build the IP header */
void build_ip_header(struct iphdr *ip_header, 
                     uint32_t src_ip, uint32_t dst_ip, 
                     uint16_t total_len) {
    ip_header->version = 4;                   /* IPv4 */
    ip_header->ihl = 5;                       /* Header length in 32-bit words */
    ip_header->tos = 0;                       /* Type of service */
    ip_header->tot_len = htons(total_len);    /* Total length */
    ip_header->id = htons(rand() % 65535);    /* ID field */
    ip_header->frag_off = 0;                  /* Fragment offset */
    ip_header->ttl = 64;                      /* Time to live */
    ip_header->protocol = CLDP_PROTOCOL;      /* Protocol */
    ip_header->check = 0;                     /* Checksum (to be calculated) */
    ip_header->saddr = src_ip;                /* Source address */
    ip_header->daddr = dst_ip;                /* Destination address */
    
    /* Calculate checksum */
    ip_header->check = calculate_checksum((unsigned short *)ip_header, ip_header->ihl * 4);
}

/* Get hostname for HELLO and QUERY_HOSTNAME */
void get_hostname(char *buffer, int max_len) {
    gethostname(buffer, max_len);
}

/* Get system time for QUERY_SYSTIME */
uint64_t get_system_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec;
}

/* Get CPU load for QUERY_CPULOAD */
void get_cpu_load(float *load) {
    struct sysinfo si;
    sysinfo(&si);
    
    /* Load average for 1, 5, and 15 minutes */
    load[0] = si.loads[0] / (float)(1 << SI_LOAD_SHIFT);
    load[1] = si.loads[1] / (float)(1 << SI_LOAD_SHIFT);
    load[2] = si.loads[2] / (float)(1 << SI_LOAD_SHIFT);
}

#endif /* CLDP_COMMON_H */