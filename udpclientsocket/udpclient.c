/* UDP Client code */
#include "udpclient.h"
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

#define CLI_PARAM_MAX_LEN 30

client_opts_t *client_opts;
int flag_msg_dont_route;
sliding_window_t sliding_window;
struct sockaddr_in servaddr;/* AF_INET */
struct sockaddr_in cliaddr;/* AF_INET */

pthread_t consumer_thrd;  // The consumer thread
pthread_attr_t attr;
pthread_mutex_t sw_mutex; // The mutex for the sliding window.
pthread_mutex_t ftd_flag_mutex; // The mutex for the flag.
int file_transfer_done;
struct interfaces_t *interfaces[MAX_INTERFACES];
static sigjmp_buf jmpbuf;

int connfd = -1;

/**
 * The do_consume thread will consume a packet.
 * It will also send out a duplicate ack of the last packet it received. 
 * The sending of duplicate acks is spread out over three different if-blocks. 
 * Probably, not the best way to do this.
 */

void* do_consume(void* p)
{
	FILE* fp = NULL;
	float sleep_time = 0.0;
	sock_buffer_t* buffer = NULL;
	fp = fopen("out.txt", "w");
	int fd = -1;

	fd = open("out.txt", O_WRONLY);
	while(!file_transfer_done) {
		sleep_time = (-1*log(drand48())*client_opts->rate_packet_read);
		usleep(sleep_time*1000);
		pthread_mutex_lock(&sw_mutex);
		while (sw_can_consume(&sliding_window)) {
			ack_sw_remove(&sliding_window, &buffer);
			LOG_MESG("Consumer thread read a packet with seq no %d\n", buffer->fup_packet->fup_header.seq);
			sw_print(&sliding_window);
			write(fd, (char*)buffer->fup_packet->fup_payload, PAYLOAD_SIZE);
			free(buffer->fup_packet);
			buffer->fup_packet = NULL;
			free(buffer);
			buffer = NULL;
		}
		pthread_mutex_unlock(&sw_mutex);
	}
	close(fd);
	LOG_MESG("Consumer thread read all data.\n");
	exit(0);
	return NULL;
}

int main(int argc, char* argv[])
{
        //char filename[20] = "test.c";
        int connfd = -1;
        //int res;
        int sockfd;
        //uint32_t drop_result = -1;
        void* data = NULL;

        socklen_t servlen = sizeof(servaddr);
        socklen_t clilen = sizeof(cliaddr);

        /* Reading input parameters from client.in file */
        client_opts = (struct client_opts_t*)malloc(sizeof(client_opts_t));
        read_client_options(client_opts);

        /* Find interfaces to thent and determine if the server is local to client. */
        get_interfaces(interfaces);

        inet_pton(AF_INET, client_opts->server_ip, &servaddr.sin_addr);
        memset(&cliaddr, 0, clilen);

        flag_msg_dont_route = check_local_to_client();
        printf(" Client IP Address: %s\n", inet_ntoa(cliaddr.sin_addr));
        printf(" Server IP Address: %s\n", inet_ntoa(servaddr.sin_addr));

        sockfd = bind_client(cliaddr, flag_msg_dont_route);
        if (get_sock_addr(sockfd) < 0)
            printf("Getsockname failed!!");

        pthread_mutex_init(&sw_mutex, NULL);
        pthread_create(&consumer_thrd, NULL, do_consume, NULL);


        sw_init(&sliding_window, client_opts->sliding_window_size);
///     connfd = setup("test.c", "127.0.0.1", 9017);
        LOG("Filename is %s", client_opts->filename);
        connfd = setup(client_opts->filename, client_opts->server_ip, client_opts->port);
        do_file_download(connfd);

        pthread_join(consumer_thrd, data);
        return 0;
}


