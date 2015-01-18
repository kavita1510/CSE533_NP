#ifndef SERVER_SLIDING_WINDOW_H
#define SERVER_SLIDING_WINDOW_H

/**
 * The header file for all function prototypes and structure declarations for the sliding window mechanism on the server's end.
 * The server's sliding window never adds anything out of order. 
 * But it removes packets from anywhere in the queue, as and when the corresponding acks come back.
 */

#include "fup.h"

typedef struct sw_entry_t {
	int sb_present;
	struct sock_buffer_t* socket_buffer;
} sw_entry_t;

typedef struct sock_buffer_t 
{
	fup_packet_t* fup_packet;  // Pointer to the actual packet
	uint32_t num_retxmts;      // Number of times retransmitted.
	uint32_t num_acks;         // Number of times this has been acked. 
	uint64_t timestamp;        // Timestamp
} sock_buffer_t;

/*
 * The sliding window is built on top of the send/receive buffer.
 * The first and last pointers give the send/receive queue.
 */
typedef struct sliding_window_t {
	uint32_t queue_size;                   // Max size of the sliding window
	uint32_t queue_count;                  // The used capacity of the sliding window
	sw_entry_t* __dynamic_array;           // The internal dynamic array that will be malloced.
	sw_entry_t* head;
	sw_entry_t* tail;
} sliding_window_t;

void sw_init(sliding_window_t *, int);
int sw_full(sliding_window_t *);
int sw_empty(sliding_window_t *);
uint32_t sw_get_window_size(sliding_window_t *);

int sw_add(sliding_window_t *, sock_buffer_t *);
int sw_remove_by_ack(sliding_window_t *, int);

sock_buffer_t* sw_oldest_unack_seg(sliding_window_t *);
sock_buffer_t* sw_last_unack_seg(sliding_window_t*);

sock_buffer_t* sw_get_by_seq_num(sliding_window_t *, int);

int sw_print(sliding_window_t *);
#endif
