/* 
 * Header file for the udp server
 */

#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include "unpifi_plus.h"
#include "server_sliding_window.h"
#include "rttcal.h"

/*
 * The structure that will encapsulate all the properties of the server. 
 * Well-known port number for server.
 * Maximum sending sliding-window size (in datagram units)
 */
typedef struct server_opts_t
{
	uint32_t port;
	uint32_t sliding_window_size;
} server_opts_t;

#define FILE_NAME_SZ 32

/*
 * Structures and functions to maintain the queue of sockets opened by the server.
 */
struct sock_t;

/*
 * For each server socket, an instance of this type is created.
 */
typedef struct sock_t
{
	int sock_fd;                           // The file descriptor for this socket
	struct sock_t* next;                   // The next socket which is listen.
	sliding_window_t sliding_window;       // The sliding window associated with this socket.
	struct itimerval* upload_timer;        // The timer associated with the oldest unacked segment in this socket. 
	uint8_t timer_active;                  // If the above timer is active or not. Will be used, as far as I know, the first time only.
	uint32_t cli_window;          // Client's window size, as we (server) know it.
} sock_t;

typedef struct sock_queue_t
{
	sock_t* first;       // The first element in the queue.
	sock_t* last;        // The last element in the queue.
} sock_queue_t;

sock_queue_t socket_q;

void init_sock_queue(void) 
{
	socket_q.first = NULL;
	socket_q.last = NULL;
}

void sq_add(sock_t* socket)
{
	socket->next = NULL; 
	if (!socket_q.first) {
		socket_q.first = socket;
		socket_q.last = socket;
	} else {
		socket_q.last->next = socket;
		socket_q.last = socket;
	}
}

// Get the sock_t object given the file descriptor associated by it.
sock_t* sq_get_by_sockfd(int sockfd)
{
	sock_t *socket = NULL;
	if (socket_q.first) {
		socket = socket_q.first;
		while(socket != NULL) {
			if (sockfd == socket->sock_fd) {
				return socket;
			}
		}
	}
	return NULL;
}

/*
 * Returns a socket given the end point address.
 */
/* sock_t* sq_get_by_cliaddr(struct sockaddr* cliaddr) */
/* { */
/* 	sock_t *socket = NULL; */
/* 	struct sockaddr_in* sin_cliaddr = (struct sockaddr_in*)cliaddr; */
/* 	if (!socket_q.first) { */
/* 		socket = socket_q.first; */
/* 		while(socket != NULL) { */
/* 			if (sin_cliaddr->sin_addr.s_addr == socket->cli_addr->sin_addr.s_addr */
/* 			    && sin_cliaddr->sin_port == socket->cli_addr->sin_port) { */
/* 				return socket; */
/* 			} */
/* 		} */
/* 	} */
/* 	return NULL; */
/* } */


/*
 * We need to keep track of all the clients that have already completed the first handshake. 
 */
typedef struct conn_client_t
{
	struct sockaddr* cli_addr;
	struct conn_client_t* next;
} conn_client_t;

typedef struct conn_client_queue_t
{
	conn_client_t* first;       // The first element in the queue.
	conn_client_t* last;        // The last element in the queue.
} conn_client_queue_t;

conn_client_queue_t conn_client_q;

void ccq_init(void)
{
	conn_client_q.first = NULL;
	conn_client_q.last = NULL;
}

void ccq_add(conn_client_t* conn_client) 
{
	conn_client->next = NULL;
	if (!conn_client_q.first) {
		conn_client_q.first = conn_client;
		conn_client_q.last = conn_client;
	} else {
		conn_client_q.last->next = conn_client;
		conn_client_q.last = conn_client;
	}
	
}

void ccq_remove(conn_client_t* conn_client)
{
	// @TODO
}

conn_client_t* ccq_get_by_client(struct sockaddr* cliaddr)
{
	struct sockaddr_in* cliaddr_in = (struct sockaddr_in*) cliaddr;
	struct sockaddr_in* cc_in = NULL;
	conn_client_t* cc_ptr = conn_client_q.first;
	while (cc_ptr != NULL) {
		cc_in = (struct sockaddr_in*) cc_ptr->cli_addr;
		if (cc_in->sin_addr.s_addr == cliaddr_in->sin_addr.s_addr
		    && cc_in->sin_port == cliaddr_in->sin_port) {
			return cc_ptr;
		}
		cc_ptr = cc_ptr->next;
	}
	return NULL;
}

int bind_on_all_interfaces(interfaces_t**);
void read_server_options(server_opts_t*) ;
/*
 * This function just reads the first segment sent by the client - that is, the file name. 
 * Then, it forks off a child server to do all the processing. 
 */
int handle_conn(int);


/*
 * The setup routine. This is the server child that  will complete the 2nd and 3rd handshake for the server.
 * This will get invoked when the 1st handshake from the client reaches the server.
 *
 * Arguments -
 * 1. int listenfd - The listening file descriptor.
 * 2. struct sockaddr* cliaddr - The client's sockaddr structure. handle_conn passes this after reading the first packet sent by the client. 
 * 3. socklen_t* clilen - The length of the above.
 *
 */
int setup(int, struct sockaddr*, socklen_t*);

/*
 * This routine does the actual file upload.
 * Arguments - 
 * 1. int conn_fd - The file descriptor for the connection socket. 
 * 2. char* file_name - The name of the file to be transferred.
 * 3. uint32_t - The client's initial window.
 */
void do_file_upload(int, char* , uint32_t);
/*
 * The teardown routine. @TODO - We still don't know what to do in it. 
 */
int teardown(int);

/* Close other sockets opened, except the one passed as an argument. */
void close_other_sockets(int);

/**
 * This routine will take care of if the sender is allowed to send more packets or not.
 * It will take into consideration the sender's own buffer space, the client's advertised window
 * and the congestion in the network.
 */

uint8_t sw_is_window_blocked(sock_t *);


void reset_timer_on_expiry(struct itimerval*, sock_buffer_t*);

void reset_timer_on_ack(struct itimerval*, fup_packet_t*);
void rtt_reset_timer_on_ack(struct itimerval*, uint32_t);

void cancel_timer(struct itimerval*);
#endif
