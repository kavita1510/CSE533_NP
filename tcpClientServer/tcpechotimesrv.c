#include "common.h"    
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>

#define MAX_NUM 255

static void* echoServerUtil(void*);
static void* timeServerUtil(void*);


int serverErrorCheck(int listenFd, int option, struct sockaddr_in servAddr) {

    int flag = 0;
   if(setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, (char*)&option, sizeof(int)) < 0) {
	printf("ERROR: Setsockopt failed\n");
	flag = 1;
    }
 
    if (bind(listenFd, (struct sockaddr*) &servAddr, sizeof(servAddr)) < 0) {
	perror("ERROR: Bind error");
	flag = 1;
    }

    if (listen(listenFd, LISTENQ) < 0) {
	perror("ERROR: Listening error");
	flag = 1;
    }
   
   // close(listenFd);
    return flag;
}

static void* echoServerUtil(void* arg) {

    int connFd = * ((int *) arg);
    int n;
    char line[MAXLINE];
    
    pthread_detach(pthread_self());    
    free(arg);

again:
    while((n = readline(connFd, line, MAXLINE)) > 0) {
	    if (writen(connFd, line, n) != n)
		perror("ERROR: Write Error");
    }
    
    if (n < 0 && errno == EINTR)
	goto again;
    else if (n < 0)
	perror("ERROR: Read error");
	if ( n == 0) 
		printf("Client termination: socket read returned with value 0\n");
    close(connFd);
    return NULL;
}

static void* timeServerUtil(void* arg) {

    int connFd = * ((int *) arg); 
    time_t ticks;
    char buff[MAXLINE]; 
    char line[MAXLINE];
    struct timeval tmeVal;
    fd_set rset;
    int nready, n = 0;
   
    pthread_detach(pthread_self());    
    free(arg);

    
    ticks = time(NULL);
    snprintf(buff, sizeof(buff), "%.24s\r\n", ctime(&ticks));
    if (write(connFd, buff, strlen(buff)) != strlen(buff))
	perror("ERROR: Write Error");

    while(1) {
	
	FD_ZERO(&rset);
	FD_SET(connFd, &rset);

	tmeVal.tv_sec = 5;
	tmeVal.tv_usec = 0;
	nready = select(connFd+1, &rset, NULL, NULL, &tmeVal);
	if (nready < 0)
	    perror("ERROR: Select Error");
	if (FD_ISSET(connFd, &rset)) {    /* new client connection */
	again:
	    if ((n =readline(connFd, line, MAXLINE)) == 0) 
		printf("Client termination: socket read returned with value 0\n");
	    if (n < 0 && errno == EINTR)
		goto again;
	    return ;
	}
        ticks = time(NULL);
        snprintf(buff, sizeof(buff), "%.24s\r\n", ctime(&ticks));
	if (write(connFd, buff, strlen(buff)) != strlen(buff))
	    perror("ERROR: Write Error");
    }
    close(connFd);
    return NULL;

}


int
main(int argc, char **argv)
{
    int	listenEchoFd, listenTimeFd, connFd, maxFd, option = 1;
    int	nready, *iptr;
    ssize_t n;
    fd_set  rset, allset;
    socklen_t	cliLen;
    char buf[MAXLINE];

    struct sockaddr_in cliAddr, servAddr1, servAddr2;
    int retVal = 0;
    pthread_t tid;    

    // For 1st connection
    listenEchoFd = socket(AF_INET, SOCK_STREAM, 0);
    
    bzero((char *) &servAddr1, sizeof(servAddr1));
    servAddr1.sin_family = AF_INET;
    servAddr1.sin_port = htons(ECHO_PORT);
    
    servAddr1.sin_addr.s_addr = htonl(INADDR_ANY);
    
    /* Server error check does the bind and listen and respective
       error check
    */

    retVal = serverErrorCheck(listenEchoFd, option, servAddr1);
    if (retVal != 0) {
	exit(EXIT_FAILURE);
    }
   
    // For 2nd Connection 
    listenTimeFd = socket(AF_INET, SOCK_STREAM, 0);
    bzero((char *) &servAddr2, sizeof(servAddr2));
    servAddr2.sin_family = AF_INET;
    servAddr2.sin_port = htons(TIME_PORT);
    
    servAddr2.sin_addr.s_addr = htonl(INADDR_ANY);
   
    retVal = serverErrorCheck(listenTimeFd, option, servAddr2);
    if (retVal != 0) {
	exit(EXIT_FAILURE);
    }

    
    maxFd = listenEchoFd;		/* initialize */
    if (maxFd < listenTimeFd) {
	maxFd = listenTimeFd;
    }
  

    FD_ZERO(&allset);
    FD_SET(listenEchoFd, &allset);
    FD_SET(listenTimeFd, &allset);

    
    for ( ; ; ) {
	
	rset = allset;	    /* structure assignment */
	nready = select(maxFd+1, &rset, NULL, NULL, NULL);

	if (nready < 0)
	    perror("ERROR: Select Error");

	if (FD_ISSET(listenEchoFd, &rset)) {    /* new client connection */
	    printf("new client: %s, port %d\n", inet_ntop(AF_INET, &cliAddr.sin_addr, buf, MAXLINE), ntohs(cliAddr.sin_port));

	    cliLen = sizeof(cliAddr);
	    iptr = malloc(sizeof(int));
	    *iptr = accept(listenEchoFd, (struct sockaddr *) &cliAddr, &cliLen);
	    pthread_create(&tid, NULL, &echoServerUtil, iptr);
	}
	
	if (FD_ISSET(listenTimeFd, &rset)) {
	    printf("new client: %s, port %d\n", inet_ntop(AF_INET, &cliAddr.sin_addr, buf, MAXLINE), ntohs(cliAddr.sin_port));
	    
	    cliLen = sizeof(cliAddr);
	    iptr = malloc(sizeof(int));
	    *iptr = accept(listenTimeFd, (struct sockaddr *) &cliAddr, &cliLen);
	    pthread_create(&tid, NULL, &timeServerUtil, iptr);
	}

    }
    return 0;

}












