#include "unp.h"
#include "api.h"

/**
 * The API which the client and server uses to talk to the ODR.
 */

int msg_send(int wfd, char* dest_addr, int dest_port, char* msg, int flag)
{
	api_mesg_t* api_mesg = NULL;
	struct sockaddr_un odr_addr;
	socklen_t addrlen;

	int n = 0;

	// Well known ODR "port"

	memset(&odr_addr, 0, sizeof(odr_addr));
	odr_addr.sun_family = AF_LOCAL;
	strcpy(odr_addr.sun_path, WK_OFN);

	addrlen = sizeof(odr_addr);

	api_mesg = malloc(sizeof(api_mesg_t));
	memset(api_mesg, 0, sizeof(api_mesg_t));
	strncpy(api_mesg->dst_addr, dest_addr, IP_ADDR_LEN);
	api_mesg->dst_port = dest_port;
	strncpy(api_mesg->msg, msg, API_MSG_LEN);
	api_mesg->force_disc_flag = flag;
	n = sendto(wfd, api_mesg, sizeof(api_mesg_t), 0, (SA*)&odr_addr, addrlen);
	return n;
}

int msg_recv(int rfd, char* msg, char* src_addr, int* src_port)
{
	api_mesg_t* api_mesg = NULL;
	struct sockaddr_un other_addr;
	int n = 0;
	socklen_t addrlen;

	addrlen = sizeof(struct sockaddr_un);
	memset(&other_addr, 0, sizeof(struct sockaddr_un));

	api_mesg = malloc(sizeof(api_mesg_t));
	n = recvfrom(rfd, api_mesg, sizeof(api_mesg_t), 0, (struct sockaddr*)&other_addr, &addrlen);
	if (n < 0) {
		return n;
	}

	// The user might not be interested in listening to what the other end had to say, just the
	// fact that the other end contacted them.
	if (msg) {
		strncpy(msg, api_mesg->msg, API_MSG_LEN);
	}
	
	strncpy(src_addr, api_mesg->src_addr, IP_ADDR_LEN);
	*src_port = api_mesg->src_port;
	return n;
}
