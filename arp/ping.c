#include "ping.h"
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern int self_ifindex;

void send_ping_eth_frame(int sockfd, unsigned char *buf, int buflen, uint8_t* src_mac, int src_ifindex, uint8_t* dest_mac)
{
	struct sockaddr_ll dest_addr;
	int result = -1;
	int j = 0;
	unsigned char* eth_buf = NULL;

	unsigned char* ether_head = NULL;
	unsigned char* ether_data = NULL;
	
	/*another pointer to ethernet header*/
	struct ethhdr *eh = NULL;

	int len = buflen + sizeof(struct ethhdr);
	//	self_ifindex = 2;
	eth_buf = malloc(len);
	memset(eth_buf, 0, len);
	ether_head = eth_buf;
	ether_data = eth_buf + 14;
	eh = (struct ethhdr *)ether_head;

	memset(&dest_addr, 0, sizeof(struct sockaddr_ll));


	dest_addr.sll_family   = PF_PACKET;	
	dest_addr.sll_protocol = htons(ETH_P_IP);	
	dest_addr.sll_ifindex  = self_ifindex;
	dest_addr.sll_hatype   = ARPHRD_ETHER;
	dest_addr.sll_pkttype  = PACKET_OTHERHOST;
	dest_addr.sll_halen    = ETH_ALEN;	
	for (j=0; j<6; j++) {
		dest_addr.sll_addr[j] = dest_mac[j];
	}
	dest_addr.sll_addr[6]  = 0x00;/*not used*/
	dest_addr.sll_addr[7]  = 0x00;/*not used*/
	
	/*set the frame header*/
	memcpy((void*)eth_buf, (void*)dest_mac, ETH_ALEN);
	memcpy((void*)(eth_buf+ETH_ALEN), (void*)src_mac, ETH_ALEN);
	eh->h_proto = htons(ETH_P_IP);
	
	memcpy(ether_data, buf, buflen);
	
	printf("Sending out ethernet frame with src mac : %x:%x:%x:%x:%x:%x:%x:%x and dest mac : %x:%x:%x:%x:%x:%x:%x:%x and proto = %d on src_ifindex = %d\n",
	       src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4], src_mac[5], src_mac[6], src_mac[7],
	       dest_mac[0], dest_mac[1], dest_mac[2], dest_mac[3], dest_mac[4], dest_mac[5], dest_mac[6], dest_mac[7],
	       eh->h_proto, self_ifindex);
	result = sendto(sockfd, eth_buf, len, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
	if (result < 0) {
		goto send_error;
	}
	return;
 send_error:
	perror("Send error: ");
	exit(0);

}

void encapsulate_in_ip (unsigned char** out_buf, int *out_size, unsigned char* in_buf, int in_size, char *src_ip, char* dest_ip)
{
    struct ip *iphdr;
    int datalen = 0;
    int *ip_flags;
    
    struct sockaddr_in src_addr;
    struct sockaddr_in dst_addr;

    ip_flags = malloc(4*sizeof(int));
    *out_size = IP4_HDRLEN + in_size;
    (*out_buf) = malloc(IP4_HDRLEN + in_size);

    datalen = in_size;

    iphdr = (struct ip*)(*out_buf);

    // IPv4 header

    // IPv4 header length (4 bits): Number of 32-bit words in header = 5
    iphdr->ip_hl = IP4_HDRLEN / sizeof (uint32_t);

    // Internet Protocol version (4 bits): IPv4
    iphdr->ip_v = 4;

    // Type of service (8 bits)
    iphdr->ip_tos = 0;

    // Total length of datagram (16 bits): IP header + tour_pack
    iphdr->ip_len = htons(IP4_HDRLEN + datalen);

    // ID sequence number (16 bits): unused, since single datagram
    iphdr->ip_id = htons(getpid());

    // Flags, and Fragmentation offset (3, 13 bits): 0 since single datagram

    // Zero (1 bit)
    ip_flags[0] = 0;

    // Do not fragment flag (1 bit)
    ip_flags[1] = 0;

    // More fragments following flag (1 bit)
    ip_flags[2] = 0;

    // Fragmentation offset (13 bits)
    ip_flags[3] = 0;

    iphdr->ip_off = htons ((ip_flags[0] << 15)
            + (ip_flags[1] << 14)
            + (ip_flags[2] << 13)
            +  ip_flags[3]);

    // Time-to-Live (8 bits): default to maximum value
    iphdr->ip_ttl = 255;

    // Transport layer protocol (8 bits): 1 for ICMP
    iphdr->ip_p = 1;

    // Source IPv4 address (32 bits)
    inet_pton(AF_INET, src_ip, &(src_addr.sin_addr));
    inet_pton(AF_INET, dest_ip, &(dst_addr.sin_addr));
    memcpy(&(iphdr->ip_src), &src_addr.sin_addr.s_addr, IP_ADDR_LEN);
    memcpy(&(iphdr->ip_dst), &dst_addr.sin_addr.s_addr, IP_ADDR_LEN);


    memcpy(*out_buf + IP4_HDRLEN, in_buf, in_size);

    iphdr->ip_sum = 0;
    iphdr->ip_sum = in_cksum ((uint16_t *) iphdr, IP4_HDRLEN);

}

