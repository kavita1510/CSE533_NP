 /* UDP server code */
 
 #include "udpserver.h"
 #include "unistd.h"
 #include <time.h>
 #include <setjmp.h>
 
 extern sock_queue_t socket_q;
 // The server configuration. Will be read from a file.
 server_opts_t server_opts;
 struct interfaces_t *interfaces[MAX_INTERFACES];
 static struct rtt_info_plus rttinfo;
 static int rttinit = 0;
 
uint32_t c_win;
uint32_t ss_thres;

void handle_sigchild(int signo)
{
 	LOG_MESG("Child died.\n");
 	while (waitpid(-1, NULL, WNOHANG) > 0);
}
 
void read_server_options(server_opts_t* server_opts)
{
 	FILE * fp;
 	char lines[10][30];
 	int i = 0;
 
 	fp = fopen("server.in", "r");
 	if (fp == NULL)
 		exit(EXIT_FAILURE);
 
 	while (fgets(lines[i], 30, fp) != NULL) {
 		//printf("Retrieved line is %s", lines[i], strlen(lines[i]));
 		i++;
 	}
 	fclose(fp);
 	server_opts->port = atoi(lines[0]);
 	server_opts->sliding_window_size = atoi(lines[1]);
 	LOG("Server: Port number = %d\n", server_opts->port);
 	LOG("Server: sliding window size = %d\n", server_opts->sliding_window_size);
 
}
 
int check_local_to_server(unsigned long serv_address, struct sockaddr_in *cliaddr) {  
   
    int i = 0;
    unsigned long cli_s_addr, net_mask, subnet_s_addr;
    int flag = 0;
    serv_address = htonl(serv_address);
    cli_s_addr = htonl(cliaddr->sin_addr.s_addr);
    
    for(i = 0; i< MAX_INTERFACES; i++) {
        if(interfaces[i] != NULL) {
        //interface_addr = htonl(interfaces[i]->ip_addr->sin_addr.s_addr);
        net_mask = htonl(interfaces[i]->net_mask->sin_addr.s_addr);
        subnet_s_addr = htonl(interfaces[i]->subnet_addr->sin_addr.s_addr);


	if(serv_address == cli_s_addr || (cli_s_addr & net_mask) == subnet_s_addr) {
		    printf("\n Client is local to server\n");flag = 1;
		    break;
		}
	}
    }
    if(flag == 0)
        printf("\n Client is not local to the server.\n");
    return 0;
}

int bind_on_all_interfaces(struct interfaces_t **interfaces) {

	int sockfd = -1;
	int result = -1;
	struct sockaddr_in *sa;
	int i = 0, on = 1;
    
	for(i = 0;i<MAX_INTERFACES; i++)
		{
			if(interfaces[i] != NULL) {
				sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
				Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
				sa = interfaces[i]->ip_addr;
				sa->sin_family = AF_INET;
				sa->sin_port = htons(server_opts.port);
				result = bind(sockfd, (SA *) sa, sizeof(*sa));
				if (result < 0) {
					fprintf(stderr, "Couldn't bind interface. Exiting.\n");
					exit(-1);
				}
				interfaces[i]->sockfd = sockfd;
			}
		}
	return 0;
}

int main(int argc, char* argv[]) 
{
 
	int in_sockfd = -1;
	//struct sockaddr_in servaddr;
	int result = 0;
	int i = 0;
	int num_interfaces = -1;

	fd_set fd_sset;
	int max_fd = -1;

	init_sock_queue();
 
	read_server_options(&server_opts);
	signal(SIGCHLD, handle_sigchild);
 	
	num_interfaces = get_interfaces(interfaces);
	bind_on_all_interfaces(interfaces);
	
	for (;;) {
		FD_ZERO(&fd_sset);
		for (i = 0; i<num_interfaces; i++) {
			if(interfaces[i]!=NULL) {
				FD_SET(interfaces[i]->sockfd, &fd_sset);
				if (interfaces[i]->sockfd > max_fd) {
					max_fd = interfaces[i]->sockfd;
				}
			}
		}
		result = select(max_fd+1, &fd_sset, NULL, NULL, NULL);
		if (result < 1) {
			if (errno == EINTR)
				continue;
			goto error;
		}
		for (i = 0; i< num_interfaces; i++) { 
			in_sockfd = interfaces[i]->sockfd;
			if (FD_ISSET(in_sockfd, &fd_sset)) {
				result = handle_conn(in_sockfd);
				if (result < 0) 
					goto error;
			}
		}
	}
	return 0;
 
 error:
	perror("Error1 :");
	return -1;
}
 