/* Create a UDP socket and bind on IPClient with 0 as port*/
int bind_client(struct sockaddr_in cliaddr, int flag_msg_dont_route) {

        int sockfd = -1;
        socklen_t clilen;
	int on = 1;
        clilen = sizeof(cliaddr);
        cliaddr.sin_family = AF_INET;
        cliaddr.sin_port = htons(0);

        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(flag_msg_dont_route)
		Setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on));

        if (bind(sockfd, (struct sockaddr *)&cliaddr, clilen) < 0) {
                perror("bind failed");
                return -1;
        }
        else
                printf("\nClient bound to ephemeral port\n");
        return sockfd;
}

/* Returns the IP and port on which the client has been bound */
int get_sock_addr(int sockfd) {

        struct sockaddr_in cliaddr1;/* AF_INET */
        socklen_t clilen = sizeof(cliaddr1);
        char buf[64]; int res = 0;
        memset(&cliaddr, 0, sizeof(cliaddr1));
        res = getsockname(sockfd, (struct sockaddr *)&cliaddr1, &clilen);
        if ( res == -1 ) {
                return -1; /* Failed */
        }

        /* Convert address into a string */
        snprintf(buf,sizeof(buf), "%s:%u",
                 inet_ntoa(cliaddr1.sin_addr),
                 (unsigned)ntohs(cliaddr1.sin_port));
        printf("\nGetsockname retrieved IPClient as %s\n", buf);
        return res;
}

int get_peer_addr(int sockfd) {

        struct sockaddr_in servaddr;/* AF_INET */
        socklen_t servlen = sizeof(servaddr);
        char buf[64]; int res = 0;
        res = getpeername(sockfd, (struct sockaddr *)&servaddr, &servlen);
        if ( res == -1 ) {
                return -1; /* Failed */
        }

        /* Convert address into a string */
        snprintf(buf,sizeof(buf), "%s:%u",
                 inet_ntoa(servaddr.sin_addr),
                 (unsigned)ntohs(servaddr.sin_port));
        printf("\nGetpeername retrieved IPserver as %s\n", buf);
        return res;
}

int check_local_to_client() {

    int i = 0;
    unsigned long cli_s_addr, serv_s_addr, net_mask, subnet_s_addr;
    unsigned long max = interfaces[i]->net_mask->sin_addr.s_addr;
    int index = 0, local_flag = 0;
    serv_s_addr = htonl(servaddr.sin_addr.s_addr);

    for(i = 0; i< MAX_INTERFACES; i++) {
        if(interfaces[i] != NULL) {

            cli_s_addr = htonl(interfaces[i]->ip_addr->sin_addr.s_addr);
            net_mask = htonl(interfaces[i]->net_mask->sin_addr.s_addr);
            subnet_s_addr = htonl(interfaces[i]->subnet_addr->sin_addr.s_addr);

            if(cli_s_addr == serv_s_addr) {
                // Server is on the same host as the client
                printf("\nClient and server are on the same host\n");
                inet_pton(AF_INET, "127.0.0.1", &cliaddr.sin_addr);
                inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);
                return 1;
            }

            if((net_mask & serv_s_addr) == subnet_s_addr) {
                // Server is local to client.
                local_flag = 1;
                if(max < net_mask) {
                    max = net_mask;
                    index = i;
                }
            }
        }
    }

    if(local_flag == 1) {
        printf("\nServer is local to the client\n");
        cliaddr.sin_addr = interfaces[index]->ip_addr->sin_addr;
        return 1;
    }
    else {
    // neither local nor on same host
        printf("\nServer is neither local nor on same host as the client\n");
        if(interfaces[1]!=NULL)
            cliaddr.sin_addr.s_addr = interfaces[1]->ip_addr->sin_addr.s_addr;
        else
            cliaddr.sin_addr.s_addr = interfaces[0]->ip_addr->sin_addr.s_addr;
        return 0;
    }
}


