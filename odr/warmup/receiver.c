#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void)
{
	int sockfd = -1;
	struct sockaddr_ll src_addr;
	char *buf = NULL;
	socklen_t addrlen;
	int n = -1;
	struct ethhdr *eh = NULL;

	buf = malloc(ETH_FRAME_LEN);
	memset(buf, 0, ETH_FRAME_LEN);
	memset(&src_addr, 0, sizeof(struct sockaddr_ll));

	sockfd = socket(AF_PACKET, SOCK_RAW, htons(0x3381));
	if (sockfd < 0) {
		goto error;
	}
	while(1) {
		n = recvfrom(sockfd, buf, ETH_FRAME_LEN, 0, (struct sockaddr*)&src_addr, &addrlen);
		if (n < 0) {
			break;
		}
		printf("Frame length = %d\n", n);
		printf("The sockaddr_ll values are: \n");
		printf("sll_family: %hu\n", src_addr.sll_family);
		printf("sll_protocol: %x\n", ntohs(src_addr.sll_protocol));
		printf("sll_ifindex: %u\n", src_addr.sll_ifindex);
		printf("sll_hatype: %hu\n", src_addr.sll_hatype);
		printf("sll_pkttype: %u\n", src_addr.sll_pkttype);
		printf("sll_halen: %u\n", src_addr.sll_halen);
		printf("sll_addr: %s\n", src_addr.sll_addr);
		
		eh = (struct ethhdr*) buf;
		printf("The source addr = %x %x %x %x %x %x\n", 
		       eh->h_source[0],
		       eh->h_source[1],
		       eh->h_source[2],
		       eh->h_source[3],
		       eh->h_source[4],
		       eh->h_source[5]);
		printf("The destination addr = %x %x %x %x %x %x\n", 
		       eh->h_dest[0],
		       eh->h_dest[1],
		       eh->h_dest[2],
		       eh->h_dest[3],
		       eh->h_dest[4],
		       eh->h_dest[5]);

		
	}
	return 0;

 error:
	fprintf(stderr, "Error happened.\n");
}