static sigjmp_buf jmpbuf;

int fd_sign[2];
 
void handle_timeout(int signo)
{
	write(fd_sign[1], "T", strlen("T"));
}
 
int handle_conn(int listenfd)
{
 	int child_pid = 0;
 	char* file_name;
 	struct sockaddr_in* cliaddr;
 	socklen_t clilen;
 	conn_client_t* conn_client; // To store the address of the client connected.
 	int connection_fd = -1;
 	int result = 0;
	uint32_t cli_init_window = 0;
 	fup_packet_t* first_handshake_pkt = NULL;
 	first_handshake_pkt = malloc(sizeof(fup_packet_t));
 
 	file_name = (char *) malloc(sizeof(char)*FILE_NAME_SZ);
 	cliaddr = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in));
 
 	/*
 	 * The setup process from the point of the server is as follows -
 	 * 0. Read from the socket for the file name.
 	 * 1. Fork a child.
 	 * 2. In the child, close all other socket descriptors. 
 	 * 3. Create a socket with an ephemeral port, with same ip addr and bind it.
 	 * 4. a. Send out the information about above created port to the client.
 	 *    b. Start timer to backup the above sent packet.
 	 * 5. Wait for the third handshake. 
 	 */
 
 	memset(cliaddr, 0, sizeof(struct sockaddr_in));
 	clilen = sizeof(struct sockaddr_in);
 
 	result = recvfrom_fup_packet(listenfd, first_handshake_pkt, (struct sockaddr *)cliaddr, &clilen);
 	if (first_handshake_pkt->fup_header.type == FUP_TYPE_FH) {
 		if (!ccq_get_by_client((SA*)cliaddr)) {
 			conn_client = (conn_client_t*) malloc(sizeof(conn_client_t));
 			memset(conn_client, 0, sizeof(conn_client_t));
 			conn_client->cli_addr = (SA*) cliaddr;
 			ccq_add(conn_client);
 			strcpy(file_name, (char*)first_handshake_pkt->fup_payload);
			cli_init_window = first_handshake_pkt->fup_header.adv_window;
 			LOG("SERVER READ: Read file name %s\n", file_name);
 			if (result < 0) 
 				goto error;
 			child_pid = fork();
 
 			if (child_pid == 0) {
 				connection_fd = setup(listenfd, (SA*)cliaddr, &clilen);
 				do_file_upload(connection_fd, file_name, cli_init_window);
 			}
 		} else {
 			LOG("Received duplicate first handshake!\n");
 		} 
 	} else {
 		LOG("The first packet received from this client wasn't a handshake packet.");
 		goto error;
 	}
 	free(first_handshake_pkt);
 	first_handshake_pkt = NULL;
 	free(file_name);
 	file_name = NULL;
 	return 0;
 error:
 	perror("Error2 :");
 	return -1;
 
}
 
