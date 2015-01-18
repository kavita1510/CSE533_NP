
#ifndef ODR_API_H
#define ODR_API_H

#define IP_ADDR_LEN 16 // Maximum numbers of characters in an IP address
#define API_MSG_LEN 32 // should be enough?


typedef struct api_mesg_t
{
	char dst_addr[IP_ADDR_LEN];
	char src_addr[IP_ADDR_LEN];
	int dst_port;
	int src_port;
	char msg[API_MSG_LEN];
	int force_disc_flag;
} api_mesg_t;

int msg_send(int wfd, char* dest_addr, int dest_port, char* msg, int flag);
int msg_recv(int rfd, char* msg, char* src_addr, int* src_port);

#endif
