#include "api.h"
#include "unp.h"
#include <setjmp.h>

static sigjmp_buf jmpbuf;

int fd_flag = 0;
int no_of_retries = 5;
int retries;
void handle_timeout(int signo)
{
	printf("Timeout! Trying to force discover route.\n");
	siglongjmp(jmpbuf, 1);
}

int main(void)
{
	int sockfd;
	struct sockaddr_un cliaddr;
	int result = -1, n=0;
	char buf[1024];
	struct hostent *h;
	struct in_addr ip_addr;
        char filename[1024];

	char src_addr[IP_ADDR_LEN];
	int src_port = 0;

	sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0);
	
	memset(&cliaddr, 0, sizeof(cliaddr));
	cliaddr.sun_family = AF_LOCAL;
	tmpnam(filename);
	strcpy(cliaddr.sun_path, filename);
	printf("file name = %s\n", filename);
	result = bind(sockfd, (SA*) &cliaddr, sizeof(cliaddr));
	signal(SIGALRM, handle_timeout);

	if (result < 0)
		goto error;


	while(1) {
                
                done_retrying: 
		memset(buf, 0, sizeof(buf));
		// Read something from the user
		printf("Enter the machine to contact: ");
		scanf("%s", buf);
		if (strlen(buf) > 0) {
			h = gethostbyname(buf);
			strcpy(buf, inet_ntoa(*((struct in_addr *)h->h_addr)));
		retry:

			n = msg_send(sockfd, buf, 13, "", fd_flag);
			fd_flag = 0;
			if (n < 0) {
				goto error;
			}

			alarm(1);
			if (sigsetjmp(jmpbuf, 1) != 0) {
				fd_flag = 1;
                                if(retries < no_of_retries) {
                                    retries +=1;
                                }
                                else {
                                    printf("Max retries reached.");
                                    goto done_retrying;
                                }
				goto retry;
			}

			n = msg_recv(sockfd, buf, src_addr, &src_port);
			alarm(0);
			inet_pton(AF_INET, src_addr, &ip_addr);

			h = gethostbyaddr(&ip_addr, sizeof(ip_addr), AF_INET);

			printf("Client received %s from %s\n", buf, h->h_name);
			if (n < 0) {
				goto error;
			}
		} else {
			break;
		}
	}
	unlink(filename);
	return 0;

 error:
	perror("Error happened: ");
	return -1;
}