void do_file_upload(int conn_fd, char* file_name, uint32_t cli_init_window)
{
 	/*
 	 * The algorithm is as follows -
 	 * I) Repeat the following till the whole file is sent.
 	 *    1. Read the next 512 bytes of the file. 
 	 *    2. Check if we can send another packet. 
 	 *       To do this, get min (own_window, client's advertised window, c_win) @TODO - Do this. Is this right?
 	 *       2.i)   If we can, then, build a FUP packet. Then, build a sock_buffer
 	 *       2.ii)  Add the sock_buffer to the sliding window.
 	 *       2.iii) Send it off.
 	 *       2.iv)  Repeat 2. 
 	 *    3. Wait for a valid acknowledgement. Read it. Update the window according to it. 
 	 *    4. Update the timer if the acknowledgment for the "oldest" segment came in. 
 	 */
 	int fd = -1;
 	uint8_t buffer[PAYLOAD_SIZE];
 	int seq_num = 0;
 	int complete = 0;
 	uint32_t ts = 0;
 	sock_t *socket = sq_get_by_sockfd(conn_fd);
 	sock_buffer_t *old_unack;
	sock_buffer_t *last_unack;
	sock_buffer_t *next_unack;
 	int read_count=0;
	int reset_rtt_flag = 0;
	uint64_t diff_in_ts = 0;
	uint32_t rto_val_ms = 0;
	fd_set  readfds;
	int finack_recv = 0;

 	sigset_t mask;
	sigemptyset (&mask);
	sigaddset (&mask, SIGALRM);

	pipe(fd_sign);
	FD_ZERO(&readfds);
	FD_SET(fd_sign[0],&readfds);
	FD_SET(conn_fd, &readfds);

	struct timeval timeout_tv = {0, 0}; 
	char sign[2];

 	struct itimerval* upload_timer = (struct itimerval*) malloc(sizeof(struct itimerval));
 	struct itimerval* persist_timer = (struct itimerval*) malloc(sizeof(struct itimerval));
	int persist_timer_active = 0;

	/* Initialize rtt structure */
	if (rttinit == 0) {
		rtt_init_plus(&rttinfo);
		rttinit = 1;
	}

	rto_val_ms = rtt_start_plus(&rttinfo);
 	upload_timer->it_interval.tv_sec = 0;
 	upload_timer->it_interval.tv_usec = 0;
 	//upload_timer->it_value.tv_sec = 2;
	upload_timer->it_value.tv_sec = rto_val_ms/1000;
 	upload_timer->it_value.tv_usec = (rto_val_ms%1000)*1000;
 
 	socket->upload_timer = upload_timer; 
 	signal(SIGALRM, handle_timeout);
 
 	//	sigset_t mask;
     
	socket->cli_window = cli_init_window;

	// Initializing the congestion control parameters
	c_win = 1;
	ss_thres = socket->cli_window;

	LOG_MESG("Client's initial advertised window = %u\n", socket->cli_window);
 	if (socket == NULL)
 		goto sock_not_found_error;
 
 	fup_packet_t* fup_packet_recv = NULL;
 	fup_packet_t* fup_packet_fin = NULL;
 
 	sock_buffer_t* sock_buffer = NULL;
 
 	LOG("Beginning file transfer with file name %s.\n", file_name);
 	fd = open(file_name, O_RDONLY);
 	if (fd < 0) 
 		goto file_not_found_error;
 
 	while(!complete) {
		sigprocmask(SIG_UNBLOCK, &mask, NULL);
 		// Sender tries to send, send, send till it can send no more.
 		while (
 		       ((!sw_is_window_blocked(socket))
 			&& (read_count = read(fd, buffer, PAYLOAD_SIZE)) > 0)
 		       ) {
			sigprocmask(SIG_BLOCK, &mask, NULL);
 			LOG("Read file contents: %s\n", buffer);
 			// Build a FUP packet and sock_buffer.
 			sock_buffer = (sock_buffer_t*) malloc (sizeof(sock_buffer_t));
 			sock_buffer->fup_packet = (fup_packet_t*) malloc (sizeof(fup_packet_t));
 			ts = rtt_ts_plus(&rttinfo);
 			populate_fup_header(sock_buffer->fup_packet, FUP_TYPE_TX, ++seq_num, -1, 0);
 			populate_fup_payload(sock_buffer->fup_packet, buffer);
 			memset(buffer, 0, sizeof(buffer));
 			// Build a sock_buffer_t to add to the sliding window.
 			sock_buffer->num_retxmts = 0;
 			sock_buffer->num_acks = 0;
			sock_buffer->timestamp = ts;
 			sw_add(&socket->sliding_window, sock_buffer);
 			// Send it out.
 			send_fup_packet(conn_fd, sock_buffer->fup_packet);
			sw_print(&socket->sliding_window);
 			if ((int32_t)socket->cli_window > 0) {
 				socket->cli_window--;
 			} else {
 				socket->cli_window = 0;
 			}
 			// If there's no timer active for this socket, then activate it.
 			// This will happen only if a packet is going out on this socket for the first time.
 			if (!socket->timer_active) {
				rto_val_ms = rtt_start_plus(&rttinfo);
				upload_timer->it_interval.tv_sec = 0;
				upload_timer->it_interval.tv_usec = 0;
				//upload_timer->it_value.tv_sec = 2;
				upload_timer->it_value.tv_sec = rto_val_ms/1000;
				upload_timer->it_value.tv_usec = (rto_val_ms%1000)*1000;
 				setitimer(ITIMER_REAL, socket->upload_timer, NULL);
 			}
			sigprocmask(SIG_UNBLOCK, &mask, NULL);
 		}
		sigprocmask(SIG_UNBLOCK, &mask, NULL);
 		if (!read_count && sw_empty(&socket->sliding_window)) {
 			LOG("Done reading the file and processing all ACKs\n");
 			complete = 1;
 		}
 		if (!complete) {
 			if (sigsetjmp(jmpbuf, 1) != 0) {
				read(fd_sign[0], sign, sizeof(sign));
				sigprocmask(SIG_BLOCK, &mask, NULL);
				printf("Persist timer active = %d\n", persist_timer_active);
				// Find oldest unacked packet, the sliding window first points there
				printf("Received one ack!\n");
				old_unack = sw_oldest_unack_seg(&socket->sliding_window);
				// Maximum no of retries have been reached so do nothing for this packet
				if (old_unack->num_retxmts > RTT_MAXNREXMT) {
				    
					LOG_MESG("ETIMEDOUT error. Could not reach the client. Max retries reached.\n");
					LOG("Seq no of last unacked packet for which max attempts reach is %d\n", old_unack->fup_packet->fup_header.seq);
					exit(0);
				}
				else if(rtt_timeout_plus(&rttinfo) == 0){
					rto_val_ms = rtt_start_plus(&rttinfo);
					upload_timer->it_interval.tv_usec = 0;
					upload_timer->it_value.tv_sec = rto_val_ms/1000;
					upload_timer->it_value.tv_usec = (rto_val_ms%1000)*1000;
					// Resend the packet
					LOG_MESG("Resending unacknowledged packet with sequence number: %d \n", old_unack->fup_packet->fup_header.seq);
					setitimer(ITIMER_REAL, upload_timer, NULL);
		   
					sw_print(&socket->sliding_window);
					// Increment the number of retransmissions 
					old_unack->num_retxmts +=1;

					send_fup_packet(conn_fd, old_unack->fup_packet);
				}
				if (persist_timer_active) {
					LOG_MESG("Persistence Timer got activated.\n");
					// Resend the last unacked segment to probe the client.
					last_unack = sw_last_unack_seg(&socket->sliding_window);
					send_fup_packet(conn_fd, last_unack->fup_packet);
					persist_timer->it_interval.tv_sec = 0;
					persist_timer->it_interval.tv_usec = 0;
					persist_timer->it_value.tv_sec = 1;
					persist_timer->it_value.tv_usec = 5000000;
					persist_timer_active = 1;
					setitimer(ITIMER_REAL, persist_timer, NULL);

				} else {
					ss_thres = c_win/2;
					if (ss_thres <= 0) {
						ss_thres = 1;
					}
					c_win = 1;
					printf("Updated c_win = %d, ss threshold = %d\n", c_win, ss_thres);
				}
				sigprocmask(SIG_UNBLOCK, &mask, NULL);

 			}
 			// Sender waits for a "valid" ACK to open up the window if we have something to send.
 			while(sw_is_window_blocked(socket) && (read_count>0)) {
 				fup_packet_recv = (fup_packet_t*) malloc (sizeof(fup_packet_t));
				FD_ZERO(&readfds);
				FD_SET(fd_sign[0],&readfds);
				FD_SET(conn_fd, &readfds);
				sigprocmask(SIG_UNBLOCK, &mask, NULL);
				setitimer(ITIMER_REAL, upload_timer, NULL);
				if (select(max(fd_sign[0],conn_fd)+1, &readfds, NULL, NULL, NULL) > 0) {
					if (FD_ISSET(conn_fd, &readfds)) {
						recv_fup_packet(conn_fd, fup_packet_recv);
						sigprocmask(SIG_BLOCK, &mask, NULL);
						sw_print(&socket->sliding_window);
						// Process the ack
						if (fup_packet_recv->fup_header.type == FUP_TYPE_ACK) {
							c_win++;
							printf("Updated c_win = %d, ss threshold = %d\n", c_win, ss_thres);
							/* Update the estimates if it is the ack to the last unacked segment
							   For combined acks, do not update the rtt estimators.  */
							old_unack = sw_get_by_seq_num(&socket->sliding_window, fup_packet_recv->fup_header.ack-1);
							if (!sw_empty(&socket->sliding_window)) {
								if(old_unack != NULL &&
								   fup_packet_recv->fup_header.ack == (old_unack->fup_packet->fup_header.seq + 1)) {
									/* LOG_MESG("Old unack ts %lu\n", old_unack->timestamp);	 */
									/* LOG_MESG("current ts %lu\n", rtt_ts_plus(&rttinfo));	 */
									diff_in_ts = rtt_ts_plus(&rttinfo) - old_unack->timestamp ;
									/* LOG_MESG("Difference in ts is%lu\n", diff_in_ts); */
								}
								rtt_reset_timer_on_ack(upload_timer, diff_in_ts);
							}
							sw_remove_by_ack(&socket->sliding_window, fup_packet_recv->fup_header.ack);
							if (sw_empty(&socket->sliding_window)) {
								cancel_timer(upload_timer);
								socket->timer_active = 0;
							}
							next_unack = sw_get_by_seq_num(&socket->sliding_window, fup_packet_recv->fup_header.ack);
							if (next_unack != NULL) {
								next_unack->num_acks++;
								if (next_unack->num_acks >= 3) {
									LOG_MESG("Doing fast retransmit for sequence number = %d\n", next_unack->fup_packet->fup_header.seq);
									send_fup_packet(conn_fd, next_unack->fup_packet);
								}
							}
							sw_print(&socket->sliding_window);
							//@TODO - What to set to here?
							socket->cli_window = fup_packet_recv->fup_header.adv_window - sw_get_window_size(&socket->sliding_window);
						}
						if (socket->cli_window == 0) {
							persist_timer->it_interval.tv_sec = 0;
							persist_timer->it_interval.tv_usec = 0;
							persist_timer->it_value.tv_sec = 1;
							persist_timer->it_value.tv_usec = 5000000;
							persist_timer_active = 1;
							setitimer(ITIMER_REAL, persist_timer, NULL);
						} else {
							sw_print(&socket->sliding_window);
							if (persist_timer_active && !sw_full(&socket->sliding_window)) {
								persist_timer->it_interval.tv_sec = 0;
								persist_timer->it_interval.tv_usec = 0;
								persist_timer->it_value.tv_sec = 0;
								persist_timer->it_value.tv_usec = 0;
								persist_timer_active = 0;
								setitimer(ITIMER_REAL, persist_timer, NULL);
							}
						}
						free(fup_packet_recv);
						fup_packet_recv = NULL;
						sigprocmask(SIG_UNBLOCK, &mask, NULL);
					} else if (FD_ISSET(fd_sign[0], &readfds)) {
						siglongjmp(jmpbuf, 1);
					}

				}
 			}
			sigprocmask(SIG_UNBLOCK, &mask, NULL);
 			// If we're done reading the file, then we wait for acks.
 			if (read_count == 0) {
 				fup_packet_recv = (fup_packet_t*) malloc (sizeof(fup_packet_t));
				FD_ZERO(&readfds);
				FD_SET(fd_sign[0],&readfds);
				FD_SET(conn_fd, &readfds);

				if (select(max(fd_sign[0],conn_fd)+1, &readfds, NULL, NULL, NULL) > 0) {
					if (FD_ISSET(conn_fd, &readfds)) {
						recv_fup_packet(conn_fd, fup_packet_recv);
						sigprocmask(SIG_BLOCK, &mask, NULL);
						sw_print(&socket->sliding_window);

						if (fup_packet_recv->fup_header.type == FUP_TYPE_ACK) {
							c_win++;
							printf("Updated c_win = %d, ss threshold = %d\n", c_win, ss_thres);

							old_unack = sw_get_by_seq_num(&socket->sliding_window, fup_packet_recv->fup_header.ack-1);
							if(old_unack != NULL &&
							   fup_packet_recv->fup_header.ack == (old_unack->fup_packet->fup_header.seq + 1)) {
								/* LOG_MESG("Old unack ts %lu", old_unack->timestamp);	 */
								/* LOG_MESG("current ts %lu\n", rtt_ts_plus(&rttinfo));	 */
								diff_in_ts = rtt_ts_plus(&rttinfo) - old_unack->timestamp ;
								/* LOG_MESG("Difference in ts is %lu\n", diff_in_ts); */
								reset_rtt_flag = 1;
							}
							//reset_timer_on_ack(upload_timer, fup_packet_recv);
							if(reset_rtt_flag == 1) {
								rtt_reset_timer_on_ack(upload_timer, diff_in_ts);
								reset_rtt_flag = 0;
							}
							sw_remove_by_ack(&socket->sliding_window, fup_packet_recv->fup_header.ack);
							next_unack = sw_get_by_seq_num(&socket->sliding_window, fup_packet_recv->fup_header.ack);
							if (next_unack != NULL) {
								next_unack->num_acks++;
								if (next_unack->num_acks >= 3) {
									LOG_MESG("Doing fast retransmit for sequence number = %d\n", next_unack->fup_packet->fup_header.seq);
									send_fup_packet(conn_fd, next_unack->fup_packet);
								}
							}
							sw_print(&socket->sliding_window);
							socket->cli_window = fup_packet_recv->fup_header.adv_window - sw_get_window_size(&socket->sliding_window);

						}
						free(fup_packet_recv);
						fup_packet_recv = NULL;
						sigprocmask(SIG_UNBLOCK, &mask, NULL);
					} else if (FD_ISSET(fd_sign[0], &readfds)) {
						siglongjmp(jmpbuf, 1);
					}
				}

 			}
			sigprocmask(SIG_UNBLOCK, &mask, NULL);
 		}
 	}
 	cancel_timer(upload_timer);
 	// Send out the FIN type packet.
 	fup_packet_fin = (fup_packet_t*) malloc (sizeof(fup_packet_t));
 	populate_fup_header(fup_packet_fin, FUP_TYPE_FIN, ++seq_num,-1,-1);
 	populate_fup_payload(fup_packet_fin, (uint8_t*)"FINISHED");
 	send_fup_packet(conn_fd, fup_packet_fin);
	sw_print(&socket->sliding_window);
	
	struct timeval tv;

	tv.tv_sec = 30;
	tv.tv_usec = 0;

	fd_set rfds;

	/* Watch stdin (fd 0) to see when it has input. */
	FD_ZERO(&rfds);
	FD_SET(conn_fd, &rfds);

	fup_packet_recv = malloc(sizeof(fup_packet_t));
	// Enter TIME-WAIT state to wait for an ACK for 30 seconds.
	while(!finack_recv) {
		// Receive a packet from the client.
		// If it is an ACK then go away. Else wait for one minute and then go away anyway.
		FD_ZERO(&rfds);
		FD_SET(conn_fd, &rfds);
		if(select(conn_fd+1, &rfds, NULL, NULL, &tv) > 0) {
			recv_fup_packet(conn_fd, fup_packet_recv);
			if (fup_packet_recv->fup_header.type == FUP_TYPE_ACK) {
				break;
			} else {
				send_fup_packet(conn_fd, fup_packet_fin);
			}
		} else {
			break;
		}
	}
 	LOG_MESG("Completed file transfer.\n");
 	return;
 
 sock_not_found_error:
 	perror("Couldn't find socket:");
 	return;
 file_not_found_error:
 	perror("File not found:");
 	return;
}
 