/* Read input options from client.in file */
void read_client_options(client_opts_t* client_opts)
{
	FILE * fp;
	char lines[10][CLI_PARAM_MAX_LEN];
	int i = 0;
	int line_len = 0;

	fp = fopen("client.in", "r");
	if (fp == NULL)
		exit(EXIT_FAILURE);

	while (fgets(lines[i], CLI_PARAM_MAX_LEN, fp) != NULL) {
		// Remove newline at end
		line_len = strlen(lines[i]);
		if (lines[i][line_len-1] == '\n') {
			lines[i][line_len-1] = '\0';
		}
		i++;
   	}
	fclose(fp);

	client_opts->server_ip = malloc(sizeof(char *)*strlen(lines[0]));	
	strcpy(client_opts->server_ip, lines[0]);
	client_opts->port = atoi(lines[1]);
	client_opts->filename = malloc(sizeof(char *)*strlen(lines[2]));
	strcpy(client_opts->filename, lines[2]);
	client_opts->sliding_window_size = atoi(lines[3]);
	client_opts->seed = atoi(lines[4]);
	client_opts->prob_packet_drop = atof(lines[5]);
	client_opts->rate_packet_read = atof(lines[6]);
}


void handle_timeout(int signo)
{
	siglongjmp(jmpbuf, 1);
}

/*
 * Just keep reading from the connection fd.
 * Till done.
 */
void do_file_download (int conn_fd)
{
	fup_packet_t* fup_packet_send = NULL;
	fup_packet_t* fup_packet_recv = NULL;
	sock_buffer_t* socket_buffer = NULL;
	int n = -1;
	int next_packet_expected = 1;

	fup_packet_send = (fup_packet_t*) malloc (sizeof(fup_packet_t));
	fup_packet_recv = (fup_packet_t *) malloc (sizeof(fup_packet_t));
	
	n = lossy_recv_fup_packet(conn_fd, fup_packet_recv);
	while (n>0) {
		// GIANT LOCK START
		pthread_mutex_lock(&sw_mutex);

		if (!sw_exists_by_seq_num(&sliding_window, fup_packet_recv->fup_header.seq)
		    && fup_packet_recv->fup_header.seq >= next_packet_expected) {
			socket_buffer = (sock_buffer_t *)malloc(sizeof(sock_buffer_t));
			socket_buffer->fup_packet = fup_packet_recv;
			sw_add(&sliding_window, socket_buffer);
			sw_print(&sliding_window);
		}
		next_packet_expected = sw_next_expected_seq_num(&sliding_window);
		if (fup_packet_recv->fup_header.type == FUP_TYPE_FIN) {
			file_transfer_done = 1;
			break; // file transfer done.
		}
		memset(fup_packet_send, 0, sizeof(fup_packet_t));
		populate_fup_header(fup_packet_send, FUP_TYPE_ACK, 
				    -1, next_packet_expected, sw_get_inorder_window_size(&sliding_window));
		populate_fup_payload(fup_packet_send, (uint8_t *)"\0");
		lossy_send_fup_packet(conn_fd, fup_packet_send);

		fup_packet_recv = (fup_packet_t *) malloc (sizeof(fup_packet_t));
		memset(fup_packet_recv, 0, sizeof(fup_packet_t));
		pthread_mutex_unlock(&sw_mutex);
		// GIANT LOCK END
		n = lossy_recv_fup_packet(conn_fd, fup_packet_recv);
	}
	
	// Send an ACK, don't hang around for reply
	populate_fup_header(fup_packet_send, FUP_TYPE_ACK, -1, -1, -1);
	lossy_send_fup_packet(conn_fd, fup_packet_send);
	file_transfer_done = 1;
	free(fup_packet_send);
	fup_packet_send = NULL;
	printf("Client sent out a FIN and is exiting.\n");
	return;
}

