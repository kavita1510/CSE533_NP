#include "client_sliding_window.h"

/*
 * Initializes the sliding window structure.
 */
void sw_init(sliding_window_t *swt, int size) 
{
	sw_entry_t* temp = NULL;
	int travel_count = 0;
	swt->__dynamic_array = (sw_entry_t*)malloc(size*sizeof(sw_entry_t));
	swt->queue_size = size;
	swt->queue_count = 0;
	swt->head = swt->__dynamic_array;
	swt->tail = swt->__dynamic_array;

	// Initialize
	while(travel_count++ < swt->queue_size) {
		(swt->__dynamic_array+(travel_count-1))->sb_present = -1;
	}

	// Say that we are expecting the first packet.
	temp = swt->tail;
	temp->sb_present = 0;
	temp->sb_expected_seq_num = 1;
	swt->queue_count++;
	swt->tail++;
	// Check wrap around -- in case queue is of size 1.
	if (swt->tail >= (swt->__dynamic_array + swt->queue_size)) {
		swt->tail = swt->__dynamic_array;
	}

}


/*
 * Checks if the sliding window is full or not.
 * A window is blocked only if all the sw_entries are Present.
 * If any of the sw_entries is missing, then the advertised window will be counted from there.
 */
int sw_blocked(sliding_window_t *swt) 
{
	sw_entry_t* temp = NULL;
	int count = 0;
	int travel_count = 0;
	temp = swt->head;
	while (travel_count++ < swt->queue_size) {
		if (temp->sb_present == 1) {
			count++;
		} else {
			return 0;
		}
		temp++;
		if (temp >= (swt->__dynamic_array + swt->queue_size)) {
			temp = swt->__dynamic_array;
		}

	}
	if (count == swt->queue_size) {
		printf("Blocked!\n");
		return 1;
	} else {
		return 0;
	}
}

/*
 * Checks if the sliding window is blocked or not.
 */
int sw_full(sliding_window_t *swt) 
{
    if(swt->queue_count < swt->queue_size)
	    return 0;
    else
	    return 1;
}

/*
 * Checks if the sliding window is empty.
 */
int sw_empty(sliding_window_t *swt) 
{
	if (swt->queue_count == 0)
		return 1;
	else
		return 0;
}

/*
 * Please see the comments in client_sliding_window.h for struct sw_entry_t for a little more information.
 *
 * The client's window adds packets in the following way --
 * 1. Iterate over the window to find if there's an empty position for it in between head and tail.
 * 2. If yes, then insert there. If not, then insert after tail.
 */
int sw_add(sliding_window_t *swt, sock_buffer_t *buff) 
{
	sw_entry_t* temp = NULL;
	int last_seq_num = -1;
	int travel_count = 0;
	if (!sw_blocked(swt)) {
		// Check if this packet should go in between head and tail
		if (!sw_empty(swt)) {
			temp = swt->head;
			while(travel_count++ < swt->queue_count) {
				if (temp->sb_expected_seq_num == buff->fup_packet->fup_header.seq) {
					if (temp->sb_present == 0) {
						temp->sb_present = 1;
						temp->socket_buffer = buff;
						return 1;
					} else if (temp->sb_present == 1) {
						// Duplicate packet received. Do nothing, go home.
						LOG_ERROR("PACKET DROPPED1!\n");
						return 0;
					}
				}
				last_seq_num = temp->sb_expected_seq_num;
				temp++;
				// Should we wrap around?
				if (temp >= (swt->__dynamic_array + swt->queue_size)) {
					temp = swt->__dynamic_array;
				}
			}
		}
		// We should insert the new buffer after the end and we should also insert places for the missing packets.
		temp = swt->tail;
		if (!sw_empty(swt)) {
			last_seq_num++;
			while(last_seq_num < buff->fup_packet->fup_header.seq
			      && travel_count++ < swt->queue_size) {
				// Missing packet
				temp->sb_expected_seq_num = last_seq_num;
				assert(temp->sb_present == -1);
				temp->sb_present = 0;
				swt->queue_count++;
				temp++;
				// Wrap around?
				if (temp >= (swt->__dynamic_array + swt->queue_size)) {
					temp = swt->__dynamic_array;
				}
				swt->tail++;
				// Wrap around?
				if (swt->tail >= (swt->__dynamic_array + swt->queue_size)) {
					swt->tail = swt->__dynamic_array;
				}
				last_seq_num++;
			}
			if (travel_count > swt->queue_size) {
				LOG_ERROR("PACKET DROPPED3!\n");
				return 0;
			}
		}
		assert(temp->sb_present == -1);
		temp->sb_present = 1;
		temp->sb_expected_seq_num = buff->fup_packet->fup_header.seq;
		temp->socket_buffer = buff;
		swt->tail++;
		// Should we wrap around?
		if (swt->tail >=  (swt->__dynamic_array + swt->queue_size)) {
			swt->tail = swt->__dynamic_array;
		}
		swt->queue_count++;
		//		printf("Inserting at %p, head = %p, tail = %p \n", temp, swt->head, swt->tail);

		return 0;
	} else {
		return -1;
	}
}