int setup(int listenfd, struct sockaddr* cliaddr, socklen_t* clilen)
{
 	struct sockaddr_in servaddr;
 	struct sockaddr_in cliaddr2;
 	socklen_t servlen;
 	socklen_t clilen2;
 	sock_t* sock;
 
 	int connection_fd = -1;
 	int result = 0;
	char buf[50];  
 	fup_packet_t* second_handshake_pkt = NULL;
 	fup_packet_t* third_handshake_pkt = NULL;
 
 	struct itimerval second_handshake_timer;
 
 	//	sigset_t mask;
 	servlen = sizeof(servaddr);
 	// Child closes other sockets
 	close_other_sockets(listenfd);
 
 
 	// Populates the ip address and port that the listening socket is listening on. 
 	// The port will be overwritten with 0, before binding, to get a ephemeral port.
 	memset(&servaddr, 0, sizeof(servaddr));
 	result = getsockname(listenfd, (SA *)&servaddr, &servlen);
 	 
	snprintf(buf,sizeof(buf), "%s", inet_ntoa(servaddr.sin_addr));  

	// If the server is not on the loopback address then check if client is local or not.
	if(strcmp(buf, "127.0.0.1") != 0)
		check_local_to_server(htonl(servaddr.sin_addr.s_addr), (struct sockaddr_in*)cliaddr);
	else
	    printf("\nServer is on loopback address!!\n");
 	if (result < 0) 
 		goto sockname_error;
 
 	// Bind a socket to an ephemeral port.
 	connection_fd = socket(AF_INET, SOCK_DGRAM, 0);
 
 	// Overwrite port with 0.
 	servaddr.sin_port = htons(0);
 	result = bind(connection_fd, (struct sockaddr *) &servaddr, sizeof(servaddr));
 	if (result < 0) 
 		goto bind_error;
 
 	// Do the second handshake.
 	result = getsockname(connection_fd, (SA *)&servaddr, &servlen);
 	second_handshake_pkt = malloc(sizeof(fup_packet_t));
 	populate_fup_header(second_handshake_pkt, FUP_TYPE_SH, 0, 0,0);
 	snprintf((char*)second_handshake_pkt->fup_payload, PAYLOAD_SIZE, "%d", servaddr.sin_port);
 
 	// Activate the timer
 	// @TODO - What should be the timeout during the handshakes? How many times to retransmit?
 
 	second_handshake_timer.it_interval.tv_sec = 0;
 	second_handshake_timer.it_interval.tv_usec = 0;
 	second_handshake_timer.it_value.tv_sec = 10;
 	second_handshake_timer.it_value.tv_usec = 0;
 
 retry:
 	signal(SIGALRM, handle_timeout);
 	result = sendto_fup_packet(listenfd, second_handshake_pkt, (SA*) cliaddr, *clilen);
 	if (result < 0) 
 		goto error;
 	result = setitimer(ITIMER_REAL, &second_handshake_timer, NULL);
 	if (result < 0)
 		goto error;
 	if (sigsetjmp(jmpbuf, 1) != 0) {
 		// Timeout happened, so need to send from the connection_fd too, as the ACK might have got lost.
 		// @TODO - How many timeouts to allow before going home?
 		LOG("Attempting to retransmit second handshake.\n");
 		result = sendto_fup_packet(connection_fd, second_handshake_pkt, (SA*) cliaddr, *clilen);
 		if (result < 0)
 			goto error;
 		goto retry;
 	}
 
 	// Wait for the client to reconnect on the connection_fd. This means, block on a recv.
 	third_handshake_pkt = malloc(sizeof(fup_packet_t));
 
 	clilen2 = sizeof(cliaddr2);
 	recvfrom_fup_packet(connection_fd, third_handshake_pkt, (SA*) &cliaddr2, &clilen2);
 
 	connect(connection_fd, (SA*) &cliaddr2, clilen2);
 	if (third_handshake_pkt->fup_header.type == FUP_TYPE_TH) {
 		// Turn off the timer
 		second_handshake_timer.it_value.tv_sec = 0;
 		result = setitimer(ITIMER_REAL, &second_handshake_timer, NULL);
 		if (result < 0)
 			goto error;
 
 		// The recv had blocked the SIG_ALRM. Turn it back on.
 		/*
 		  signal(SIGALRM, SIG_IGN);
 		  sigemptyset (&mask);
 		  sigaddset (&mask, SIGALRM);
 		  sigprocmask(SIG_UNBLOCK, &mask, NULL);
 		*/
 		LOG("TCP handshake successfully completed.\n");
 	} else {
 		LOG("Client sent something other than the second handshake. Quitting.");
 		goto error;
 	}
 	free(second_handshake_pkt);
 	second_handshake_pkt = NULL;
 	free(third_handshake_pkt);
 	third_handshake_pkt = NULL;
 
 	sock = (sock_t*) malloc (sizeof(sock_t));
 	memset(sock, 0, sizeof(sock));
 	sock->sock_fd = connection_fd;
 	sock->next = NULL;
 	sw_init(&sock->sliding_window, server_opts.sliding_window_size);
 
 	sq_add(sock);
 
 	return connection_fd;
 
 sockname_error:
 	perror("getsockname returned an error: ");
 	return -1;
 
 bind_error:
 	perror("Bind error: ");
 	return -1;
 error:
 	free(second_handshake_pkt);
 	second_handshake_pkt = NULL;
 	free(third_handshake_pkt);
 	third_handshake_pkt = NULL;
 
 	perror("Error3 :");
 	return -1;
}
 
