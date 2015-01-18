#include "common.h"
#define MAX_NUM 255



int
echoClient(FILE *fp, int sockFd, int cliFd) {

    /*
     * Read a line from client's stdin and write to the server.
     * Read the line back from the server and output the echoed line
     * on standard output of client.
     */
    
    int maxfdp1, stdineof;
    fd_set rset;
    char buf[MAXLINE];
    int n;
    char *msgStr;

    stdineof = 0;
    FD_ZERO(&rset);
    
    while(1) {
	if (stdineof == 0)
	    FD_SET(fileno(fp), &rset);
	FD_SET(sockFd, &rset);
	maxfdp1 = max(fileno(fp), sockFd) + 1;
	if (select(maxfdp1, &rset, NULL, NULL, NULL) < 0) {
		printError(sockFd, "ERROR: Select error");
	}

	if (FD_ISSET(sockFd, &rset)) {	/* socket is readable */
	    if ( (n = read(sockFd, buf, MAXLINE)) == 0) {
		if (stdineof == 1)
		    return;		/* normal termination */
		else
		    printError(cliFd, "ERROR: Server terminated prematurely");
	    }
	    
	    n = write(fileno(stdout), buf, n);
	    if ( n > 0) {
		msgStr = "STATUS MESSAGE: Message echoed from server";
		write(cliFd, msgStr, strlen(msgStr) + 1);
	    }
	}
    
	// When EOF on stdin is received
	if (FD_ISSET(fileno(fp), &rset)) {  /* input is readable */
	    if ( (n = read(fileno(fp), buf, MAXLINE)) == 0) {
		stdineof = 1;
		shutdown(sockFd, SHUT_WR);	/* send FIN */
		FD_CLR(fileno(fp), &rset);
		continue;
	    }

	    writen(sockFd, buf, n);
	}
    }
    return 0;
}


int 
main(int argc, char **argv) {
    int sockFd, cliFd;
    struct sockaddr_in servAddr;
    int pfd[2];
    
    if(argc < 2) {
		printError(cliFd, "ERROR: Required IP address\n");
		exit(EXIT_FAILURE);
    }
    
    if(argc >= 3)
	cliFd = atoi(argv[2]);
    else
	cliFd = fileno(stdout);
    
    sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFd < 0) {
		printError(cliFd, "ERROR: Opening TCP socket error");
		exit(EXIT_FAILURE);
    }

    /* Initialize socket structure */

    bzero((char *) &servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(ECHO_PORT);
    inet_pton(AF_INET, argv[1], &servAddr.sin_addr);
    
	if (connect(sockFd, (struct sockaddr *)&servAddr, sizeof(servAddr)) == -1) {
		printError(cliFd, "ERROR: Connection not made");
		exit(EXIT_FAILURE);
    }	
    printf("Welcome to echo client!!\n");
    echoClient(stdin, sockFd, cliFd);
    return 0;
} 
