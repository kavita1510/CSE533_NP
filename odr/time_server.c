#include "api.h"
#include "unp.h"

/**
 * The time server will bind itself to a well known file name and wait from the ODR to send 
 * it further messages.
 * Once it receives it, it'll send out the current time to it in the form of a string.
 */
int main(void)
{
	int sockfd;
	struct sockaddr_un myaddr;
	int result = -1, n=0;

	char msg[API_MSG_LEN];

	char src_addr[IP_ADDR_LEN];
	int src_port = 0;

	time_t ticks;
        struct in_addr ipv4addr;
        struct hostent *h;

	sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0);

	unlink(WK_SFN);

	memset(&myaddr, 0, sizeof(myaddr));
	myaddr.sun_family = AF_LOCAL;
	strcpy(myaddr.sun_path, WK_SFN);
        printf(" Server well known path %s\n", myaddr.sun_path);	
	result = bind(sockfd, (SA*) &myaddr, sizeof(myaddr));
	
	if (result < 0)
		goto error;

	while(1) {
		// Receive a request, send a response
		n = msg_recv(sockfd, NULL, src_addr, &src_port);
		if (n < 0) {
			goto error;
		}

                inet_pton(AF_INET, src_addr, &ipv4addr);
                h = gethostbyaddr(&ipv4addr, sizeof ipv4addr, AF_INET);

		printf("The server read something from %s at port %d!\n", h->h_name, src_port);
		time(&ticks);
		snprintf(msg, API_MSG_LEN, "%.24s\r\n", ctime(&ticks));

		n = msg_send(sockfd, src_addr, src_port, msg, 0);
		if (n < 0) {
			goto error;
		}
	}
	return 0;

 error:
	perror("Error happened: ");
	return -1;
}
