#include "common.h"

static char* ip;
void getHostData( char *addrStr) {

    struct in_addr ipv4addr;
    struct in_addr **addrList;
	struct hostent *hp;

    /*
     * Checks if the enetered address is a valid IP address,
     * if yes then prints the corresponding domain name
     * else checks if its a hostname and prints corresponding
     * IP address.
     */

    if (inet_pton(AF_INET, addrStr, &ipv4addr)) {
	hp = gethostbyaddr(&ipv4addr, sizeof ipv4addr, AF_INET);
    
	if(hp != NULL) {
	    printf("Host name: %s\n", hp->h_name);
		ip = addrStr;
	}
	else {
	    printf("No name found corresponding to this IP address!!\n");
	    exit(EXIT_FAILURE);
	}

    }
    else
    {
	hp = gethostbyname(addrStr);  
    
	if ( hp == (struct hostent *)NULL ) { //Report lookup failure  
	    printf("No IP found corresponding to this domain name!!\n");
	    exit(EXIT_FAILURE);
	}
	else {
	    addrList = (struct in_addr **)(hp->h_addr_list);
	    ip = inet_ntoa(*addrList[0]);
	    printf("IP Address: %s\n", ip);
	}
    }
}

int
main(int argc, char **argv) {

    int sockFd, status;
    struct sockaddr_in servAddr;
    pid_t childpid, pid;
    int stat, serviceNo;
    char *addrStr = NULL, *serviceProg = NULL, pfdStr[10];
    int pfd[2];
    char msgFromChild[1000];

    if(argc != 2) {
	perror("Usage: TCP client <IPAddress/Host Name>");
	exit(EXIT_FAILURE);
    }
    
    /*
     * Retrieves the host information corresponding to the
     * entered address.
     */
    
    getHostData(argv[1]);

    /*
     * Client enters and infinite loop and queries 
     * user for the service it requires. For eg, echo/time.
     * The client then forks off a child. After the child is
     * forked off, the parent process enters a second loop
     * in which it continually reads and prints out status 
     * messages received from the child via a half-duplex pipe
     */

    while(1){

	printf("\n Welcome!! Which service would you like to use (1-3)?");
	printf("\n 1) Echo");
	printf("\n 2) Time");
	printf("\n 3) Quit");
	printf("\n Service No.:");
	// Receive the service no input
	scanf("%d", &serviceNo);
	if(serviceNo !=1 || serviceNo !=2 || serviceNo !=3) {
		printf("Invalid Input\n");
		exit(EXIT_FAILURE);
	}

	switch (serviceNo) {
	
	case 1: serviceProg = "./echo_cli"; break;
	case 2: serviceProg = "./time_cli"; break;
	case 3: 
    	    printf("You asked to quit Bbye!!\n");
	    exit(EXIT_SUCCESS);
	}

	if (pipe(pfd) == -1) {
		perror("Pipe failed");
	    exit(EXIT_FAILURE);
	}

	if ((childpid = fork()) < 0) {
	    perror("Fork failed");
	    exit(EXIT_FAILURE);
	}
	
	sprintf(pfdStr, "%d", pfd[1]);

	//printf("Would connect to this IP %s\n", ip);
	
	if (childpid == 0) {	// Child Process
		
		close(pfd[0]);		    // Close the read end of the pipe
	    if (execlp("xterm", "xterm", "-e", serviceProg, ip, pfdStr, (char *) 0) < 0) {

			perror("Xterm failed");
			exit(EXIT_FAILURE);
	    }
        exit(EXIT_SUCCESS);	
	}
	else {	// In parent
	    close(pfd[1]);

		// Relaying back status messages from client
	
		while(read(pfd[0], msgFromChild, 1000) >0)
		printf("%s\n",msgFromChild);	
	    
		close(pfd[0]);
	}
	
	waitpid(childpid, &status, 0);

	if (WIFEXITED(status) == 0) {
	    printf("Child did not terminate properly");
	    close(pfd[0]);
	}
	// TODO:: Modify the following for error message
	else if ( (status = WEXITSTATUS(status)) == 0)
	    printf("Child terminated properly");
	else
	    perror("Child exited with error");

    }

    return 0;
}




