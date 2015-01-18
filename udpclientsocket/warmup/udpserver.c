/**
 * A simple UDP echo server which accepts a connection and forks off a child to handle that connection.
 * Uses recvmsg with iovecs.
 *
 * To simulate a client use - 
 * nc -u <server_ip> <server_port>
 *
 * netcat (nc) works like telnet. The only difference is that netcat supports UDP while telnet is connection-oriented, that is TCP only.
 */

#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>


int main (int argc, char* argv[])
{
  int sockfd;
  struct sockaddr_in servaddr, cliaddr;
  int result = 0, n = 0;
  socklen_t clilen;
  char clistr[INET_ADDRSTRLEN];
  // These are the scatter gather arrays.
  char mesg1[9];
  char mesg2[9];
  char mesg3[9];

  // A static message to send back to the client.
  char static_mesg[] = "hello world!\n";
  
  // IOVECs for the above scatter gather arrays.
  struct iovec vec[3];

  // The struct msghdr which encapsulates all the iovecs and stuff.
  struct msghdr recvmsghdr;

  mesg1[8] = '\0';
  mesg2[8] = '\0';
  mesg3[8] = '\0';

  vec[0].iov_base = mesg1;
  vec[0].iov_len = sizeof(mesg1)-sizeof(char); // leave a char for the NULL character
  vec[1].iov_base = mesg2;
  vec[1].iov_len = sizeof(mesg2)-sizeof(char);
  vec[2].iov_base = mesg3;
  vec[2].iov_len = sizeof(mesg3)-sizeof(char);

  recvmsghdr.msg_name = &cliaddr;
  recvmsghdr.msg_namelen = sizeof(cliaddr);
  recvmsghdr.msg_iov = vec;
  recvmsghdr.msg_iovlen = 3;
  recvmsghdr.msg_control = 0;
  recvmsghdr.msg_controllen = 0;

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  
  // Get a socket
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(13);


  // Bind it
  result = bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
  if (result < 0) {
    goto error;
  }

  // Can't listen on a UDP socket. 
  // Directly do a recvfrom

  clilen = sizeof(cliaddr);
  while(1) {
    /* SIMPLE RECVFROM and SENDTO
    n = recvfrom(sockfd, mesg, 1024, 0, (struct sockaddr *) &cliaddr, &clilen);
    sendto(sockfd, mesg, n, 0, (struct sockaddr *) &cliaddr, clilen);
    */

    // Clear out the buffers.

    mesg1[0] = '\0';
    mesg2[0] = '\0';
    mesg3[0] = '\0';

    // recvmsg and sendmsg
    n = recvmsg(sockfd, &recvmsghdr, 0);
    
    // Let's print out what is populated by the recvmsg
    inet_ntop(AF_INET, &cliaddr.sin_addr, clistr, INET_ADDRSTRLEN);
    printf("The client's IP is : %s\n", clistr);
    printf("The size of the sockaddr is = %d\n", recvmsghdr.msg_namelen); 
    printf("SERVER READ:\n");
    printf("Buffer1 = %s\n", mesg1);
    printf("Buffer2 = %s\n", mesg2);
    printf("Buffer3 = %s\n", mesg3);
    sendto(sockfd, static_mesg, sizeof(static_mesg), 0, (struct sockaddr *) &cliaddr, clilen);
  }
  return 0;

 error:
  fprintf(stderr, "Error happened!\n");
  return -1;
}
