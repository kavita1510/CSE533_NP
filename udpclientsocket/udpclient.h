/* 
 * Header file for the udp client.
 */
#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include "unpifi_plus.h" 
#include "client_sliding_window.h"

/* 
 * Structure of client options as read from client.in:-
 * IP address of server (not the hostname).
 * Well-known port number of server.
 * filename to be transferred.
 * Receiving sliding-window size (in datagram units).
 * Random generator seed value.
 * Probability p of datagram loss. This should be a real number in the range [ 0.0 , 1.0 ]  (value 0.0 means no loss occurs; value 1.0 means all datagrams all lost).
 * The mean µ, in milliseconds, for an exponential distribution controlling the rate at which the client reads received datagram payloads from its receive buffer.
 */
typedef struct client_opts_t
{
	char *server_ip;
	uint32_t port;
	char *filename;
	uint32_t sliding_window_size;
	uint32_t seed;
	float prob_packet_drop;
	float rate_packet_read;				
	
} client_opts_t;

/*
 * The setup routine. This will complete the 1st, 2nd and 3rd handshake for the client.
 * 
 * Arguments - File name, Server ip, port number.
 * Returns   - fd for the socket for which connection is established.
 */
int setup(char*, char*, int);

/*
 * The teardown routine. @TODO - We still don't know what to do in it. 
 */
int teardown(int);
int drop_probability_test();
void do_file_download (int);
void read_client_options(client_opts_t*);
int bind_client(struct sockaddr_in, int);
int get_sock_addr(int);
int get_peer_addr(int);
int check_local_to_client();

int lossy_send_fup_packet(int, fup_packet_t*);
int lossy_recv_fup_packet(int, fup_packet_t*);

int ack_sw_remove(sliding_window_t *, sock_buffer_t **);
int safe_sw_add(sliding_window_t *, sock_buffer_t *);

uint32_t safe_sw_next_expected_seq_num(sliding_window_t *);
int safe_sw_get_inorder_window_size(sliding_window_t *);

int safe_sw_blocked(sliding_window_t *);

int safe_sw_exists_by_seq_num(sliding_window_t *, int);
int safe_sw_can_consume(sliding_window_t *);

#endif
