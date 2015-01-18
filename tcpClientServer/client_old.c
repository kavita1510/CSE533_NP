#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
           
struct in_addr ipv4addr;
struct in6_addr ipv6addr;
struct hostent *hp;


void getHostNameFromIP( char *addrStr) {
/*
    if (!inet_aton(ipstr, &ip))
	errx(1, "can't parse IP address %s", ipstr);

    if ((hp = gethostbyaddr((const void *)&ip, sizeof ip, AF_INET)) == NULL)
	errx(1, "no name associated with %s", ipstr);

    printf("name associated with %s is %s\n", ipstr, hp->h_name);
*/



/* 
    Check if the entered address is a valid IPv4 or IPv6 address and
    accordingly call the following
*/

    if (inet_pton(AF_INET, addrStr, &ipv4addr)) {
        hp = gethostbyaddr(&ipv4addr, sizeof ipv4addr, AF_INET);
    }
    else {
	inet_pton(AF_INET6, addrStr, &ipv6addr);
	hp = gethostbyaddr(&ipv6addr, sizeof ipv6addr, AF_INET6);
    }
    if(hp != NULL)
        printf("Host name: %s\n", hp->h_name);

}

void getHostIPFromName( char* addrStr ) {

    hp = gethostbyname(addrStr);  
    if ( !hp ) { //Report lookup failure  
    fprintf(stderr,  "%s: host '%s'\n", hstrerror(h_errno), addrStr);  
    }
    else {
    // Figure out a way to print this
   // printf("Address %s\n", inet_ntoa( *(struct in_addr*) ( hp->h_addr_list[0])));

    } 


}


int main (int argc , char *argv[]) {

/*
    if(argc <2)  {
	printf("Please provide a hostname");         
        exit(1);
    }     
*/  

  pid_t childpid, pid;
  int stat;

/*
 * Check the user input if IP address then
 * print the hostname else vice versa
 */
    char *addrStr = "127.0.0.1";
    getHostNameFromIP(addrStr);

    addrStr = "localhost";
    getHostIPFromName(addrStr);

/*
    Client enters and infinite loop and queries 
    user for the service it requires. For eg, echo/time.
    The client then forks off a child. After the child is
    forked off, the parent process enters a second loop
    in which it continually reads and prints out status 
    messages received from the child via a half-duplex pipe
*/


  printf("forking child\n");

  if ( (childpid = fork()) == 0)  {

      if ( (execlp("xterm", "xterm", "-e", "./test", "127.0.0.1", (char *) 0)) < 0)  {
            exit(1);
         }
	else {
            printf("%s", "Hi!! How may I help you?");
	    
	    sleep(10);
	}
     }

  pid = wait(&stat);  
  printf("child terminated\n");
}    