int teardown(int sockfd)
{
 	return 0;
}
 
void close_other_sockets(int sockfd)
{
 	sock_t* socket;
 	socket = socket_q.first;
 	while(socket != NULL) {
 		if (socket->sock_fd != sockfd) {
 			close(socket->sock_fd);
 		} 
 		socket = socket->next;
 	}
}
 
uint32_t find_min(sliding_window_t* swt, uint32_t cli_adv_win, uint32_t c_win, uint32_t ss_thres)
{
	uint32_t room = 0;
	room = c_win - sw_get_window_size(swt);
	if (room <= cli_adv_win) 
		return room;
	if (cli_adv_win <= room)
		return cli_adv_win;
	return cli_adv_win;
}
/**
 * This routine will take care of if the sender is allowed to send more packets or not.
 * It will take into consideration the sender's own buffer space, the client's advertised window
 * and the congestion in the network.
 *
 * Returns -
 * 0: Window is not blocked
 * 1: Window is blocked.
 */
uint8_t sw_is_window_blocked(sock_t* socket)
{
 	int result = 0;
	int min_window_size = 0;
 	// Check the sender's window size
 	if (sw_full(&socket->sliding_window)) {
 		result = 1;
 	}
 	if (result)
 		return 1;
      
 	if (socket->cli_window <= 0) {
 		result = 1;
 	}

	min_window_size = find_min(&socket->sliding_window, socket->cli_window, c_win, ss_thres);
	if (min_window_size <= 0) {
		result = 1;
	}
 	if (result)
 		return result;
 	return result;
 
}
 
