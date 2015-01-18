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
	struct sockaddr_ll dest_addr;
	char *buf = NULL;
	socklen_t addrlen;
	int result = -1;
	int j = 0;

	// Source mac address
	unsigned char src_mac[6] = {0x00, 0x0C, 0x29, 0xD9, 0x08, 0xF6};

	// Destination mac address
	unsigned char dest_mac[6] = {0x00, 0x0C, 0x29, 0x49, 0x3F, 0x65};
	unsigned char* ether_head = NULL;
	unsigned char* ether_data = NULL;
	
	/*another pointer to ethernet header*/
	struct ethhdr *eh = NULL;


	buf = malloc(ETH_FRAME_LEN);
	ether_head = buf;
	ether_data = buf + 14;
	eh = (struct ethhdr *)ether_head;

	memset(&dest_addr, 0, sizeof(struct sockaddr_ll));

	sockfd = socket(AF_PACKET, SOCK_RAW, htons(0x3381));
	if (sockfd < 0) {
		goto socket_error;
	}
	// VM2 to VM1 communication
	dest_addr.sll_family   = PF_PACKET;	
	dest_addr.sll_protocol = htons(0x3381);	
	dest_addr.sll_ifindex  = 3;
	dest_addr.sll_hatype   = ARPHRD_ETHER;
	dest_addr.sll_pkttype  = PACKET_OTHERHOST;
	dest_addr.sll_halen    = ETH_ALEN;		
	dest_addr.sll_addr[0]  = 0x00;		
	dest_addr.sll_addr[1]  = 0x0C;		
	dest_addr.sll_addr[2]  = 0x29;
	dest_addr.sll_addr[3]  = 0x49;
	dest_addr.sll_addr[4]  = 0x3F;
	dest_addr.sll_addr[5]  = 0x65;
	dest_addr.sll_addr[6]  = 0x00;/*not used*/
	dest_addr.sll_addr[7]  = 0x00;/*not used*/
	
	/*set the frame header*/
	memcpy((void*)buf, (void*)dest_mac, ETH_ALEN);
	memcpy((void*)(buf+ETH_ALEN), (void*)src_mac, ETH_ALEN);
	eh->h_proto = htons(0x3381);
	/*fill the frame with some data*/
	for (j = 0; j < 10; j++) {
		//		ether_data[j] = (unsigned char)((int) (255.0*rand()/(RAND_MAX+1.0)));
		ether_data[j] = 'A';
	}
	ether_data[j] = '\0';

	result = sendto(sockfd, buf, ETH_FRAME_LEN, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
	if (result < 0) {
		goto send_error;
	}
	return 0;

 socket_error:
	perror("Socket error: ");
	return -1;
 send_error:
	perror("Send error: ");
	return -1;
}
