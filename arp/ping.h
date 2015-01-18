#include	"unp.h"
#include	<netinet/in_systm.h>
#include	<netinet/ip.h>
#include	<netinet/ip_icmp.h>

#define	BUFSIZE	       64
#define ICMP_ID 0x6872
#define DATALEN 56
#define IP_ADDR_LEN 16
#define IP4_HDRLEN 20

int nsent;

/* globals */

uint16_t	in_cksum(uint16_t *, int);


int do_ping(int sockfd, char* src_ip, char* dest_ip, unsigned char* src_mac, int src_ifindex, unsigned char* dest_mac);