void reset_timer_on_expiry(struct itimerval* upload_timer, sock_buffer_t * oldest_unack_sb)
{
 	upload_timer->it_interval.tv_sec = 0;
 	upload_timer->it_interval.tv_usec = 0;
 	upload_timer->it_value.tv_sec = 10;
 	upload_timer->it_value.tv_usec = 0;
 	setitimer(ITIMER_REAL, upload_timer, NULL);
}
 
void reset_timer_on_ack(struct itimerval* upload_timer, fup_packet_t* ack_packet)
{
 	upload_timer->it_interval.tv_sec = 0;
 	upload_timer->it_interval.tv_usec = 0;
 	upload_timer->it_value.tv_sec = 10;
 	upload_timer->it_value.tv_usec = 0;
 	setitimer(ITIMER_REAL, upload_timer, NULL);
}
 
void cancel_timer(struct itimerval* upload_timer)
{
 	upload_timer->it_interval.tv_sec = 0;
 	upload_timer->it_interval.tv_usec = 0;
 	upload_timer->it_value.tv_sec = 0;
 	upload_timer->it_value.tv_usec = 0;
 	setitimer(ITIMER_REAL, upload_timer, NULL);
}
 
void rtt_reset_timer_on_ack(struct itimerval* upload_timer, uint32_t diff_in_ts)
{
	uint32_t rto_val_ms = 0;
	rtt_stop_plus(&rttinfo, diff_in_ts);
	rto_val_ms = rtt_start_plus(&rttinfo);
	upload_timer->it_interval.tv_sec = 0;
	upload_timer->it_interval.tv_usec = 0;
	upload_timer->it_value.tv_sec = rto_val_ms/1000;
	upload_timer->it_value.tv_usec = (rto_val_ms%1000)*1000;
	setitimer(ITIMER_REAL, upload_timer, NULL);
	LOG_MESG("Updated the timer to value = %d seconds, %d microseconds\n", upload_timer->it_value.tv_sec, upload_timer->it_value.tv_usec);
}
