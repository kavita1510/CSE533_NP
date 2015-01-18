/**
 * Test file
 */

#include "arp.h"

int main(void)
{
	struct sockaddr_in ipaddr;
	socklen_t sockaddrlen;
	struct hwaddr HWaddr;
	int i;

	HWaddr.sll_ifindex = 0;
	HWaddr.sll_hatype = 0;
	HWaddr.sll_halen = 0;
	for (i = 0 ; i<10; i++) {
		HWaddr.sll_addr[i] = 0;
	}

	ipaddr.sin_family = AF_INET;
	ipaddr.sin_port = 0x0;
	inet_pton(AF_INET, "130.245.156.22", &ipaddr.sin_addr);

	sockaddrlen = sizeof(struct sockaddr_in);
	areq((struct sockaddr*)&ipaddr, sockaddrlen, &HWaddr);

	printf("The HW address is: %x:%x:%x:%x:%x:%x\n", HWaddr.sll_addr[0], HWaddr.sll_addr[1], HWaddr.sll_addr[2], HWaddr.sll_addr[3], HWaddr.sll_addr[4], HWaddr.sll_addr[5]);
	printf("The interface is %d\n", HWaddr.sll_ifindex);
	return 0;
}
