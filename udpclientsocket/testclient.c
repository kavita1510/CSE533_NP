/* Test UDP client. */

#include "udpclient.h"
#include <stdlib.h>
#include <setjmp.h>

void do_simple_file_download (int);

int main(int argc, char** argv[])
{
	char filename[20] = "test.c";
	int connfd = -1;
	connfd = setup(filename, "127.0.0.1", 9013);
	do_simple_file_download(connfd);
	return 0;
}

static sigjmp_buf jmpbuf;

void handle_timeout(int signo)
{
	siglongjmp(jmpbuf, 1);
}

/*
 * Just keep reading from the connection fd.
 * Till done.
 */
void do_simple_file_download (int conn_fd)
{
	FILE* fp = NULL;
	fup_packet_t* fup_packet_send = NULL;
	fup_packet_t* fup_packet_recv = NULL;
	int n = -1;

	fup_packet_send = (fup_packet_t*) malloc (sizeof(fup_packet_t));
	fup_packet_recv = (fup_packet_t *) malloc (sizeof(fup_packet_t));

	fp = fopen("test.txt", "w");
	n = recv_fup_packet(conn_fd, fup_packet_recv);
	while (n>0) {
		fprintf(fp, "%s", fup_packet_recv->fup_payload);
		printf("Read a packet.");
		// Send out the ACK
		memset(fup_packet_send, 0, sizeof(fup_packet_t));
		populate_fup_header(fup_packet_send, FUP_TYPE_ACK, 
				    -1, fup_packet_recv->fup_header.seq++, -1, -1);
		populate_fup_payload(fup_packet_send, "\0");
		send_fup_packet(conn_fd, fup_packet_send);

		memset(fup_packet_recv, 0, sizeof(fup_packet_t));
		n = recv_fup_packet(conn_fd, fup_packet_recv);
	}
	fclose(fp);
	free(fup_packet_recv);
	fup_packet_recv = NULL;
	free(fup_packet_send);
	fup_packet_send = NULL;
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

	populate_fup_header(first_handshake_pkt, FUP_TYPE_FH, 0, 0, 0, 0);
	populate_fup_payload(first_handshake_pkt, filename);

	result = connect(sockfd, (SA*) &servaddr, servlen);
	if (result < 0)
		goto error;

	first_handshake_timer.it_interval.tv_sec = 0;
	first_handshake_timer.it_interval.tv_usec = 0;
	first_handshake_timer.it_value.tv_sec = 2;
	first_handshake_timer.it_value.tv_usec = 0;

 retry:
	signal(SIGALRM, handle_timeout);

	// Send the first handshake and back it up with a timer
	result = send_fup_packet(sockfd, first_handshake_pkt);
	if (result < 0)
		goto error;

	result = setitimer(ITIMER_REAL, &first_handshake_timer, NULL);
	if (result < 0)
		goto error;

	if (sigsetjmp(jmpbuf, 1) != 0) {
		// Timeout happened. So let's retransmit again.
		LOG("Attempting to retransmit first handshake.\n");
		result = send_fup_packet(sockfd, first_handshake_pkt);
		if (result < 0)
			goto error;
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
				goto error;

			// Send the third handshake by reconnecting.
			third_handshake_pkt = (fup_packet_t *)malloc(sizeof(fup_packet_t));
			populate_fup_header(third_handshake_pkt, FUP_TYPE_TH, 0,0,0,0);
			servaddr.sin_port = atoi(second_handshake_pkt->fup_payload);

			result = connect(sockfd, (SA*) &servaddr, servlen);
			if (result < 0)
				goto error;
			populate_fup_payload(third_handshake_pkt, "Ready to begin transmission");
			send_fup_packet(sockfd, third_handshake_pkt);
		}
	} while (second_handshake_pkt->fup_header.type != FUP_TYPE_SH);

	LOG("cc1\n");
	// Wait for the file transfer to begin and deal with retransmitted Second handshakes along the way.
	do {
		memset(second_handshake_pkt, 0, sizeof(fup_packet_t));
		peek_fup_packet(sockfd, second_handshake_pkt);
		
		LOG("The received segment is of type %d\n", second_handshake_pkt->fup_header.type);
		if (second_handshake_pkt->fup_header.type == FUP_TYPE_SH) {
			recv_fup_packet(sockfd, second_handshake_pkt);
			send_fup_packet(sockfd, third_handshake_pkt);
		} else if (second_handshake_pkt->fup_header.type == FUP_TYPE_TX) {
			// File transfer started
			handshake_complete = 1;
		}

	} while(!handshake_complete);
	LOG("cc2\n");
	return sockfd;

 error:
	perror("Error: ");
}
