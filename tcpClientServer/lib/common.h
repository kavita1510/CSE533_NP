#ifndef COMMON_H		//Include guards
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>


#define LISTENQ         1024    /* 2nd argument to listen() */
#define max(a,b)        ((a) > (b) ? (a) : (b))

#define ECHO_PORT		7765
#define TIME_PORT		7766

/* Miscellaneous constants */
#define MAXLINE         4096    /* max text line length */
#define BUFFSIZE        8192    /* buffer size for reads and writes */


typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);
int echoClient(FILE *fp, int sockFd, int cliFd);
ssize_t  readline(int, void *, size_t);
ssize_t  readn(int, void *, size_t);
ssize_t  writen(int, const void *, size_t);

static void printError(int fd, char *msg) {
 
    int n = 0; 
    char *err = strerror(errno);
    char *str_output = malloc( strlen(msg) + strlen(err) + 4);
 
    strcpy(str_output, msg);
    if(errno != 0) {
	strcat(str_output, " : ");
	strcat(str_output, err );
    }
     
    n = write(fd, str_output, strlen(str_output) + 1);
}

#endif