int do_ping(int sockfd, char* src_ip, char* dest_ip, unsigned char* src_mac, int src_ifindex, unsigned char* dest_mac)
{
	int len, n = 0;
	struct icmp *icmp;
	unsigned char* icmpbuf = NULL;
	unsigned char* ipbuf = NULL;

	icmpbuf = malloc(BUFSIZE);
	icmp = (struct icmp *) icmpbuf;
	icmp->icmp_type = ICMP_ECHO;
	icmp->icmp_code = 0;
	icmp->icmp_id = htons(ICMP_ID);
	
	icmp->icmp_seq = htons(++nsent);
	memset(icmp->icmp_data, 0xa5, DATALEN);	/* fill with pattern */
	n = gettimeofday((struct timeval *) icmp->icmp_data, NULL);
	
	if (n < 0) {
		perror("Error getting time of day: ");
		return -1;
	}

	len = 8 + DATALEN;		/* checksum ICMP header and data */
	icmp->icmp_cksum = 0;
	icmp->icmp_cksum = in_cksum((u_short *) icmp, len);

	encapsulate_in_ip(&ipbuf, &len, icmpbuf, BUFSIZE, src_ip, dest_ip);
	send_ping_eth_frame(sockfd, ipbuf, len, src_mac, src_ifindex, dest_mac);
	return len;
}

/*
 * in_cksum --
 *	Checksum routine for Internet Protocol family headers (C Version)
 */

uint16_t in_cksum(uint16_t* addr, int len)
{
	int nleft = len;
	uint16_t *w = addr;
	int sum = 0;
	uint16_t answer = 0;

	/*
	 * Our algorithm is simple, using a 32 bit accumulator (sum), we add
	 * sequential 16 bit words to it, and at the end, fold back all the
	 * carry bits from the top 16 bits into the lower 16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		*(unsigned char *)(&answer) = *(unsigned char *)w ;
		sum += answer;
	}

	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return (answer);
}

int dmain(void)
{
	unsigned char dest_mac[] = {0x90, 0xb1, 0x1c, 0x5e, 0xdf, 0x8d, 0x00, 0x00};
	unsigned char src_mac[] = {0x00, 0x26, 0x22, 0xc1, 0xb1, 0x1b, 0x00, 0x00};
	int src_ifindex = 2;
	int sockfd = -1;
	int sockfd_pg = -1;
	int data_len, n = 0;
	void *buf = NULL;
	struct icmp *icmp = NULL;
	struct sockaddr_in src;
	socklen_t srclen;

	srclen = sizeof(src); 
	sockfd = socket(AF_PACKET, SOCK_RAW, ETH_P_IP);
	sockfd_pg = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	
	buf = malloc(1500);
	data_len = 1500;

	if (sockfd_pg < 0) {
		goto sock_error;
	}
	while(1) {
		do_ping(sockfd, "10.22.17.173", "10.22.17.20", src_mac, src_ifindex, dest_mac);

		n = recvfrom(sockfd_pg, buf, data_len, 0, (SA*)&src, &srclen);
		if (n < 0) {
			goto error;
		}
		
		icmp = buf + IP4_HDRLEN;
		printf("ICMP packet received of type = %d and code = %d and icmp_id = %x\n", icmp->icmp_type, icmp->icmp_code, icmp->icmp_id);

		sleep(2);
	}
	return 0;

 sock_error:
	perror("Sock error: ");
	return -1;
 error:
	perror("Receive error: ");
	return -1;
}