int setup(char* filename, char* serv_ip, int port)
{
	struct sockaddr_in servaddr;
	int result = 0;
	int sockfd = -1;

	socklen_t servlen;

	fup_packet_t* first_handshake_pkt = NULL;
	fup_packet_t* second_handshake_pkt = NULL;
	fup_packet_t* third_handshake_pkt = NULL;

	// The client backs up the first handshake by timer.
	struct itimerval first_handshake_timer;

	uint8_t handshake_complete = 0;

	servlen = sizeof(servaddr);
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	inet_pton(AF_INET, serv_ip, &servaddr.sin_addr);

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	first_handshake_pkt = (fup_packet_t *)malloc(sizeof(fup_packet_t));

	populate_fup_header(first_handshake_pkt, FUP_TYPE_FH, 0, 0, client_opts->sliding_window_size);
	populate_fup_payload(first_handshake_pkt, (uint8_t*)client_opts->filename);
	
	result = connect(sockfd, (SA*) &servaddr, servlen);
	if (result < 0)
		goto conn_error;
	get_peer_addr(sockfd);

	first_handshake_timer.it_interval.tv_sec = 0;
	first_handshake_timer.it_interval.tv_usec = 0;
	first_handshake_timer.it_value.tv_sec = 2;
	first_handshake_timer.it_value.tv_usec = 0;

 retry:
	signal(SIGALRM, handle_timeout);

	// Send the first handshake and back it up with a timer
	result = lossy_send_fup_packet(sockfd, first_handshake_pkt);
	if (result < 0)
		goto send_error;

	result = setitimer(ITIMER_REAL, &first_handshake_timer, NULL);
	if (result < 0)
		goto ft_error;

	if (sigsetjmp(jmpbuf, 1) != 0) {
		// Timeout happened. So let's retransmit again.
		LOG("Attempting to retransmit first handshake.\n");
		result = lossy_send_fup_packet(sockfd, first_handshake_pkt);
		if (result < 0)
			goto ftexp_error;
		goto retry;
	}

	// Wait for server to send the second handshake.
	second_handshake_pkt = (fup_packet_t *)malloc(sizeof(fup_packet_t));
	
	// Try to receive the second handshake
	do {
		memset(second_handshake_pkt, 0, sizeof(fup_packet_t));
		recv_fup_packet(sockfd, second_handshake_pkt);

		if (second_handshake_pkt->fup_header.type == FUP_TYPE_SH) {
			// Received the second handshake.
			// Turn off the timer.
			first_handshake_timer.it_value.tv_sec = 0;
			result = setitimer(ITIMER_REAL, &first_handshake_timer, NULL);
			if (result < 0)
				goto st_error;

			// Send the third handshake by reconnecting.
			third_handshake_pkt = (fup_packet_t *)malloc(sizeof(fup_packet_t));
			populate_fup_header(third_handshake_pkt, FUP_TYPE_TH, 0,0,0);
			servaddr.sin_port = atoi((char *)second_handshake_pkt->fup_payload);
			ntoh_addr(&servaddr);

			result = connect(sockfd, (SA*) &servaddr, servlen);
			if (result < 0)
				goto conn2_error;
			populate_fup_payload(third_handshake_pkt, (uint8_t*)"Ready to begin transmission");
			lossy_send_fup_packet(sockfd, third_handshake_pkt);
		}
	} while (second_handshake_pkt->fup_header.type != FUP_TYPE_SH);
	
	// Wait for the file transfer to begin and deal with retransmitted Second handshakes along the way.
	do {
		memset(second_handshake_pkt, 0, sizeof(fup_packet_t));
		peek_fup_packet(sockfd, second_handshake_pkt);
		
		if (second_handshake_pkt->fup_header.type == FUP_TYPE_SH) {
			recv_fup_packet(sockfd, second_handshake_pkt);
			send_fup_packet(sockfd, third_handshake_pkt);
		} else if (second_handshake_pkt->fup_header.type == FUP_TYPE_TX) {
			// File transfer started
			handshake_complete = 1;
		}

	} while(!handshake_complete);
	return sockfd;

 conn_error:
	perror("Connection error: ");
	return -1;
 send_error:
	perror("Send error: ");
	return -1;
 ft_error:
	perror("First timer setup error: ");
	return -1;
 ftexp_error:
	perror("First timer expiry rror: ");
	return -1;
 st_error:
	perror("Second timer setup error: ");
	return -1;
 conn2_error:
	perror("Second connection error: ");
	return -1;
}

