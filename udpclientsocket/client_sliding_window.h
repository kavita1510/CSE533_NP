#ifndef SLIDING_WINDOW_H
#define SLIDING_WINDOW_H

/**
 * The header file for all function prototypes and structure declarations for the client's sliding window mechanism. 
 * The sliding window on the client's end will add packets in order or in out-of-order fashion. 
 * But it always removes packets from the head of the queue.
 *
 * The sliding window is a dynamically allocated queue of sw_entry_t items. Each sw_entry_t item can either point to 
 * a valid socket_buffer or be marked as "not present". 
 *
 * This design allows for missing packets to be marked in the sliding window.
 *
 */

#include "fup.h"

struct sock_buffer_t;

/*
 * The basic idea is that the sliding window contains placeholders for the packets that arrived or we expect to arrive.
 * For example, [1,2,-,-,5,6] will be represented by 6 sw_entry_t entities, with their sb_expected_seq_num values as (1,2,3,4,5,6)
 * and sb_present values as (1,1,0,0,1,1).
 */
typedef struct sw_entry_t {
	int sb_present;                         // -1: Invalid, 0: Expected, but absent, 1: Expected, present
	int sb_expected_seq_num;                // The sequence number of the packet that we expect to live here.
	                                        // Might be -1, if we have no such expectation.
	struct sock_buffer_t* socket_buffer;
} sw_entry_t;


/*
 * The socket_buffer pointer in the sliding window points to these socket buffers if 
 * the sb_present value is 1.
 */
typedef struct sock_buffer_t 
{
	uint32_t timestamp;
	fup_packet_t* fup_packet;  // Pointer to the actual packet
} sock_buffer_t;

/*
 * The sliding window is a queue, the size of which is decided at run time.
 * Important thing to note is that queue_count includes missing (X) packets. 
 * queue_count is ONLY to be used to keep track of internal queue processing, 
 * NOT to see if the window is blocked. 
 */
typedef struct sliding_window_t {
	uint32_t queue_size;   // Max size of the sliding window
	uint32_t queue_count;  // The used capacity of the sliding window
	sw_entry_t* __dynamic_array; // The internal dynamic array that will be malloced.
	sw_entry_t* head;
	sw_entry_t* tail;
	
} sliding_window_t;

void sw_init(sliding_window_t *, int);
int sw_full(sliding_window_t *);
int sw_blocked(sliding_window_t *);
int sw_empty(sliding_window_t *);

int sw_add(sliding_window_t *, sock_buffer_t *);
int sw_remove(sliding_window_t *, sock_buffer_t **);


int sw_next_expected_seq_num(sliding_window_t *);

int sw_can_consume(sliding_window_t *);
int sw_exists_by_seq_num(sliding_window_t *, int);

int sw_get_inorder_window_size(sliding_window_t *);
#endif