/*
 * The client can consume only in order packets from the head of the queue.
 */
int sw_remove(sliding_window_t *swt, sock_buffer_t **buff) 
{
	sw_entry_t* temp = NULL;
	if (!sw_empty(swt) && sw_can_consume(swt)) {
		temp = swt->head;
		temp->sb_present = -1;
		temp->sb_expected_seq_num = -1;
		*buff = temp->socket_buffer;
		//		printf("%d\n", (*buff)->fup_packet->fup_header.seq);
		swt->queue_count--;
		// Update the head
		swt->head++;
		// Wrap around?
		if (swt->head >= (swt->__dynamic_array + swt->queue_size)) {
			swt->head = swt->__dynamic_array;
		}
		// If we're emptying out the queue, then add a placeholder for the next expected sequence number in the tail.
		if (sw_empty(swt)) {
			//			printf("swt->__dynamic_array = %p, swt->head = %p, swt->tail = %p\n", swt->__dynamic_array, swt->head, swt->tail);
			assert(swt->head == swt->tail);
			assert(swt->tail->sb_present == -1);
			swt->tail->sb_expected_seq_num = (temp->socket_buffer->fup_packet->fup_header.seq+1);
			//			printf("Expected sequence num = %d\n", swt->tail->sb_expected_seq_num);
			swt->tail->sb_present = 0;
			swt->tail++;
			// Wrap around? 
			if (swt->tail >= (swt->__dynamic_array + swt->queue_size)) {
				swt->tail = swt->__dynamic_array;
			}
			swt->queue_count++;

		}
		return 1;
	} else {
		return -1;
	}
	
}


int sw_exists_by_seq_num(sliding_window_t *swt, int seq_num)
{
	sw_entry_t* temp = NULL;
	int travel_count = 0;
	if (!sw_empty(swt)) {
		temp = swt->head;
		while (travel_count++ < swt->queue_size) {
			if ((temp->sb_present == 1) &&
			    temp->socket_buffer->fup_packet->fup_header.seq == seq_num) {
				return 1;
			}
			temp++;
			// Wrap around?
			if (temp >= (swt->__dynamic_array + swt->queue_size)) {
				temp = swt->__dynamic_array;
			}
		}
	}
	return 0;
}


/*
 * Returns the next expected sequence number.
 */
int sw_next_expected_seq_num(sliding_window_t *swt)
{
	sw_entry_t* temp = NULL;
	int last_valid_seq_num = -1;
	int travel_count = 0;
	if (!sw_empty(swt)) {
		temp = swt->head;
		while (travel_count++ < swt->queue_size) {
			//			printf("temp->sb_present = %d\n", temp->sb_present);
			if (temp->sb_present == 0) {
				return temp->sb_expected_seq_num;
			} else if (temp->sb_present == 1) {
				last_valid_seq_num = temp->socket_buffer->fup_packet->fup_header.seq;
			} else {
				break;
			}
			temp++;
			// Wrap around?
			if (temp >= (swt->__dynamic_array + swt->queue_size)) {
				temp = swt->__dynamic_array;
			}
		}

	}
	return last_valid_seq_num+1;
}

/* Prints the sequence numbers of the buffers in sliding window */
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

int sw_can_consume(sliding_window_t *swt) 
{
	if (swt->head->sb_present == 1) {
		return 1;
	} else {
		return 0;
	}
}

