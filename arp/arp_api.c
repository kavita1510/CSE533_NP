/**
 * The ARP API file.
 */
#include "unp.h"
#include "arp.h"

int areq (struct sockaddr *IPaddr, socklen_t sockaddrlen, struct hwaddr *HWaddr)
{
	int sockfd;
	struct sockaddr_un other_addr;
	int result = -1, n=0;
	char filename[1024];

	socklen_t addrlen;

	char ip_str[INET_ADDRSTRLEN];

	struct sockaddr_in *IPaddr_in = (struct sockaddr_in*) IPaddr;

	printf("ARP request for IP: %s\n", inet_ntop(AF_INET, &(IPaddr_in->sin_addr), ip_str, INET_ADDRSTRLEN));

	// Open a unix datagram socket to the well known ARP file path
	sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);

	memset(&other_addr, 0, sizeof(other_addr));
	other_addr.sun_family = AF_LOCAL;
	strcpy(other_addr.sun_path, WK_AFN);

	addrlen = sizeof(other_addr);
	
	result = connect(sockfd, (struct sockaddr*) &other_addr, addrlen);

	if (result < 0) {
		goto failure;
	}


	// Send out the request for the hw address of that IP address
	n = send(sockfd, IPaddr_in, sizeof(struct sockaddr_in), 0);

	if (n < 0) {
		goto failure;
	}

	// Wait for a reply. Reply populates the HWaddr.
	n = recv(sockfd, HWaddr, sizeof(struct hwaddr), 0);
	
	printf("ARP response for IP: %s is HW: %x:%x:%x:%x:%x:%x on %d index\n", ip_str, HWaddr->sll_addr[0], HWaddr->sll_addr[1], HWaddr->sll_addr[2], HWaddr->sll_addr[3], HWaddr->sll_addr[4], HWaddr->sll_addr[5], HWaddr->sll_ifindex);

	// Close the socket and unlink the file
	close(sockfd);
	unlink(filename);

	// Return
	return 0;

 failure:
	perror("Couldn't communicate with ARP module : ");
	return -1;
}
