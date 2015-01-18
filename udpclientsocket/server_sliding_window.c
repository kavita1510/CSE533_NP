#include "server_sliding_window.h"

void sw_init(sliding_window_t *swt, int size)
{
	int travel_count = 0;

	swt->__dynamic_array = (sw_entry_t*)malloc(size*sizeof(sw_entry_t));
	swt->queue_size = size;
	swt->queue_count = 0;
	swt->head = swt->__dynamic_array;
	swt->tail = swt->__dynamic_array;

	while(travel_count++ < swt->queue_size) {
		(swt->__dynamic_array+(travel_count-1))->sb_present = -1;
	}
}

int sw_full(sliding_window_t *swt)
{
	if (swt->queue_count >= swt->queue_size) {
		return 1;
	} else {
		return 0;
	}
}

int sw_empty(sliding_window_t *swt)
{
	if (swt->queue_count == 0) {
		return 1;
	} else {
		return 0;
	}
}

uint32_t sw_get_window_size(sliding_window_t *swt)
{
	return swt->queue_count;
}

int sw_add(sliding_window_t *swt, sock_buffer_t *buff)
{
	if (!sw_full(swt)) {
		swt->tail->socket_buffer = buff;
		swt->tail->sb_present = 1;
		swt->tail++;
		// Wrap around?
		if (swt->tail >= (swt->__dynamic_array + swt->queue_size)) {
			swt->tail = swt->__dynamic_array;
		}
		swt->queue_count++;

		return 0;
	} else {
		LOG("PACKET DROPPED\n");
		return -1;
	}
}

int sw_remove_by_ack(sliding_window_t *swt, int ack_num) 
{
	sw_entry_t* temp = NULL;		
	int travel_count = 0;
	temp = swt->head;
	// Receiving an ack for N, means the client is acknowledging the receipt of 
	// the packet (N-1). This means we can remove all packets before and including the one
	// with sequence number (N-1).
	while (!sw_empty(swt) && travel_count++ < swt->queue_count) {
		if (temp->socket_buffer->fup_packet->fup_header.seq <= (ack_num-1)) {
			assert(temp->sb_present);
			//			printf("Removing packet with sequence number = %d temp->socket_buffer = %p\n", temp->socket_buffer->fup_packet->fup_header.seq, temp->socket_buffer);
			temp->sb_present = 0;
			free(temp->socket_buffer->fup_packet);
			free(temp->socket_buffer);
			swt->head++;
			swt->queue_count--;
			// Wrap around?
			if (swt->head >= (swt->__dynamic_array + swt->queue_size)) {
				swt->head = swt->__dynamic_array;
			}
		}
		temp++;
		// Wrap around?
		if (temp >= (swt->__dynamic_array + swt->queue_size)) {
			temp = swt->__dynamic_array;
		}
	}
	return 0;
}

sock_buffer_t* sw_get_by_seq_num(sliding_window_t *swt, int seq_num)
{
	sw_entry_t* temp = NULL;
	int travel_count = 0;
	temp = swt->head;

	while (!sw_empty(swt) && travel_count++ < swt->queue_size) {

		if (temp->sb_present == 1) {
			if (temp->socket_buffer->fup_packet->fup_header.seq == seq_num) {
				return temp->socket_buffer;
			}
		}
		temp++;
		// Wrap around?
		if (temp >= (swt->__dynamic_array + swt->queue_size)) {
			temp = swt->__dynamic_array;
		}
	}
	return NULL;
}

sock_buffer_t* sw_oldest_unack_seg(sliding_window_t *swt)
{
	assert(!sw_empty(swt));
	return swt->head->socket_buffer;
}