/*
 * Returns 1 if the next packet should be dropped.
 * Returns 0 if the next packet should not be dropped.
 */
int drop_probability_test() {

	srand48(client_opts->seed++);
	float random_no = drand48();

	if(random_no > client_opts->prob_packet_drop)
		return 0;	    // Dont drop the packet as drop prob is less than random no
	else 
		return 1;	    // Drop the packet as prob of drop is higher than random no

}

int lossy_send_fup_packet(int sockfd, fup_packet_t* fup_packet)
{
	int n = 0;
	if(!drop_probability_test()){
		n = send_fup_packet(sockfd, fup_packet);
	} else {
		LOG_MESG("Sent packet dropped\n");
	}
	return n;

}

int lossy_recv_fup_packet(int sockfd, fup_packet_t* fup_packet)
{
	int n = 0;
	while(drop_probability_test()){
		memset(fup_packet, 0, sizeof(fup_packet_t));
		n = recv_fup_packet(sockfd, fup_packet);
		LOG_MESG("Received packet dropped\n");
		/* if (fup_packet->fup_header.type == FUP_TYPE_FIN) { */
		/* 	return -1; */
		/* } */
	}
	n = recv_fup_packet(sockfd, fup_packet);
	if (fup_packet->fup_header.type == FUP_TYPE_FIN) {
		return -1;
	}
	return n;
}


int ack_sw_remove(sliding_window_t *swt, sock_buffer_t **buff) 
{
	int result;
	fup_packet_t* fup_packet_send = NULL;
	int send_dup_ack = 0;
	// Check if removing a packet will open up the window. If it does, then
	// send a duplicate ack.
	if (sw_blocked(swt)) {
		send_dup_ack = 1;
	}	
	result = sw_remove(swt, buff);
	if (send_dup_ack) {
		fup_packet_send = (fup_packet_t*)malloc(sizeof(fup_packet_t));
		memset(fup_packet_send, 0, sizeof(fup_packet_t));

		populate_fup_header(fup_packet_send, 
				    FUP_TYPE_ACK, 
				    -1, 
				    sw_next_expected_seq_num(&sliding_window), 
				    sw_get_inorder_window_size(&sliding_window));
		populate_fup_payload(fup_packet_send, (uint8_t *)"Duplicate ACK");
		send_fup_packet(connfd, fup_packet_send);
		free(fup_packet_send);
	}
	return result;
}

int safe_sw_add(sliding_window_t *swt, sock_buffer_t *buff) 
{
	int result;
	pthread_mutex_lock(&sw_mutex);
	result = sw_add(swt, buff);
	pthread_mutex_unlock(&sw_mutex);
	return result;
}

int safe_sw_blocked(sliding_window_t *swt) 
{
	int result;
	pthread_mutex_lock(&sw_mutex);
	result = sw_blocked(swt);
	pthread_mutex_unlock(&sw_mutex);
	return result;
}

uint32_t safe_sw_next_expected_seq_num(sliding_window_t *swt)
{
	int result;
	pthread_mutex_lock(&sw_mutex);
	result = sw_next_expected_seq_num(swt);
	pthread_mutex_unlock(&sw_mutex);
	return result;
}

int safe_sw_get_inorder_window_size(sliding_window_t *swt) 
{
	int result;
	pthread_mutex_lock(&sw_mutex);
	result = sw_get_inorder_window_size(swt);
	pthread_mutex_unlock(&sw_mutex);
	return result;
}

int safe_sw_exists_by_seq_num(sliding_window_t *swt, int seq_num) 
{
	int result;
	pthread_mutex_lock(&sw_mutex);
	result = sw_exists_by_seq_num(swt, seq_num);
	pthread_mutex_unlock(&sw_mutex);
	return result;
}

int safe_sw_can_consume(sliding_window_t *swt)
{
	int result;
	pthread_mutex_lock(&sw_mutex);
	result = sw_can_consume(swt);
	pthread_mutex_unlock(&sw_mutex);
	return result;
}