int sw_get_inorder_window_size(sliding_window_t *swt)
{
	sw_entry_t* temp = NULL;
	int count = 0;
	int travel_count = 0;
	temp = swt->head;
	while (travel_count++ < swt->queue_size) {
		if (temp->sb_present == 1) {
			count++;
		} else {
			break;
		}
		temp++;
		// Wrap around?
		if (temp >= (swt->__dynamic_array + swt->queue_size)) {
			temp = swt->__dynamic_array;
		}
	}
	assert(count <= swt->queue_count);
	return (swt->queue_size - count);
}

/* Make the buffers to test the implementation */
sock_buffer_t* make_buffer(int seq){

    struct sock_buffer_t *buff;
    buff = (struct sock_buffer_t *)malloc(sizeof(sock_buffer_t));
    buff->fup_packet = malloc(sizeof(fup_packet_t));
    buff->fup_packet->fup_header.seq = seq;

    return buff;
}

int testmain(void)
{
	sock_buffer_t* buff1 = NULL;
	sock_buffer_t* buff2 = NULL;
	sock_buffer_t* buff3 = NULL;
	sock_buffer_t* buff4 = NULL;
	sock_buffer_t* buff5 = NULL;
	sock_buffer_t* buff6 = NULL;
	sock_buffer_t* buff7 = NULL;
	sock_buffer_t* buff8 = NULL;

	sock_buffer_t* buffr1 = NULL;
	sock_buffer_t* buffr2 = NULL;

	sliding_window_t* swt = NULL;

	printf("Test case 1\n");
	swt = (sliding_window_t*) malloc (sizeof(sliding_window_t));
	sw_init(swt, 5);
	buff1 = make_buffer(1);
	sw_add(swt, buff1);
	printf("In order window size = %d\n", sw_get_inorder_window_size(swt));
	printf("Next expected sequence number = %d\n", sw_next_expected_seq_num(swt));
	sw_print(swt);
	buff2 = make_buffer(4);
	sw_add(swt, buff2);
	sw_print(swt);
	buff3 = make_buffer(3);
	sw_add(swt, buff3);
	sw_print(swt);

	printf("\nTest case 2\n");
	swt = (sliding_window_t*) malloc (sizeof(sliding_window_t));
	sw_init(swt, 5);
	buff1 = make_buffer(1);
	sw_add(swt, buff1);
	sw_print(swt);
	buff2 = make_buffer(2);
	sw_add(swt, buff2);
	sw_print(swt);
	buff3 = make_buffer(3);
	sw_add(swt, buff3);
	sw_print(swt);

	printf("\nTest case 3\n");
	swt = (sliding_window_t*) malloc (sizeof(sliding_window_t));
	sw_init(swt, 10);
	buff1 = make_buffer(5);
	sw_add(swt, buff1);
	sw_print(swt);
	buff2 = make_buffer(1);
	sw_add(swt, buff2);
	sw_print(swt);
	buff3 = make_buffer(4);
	sw_add(swt, buff3);
	sw_print(swt);
	buff4 = make_buffer(3);
	sw_add(swt, buff4);
	sw_print(swt);
	buff5 = make_buffer(2);
	sw_add(swt, buff5);
	sw_print(swt);

	printf("\nTest case 4\n");
	swt = (sliding_window_t*) malloc (sizeof(sliding_window_t));
	sw_init(swt, 10);
	buff1 = make_buffer(5);
	sw_add(swt, buff1);
	sw_print(swt);
	buff2 = make_buffer(1);
	sw_add(swt, buff2);
	sw_print(swt);
	buff3 = make_buffer(4);
	sw_add(swt, buff3);
	sw_print(swt);
	buff4 = make_buffer(3);
	sw_add(swt, buff4);
	sw_print(swt);
	buff5 = make_buffer(2);
	sw_add(swt, buff5);
	sw_print(swt);
	
	sw_remove(swt, &buffr1);
	sw_print(swt);
	sw_remove(swt, &buffr2);
	sw_print(swt);

	buff6 = make_buffer(12);
	sw_add(swt, buff6);
	sw_print(swt);

	buff7 = make_buffer(9);
	sw_add(swt, buff7);
	sw_print(swt);

	printf("In order window size = %d\n", sw_get_inorder_window_size(swt));

	buff8 = make_buffer(6);
	sw_add(swt, buff8);
	sw_print(swt);

	printf("In order window size = %d\n", sw_get_inorder_window_size(swt));

	sw_remove(swt, &buffr1);
	sw_print(swt);
	sw_remove(swt, &buffr1);
	sw_print(swt);
	sw_remove(swt, &buffr1);
	sw_print(swt);
	sw_remove(swt, &buffr1);
	sw_print(swt);

	printf("In order window size = %d\n", sw_get_inorder_window_size(swt));
	printf("Next expected sequence number = %d\n", sw_next_expected_seq_num(swt));


	printf("\nTest case 5\n");
	swt = (sliding_window_t*) malloc (sizeof(sliding_window_t));
	sw_init(swt, 3);
	buff1 = make_buffer(1);
	sw_add(swt, buff1);
	sw_print(swt);
	printf("In order window size = %d\n", sw_get_inorder_window_size(swt));
	printf("Next expected sequence number = %d\n", sw_next_expected_seq_num(swt));
	buff2 = make_buffer(2);
	sw_add(swt, buff2);
	sw_print(swt);
	buff3 = make_buffer(3);
	sw_add(swt, buff3);
	sw_print(swt);
	sw_remove(swt, &buffr1);
	sw_print(swt);
	sw_remove(swt, &buffr1);
	sw_print(swt);
	sw_remove(swt, &buffr1);
	sw_print(swt);
	printf("In order window size = %d\n", sw_get_inorder_window_size(swt));
	printf("Next expected sequence number = %d\n", sw_next_expected_seq_num(swt));

	printf("\nTest case 6\n");
	swt = (sliding_window_t*) malloc (sizeof(sliding_window_t));
	sw_init(swt, 3);
	buff1 = make_buffer(1);
	sw_add(swt, buff1);
	printf("In order window size = %d\n", sw_get_inorder_window_size(swt));
	printf("Next expected sequence number = %d\n", sw_next_expected_seq_num(swt));
	sw_print(swt);
	buff2 = make_buffer(4);
	sw_add(swt, buff2);
	sw_print(swt);
	buff3 = make_buffer(3);
	sw_add(swt, buff3);
	sw_print(swt);
	sw_remove(swt, &buffr1);
	sw_print(swt);
	printf("Next expected sequence number = %d\n", sw_next_expected_seq_num(swt));
	sw_remove(swt, &buffr1);
	sw_print(swt);
	buff1 = make_buffer(2);
	sw_add(swt, buff1);
	sw_print(swt);
	sw_remove(swt, &buffr1);
	sw_print(swt);
	sw_remove(swt, &buffr1);
	sw_print(swt);
	printf("Next expected sequence number = %d\n", sw_next_expected_seq_num(swt));
	sw_remove(swt, &buffr1);
	sw_print(swt);
	printf("Next expected sequence number = %d\n", sw_next_expected_seq_num(swt));


	printf("\nTest case 7\n");
	swt = (sliding_window_t*) malloc (sizeof(sliding_window_t));
	sw_init(swt, 1);
	buff1 = make_buffer(1);
	sw_add(swt, buff1);
	sw_print(swt);
	printf("In order window size = %d\n", sw_get_inorder_window_size(swt));
	printf("Next expected sequence number = %d\n", sw_next_expected_seq_num(swt));
	sw_remove(swt, &buffr1);
	sw_print(swt);
	printf("In order window size = %d\n", sw_get_inorder_window_size(swt));
	printf("Next expected sequence number = %d\n", sw_next_expected_seq_num(swt));
	buff1 = make_buffer(2);
	sw_add(swt, buff1);
	sw_print(swt);
	printf("In order window size = %d\n", sw_get_inorder_window_size(swt));
	printf("Next expected sequence number = %d\n", sw_next_expected_seq_num(swt));
	sw_remove(swt, &buffr1);
	sw_print(swt);
	printf("In order window size = %d\n", sw_get_inorder_window_size(swt));
	printf("Next expected sequence number = %d\n", sw_next_expected_seq_num(swt));

	return 0;
}