sock_buffer_t* sw_last_unack_seg(sliding_window_t* swt)
{
	sw_entry_t* temp = NULL;
	sw_entry_t* prev = NULL;
	int travel_count = 0;

	temp = swt->head;
	prev = temp;
	while (!sw_empty(swt) && travel_count++ < swt->queue_size) {
		if (temp->sb_present) {
			prev = temp;
		}
		temp++;
		// Wrap around?
		if (temp >= (swt->__dynamic_array + swt->queue_size)) {
			temp = swt->__dynamic_array;
		}
	}
	return prev->socket_buffer;
}

int sw_print(sliding_window_t *swt) 
{
	sw_entry_t* temp = NULL;
	int travel_count = 0;
	printf("Contents of sliding window: [ ");
	temp = swt->head;
	while (travel_count++ < swt->queue_count) {

		if (temp->sb_present != 1) {
			printf("X ");
		} else {
			printf("%d ", temp->socket_buffer->fup_packet->fup_header.seq);
		}
		temp++;
		// Wrap around?
		if (temp >= (swt->__dynamic_array + swt->queue_size)) {
			temp = swt->__dynamic_array;
		}
	}
	while (travel_count++ <= swt->queue_size) {
		printf("? ");
	}
	printf(" ]\n");
	//	printf("__dyn_array = %p, head = %p, tail = %p\n", swt->__dynamic_array, swt->head, swt->tail);
	return 0;
}

/* Make the buffers to test the implementation */
void make_buffer(sock_buffer_t** buff, int seq){

	*buff = (struct sock_buffer_t *)malloc(sizeof(sock_buffer_t));
	printf("Creating buffer at %p\n", *buff);
	(*buff)->fup_packet = malloc(sizeof(fup_packet_t));
	(*buff)->fup_packet->fup_header.seq = seq;
}

int tmain(void)
{
	printf("Test case 1\n");
	sock_buffer_t* buff1 = NULL;
	sock_buffer_t* buff2 = NULL;
	sock_buffer_t* buff3 = NULL;

	sliding_window_t* swt = (sliding_window_t*) malloc (sizeof(sliding_window_t));
	sw_init(swt, 5);
	make_buffer(&buff1, 1);
	sw_add(swt, buff1);
	sw_print(swt);
	make_buffer(&buff2, 2);
	sw_add(swt, buff2);
	sw_print(swt);
	make_buffer(&buff3, 3);
	sw_add(swt, buff3);
	sw_print(swt);

	printf("\nTest case 2\n");
	swt = (sliding_window_t*) malloc (sizeof(sliding_window_t));
	sw_init(swt, 100);
	make_buffer(&buff1, 1);
	sw_add(swt, buff1);
	sw_print(swt);
	make_buffer(&buff1, 2);
	sw_add(swt, buff1);
	sw_print(swt);
	make_buffer(&buff1, 3);
	sw_add(swt, buff1);
	sw_print(swt);
	make_buffer(&buff1, 4);
	sw_add(swt, buff1);
	sw_print(swt);
	make_buffer(&buff1, 5);
	sw_add(swt, buff1);
	sw_print(swt);
	make_buffer(&buff1, 6);
	sw_add(swt, buff1);
	sw_print(swt);
	make_buffer(&buff1, 7);
	sw_add(swt, buff1);
	sw_print(swt);
	make_buffer(&buff1, 8);
	sw_add(swt, buff1);
	sw_print(swt);
	make_buffer(&buff1, 9);
	sw_add(swt, buff1);
	sw_print(swt);
	make_buffer(&buff1, 10);
	sw_add(swt, buff1);
	sw_print(swt);
	make_buffer(&buff1, 11);
	sw_add(swt, buff1);
	sw_print(swt);
	make_buffer(&buff1, 12);
	sw_add(swt, buff1);
	sw_print(swt);
	make_buffer(&buff1, 13);
	sw_add(swt, buff1);
	sw_print(swt);
	make_buffer(&buff1, 14);
	sw_add(swt, buff1);
	sw_print(swt);
	make_buffer(&buff1, 15);
	sw_add(swt, buff1);
	sw_print(swt);

	return 0;
}
