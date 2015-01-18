/*
 * Routines to send and receive FUP packets.
 * These routines just put the packets out on the udp sockets. 
 * Any retransmission, etc. is handled on top of these routines.
 * These also deal with all the network-to-host and host-to-network conversions.
 * We send packets out in network format. We store in host format.
 */

#include "fup.h"
#include <string.h>
#include <arpa/inet.h>
#include "udpclient.h"

int sendto_fup_packet(int sockfd, fup_packet_t* fup_packet, struct sockaddr* destaddr, socklen_t destaddrlen) 
{
	int n = 0;

	uint32_t type, seq,  adv_window; //timestamp,
	type = fup_packet->fup_header.type;
	seq = fup_packet->fup_header.seq;
	//	timestamp = fup_packet->fup_header.timestamp;
	adv_window = fup_packet->fup_header.adv_window;

	fup_packet->fup_header.type = htonl(fup_packet->fup_header.type);
	fup_packet->fup_header.seq = htonl(fup_packet->fup_header.seq);
	//	fup_packet->fup_header.timestamp = htonl(fup_packet->fup_header.timestamp);
	fup_packet->fup_header.adv_window = htonl(fup_packet->fup_header.adv_window);
	LOG("Sending out packet of type = %d with payload: %s \n", type, fup_packet->fup_payload);
	n = sendto(sockfd, fup_packet, sizeof(fup_packet_t), 0, destaddr, destaddrlen);

	fup_packet->fup_header.type = type;
	fup_packet->fup_header.seq = seq;
	//	fup_packet->fup_header.timestamp = timestamp;
	fup_packet->fup_header.adv_window = adv_window;

	return n;
}

int recvfrom_fup_packet(int sockfd, fup_packet_t* fup_packet, struct sockaddr* destaddr, socklen_t* destaddrlen) 
{
	int n = 0;
	sigset_t mask;
	sigemptyset (&mask);
	sigaddset (&mask, SIGALRM);

	n = recvfrom(sockfd, fup_packet, sizeof(fup_packet_t), 0, destaddr, destaddrlen);

	//	sigprocmask(SIG_BLOCK, &mask, NULL);

	if (n > 0) {
		fup_packet->fup_header.type = ntohl(fup_packet->fup_header.type);
		fup_packet->fup_header.seq = ntohl(fup_packet->fup_header.seq);
		//		fup_packet->fup_header.timestamp = ntohl(fup_packet->fup_header.timestamp);
		fup_packet->fup_header.adv_window = ntohl(fup_packet->fup_header.adv_window);
		fup_packet->fup_header.ack = ntohl(fup_packet->fup_header.ack);
		LOG("Receiving packet with payload:\n ####################\n%s \n, type %d, seq %d \n####################\n\n", fup_packet->fup_payload, fup_packet->fup_header.type, fup_packet->fup_header.seq);
	}
	return n;
}

int send_fup_packet(int sockfd, fup_packet_t* fup_packet) 
{
	int n = 0;

	uint32_t type, seq, adv_window, ack; //timestamp, 
	type = fup_packet->fup_header.type;
	seq = fup_packet->fup_header.seq;

	adv_window = fup_packet->fup_header.adv_window;
	ack = fup_packet->fup_header.ack;

	fup_packet->fup_header.type = htonl(fup_packet->fup_header.type);
	fup_packet->fup_header.seq = htonl(fup_packet->fup_header.seq);

	fup_packet->fup_header.adv_window = htonl(fup_packet->fup_header.adv_window);
	fup_packet->fup_header.ack = htonl(fup_packet->fup_header.ack);

	/*
	LOG_SEND("Dumping packet contents:\n");
	LOG_SEND("Packet header:\n");
	LOG_SEND("\t\ttype = %u\n", type);
	LOG_SEND("\t\tseq = %u\n", seq);
	LOG_SEND("\t\tack = %u\n", ack);
	LOG_SEND("\t\tadv_window = %u\n", adv_window);
	LOG_SEND("Packet payload:%s\n", fup_packet->fup_payload);

	printf("3\n");
	*/
	if (type == FUP_TYPE_TX) {
		LOG_SEND("Sending packet: Sequence Number: %d\n", seq);
	} else if (type == FUP_TYPE_ACK) {
		LOG_SEND("Sending ack: Acknowledgement Number: %d with advertised window = %d\n", ack, adv_window);
	} else if (type == FUP_TYPE_FIN) {
		LOG_SEND("Sending FIN\n");
	}

	n = send(sockfd, fup_packet, sizeof(fup_packet_t), 0);

	fup_packet->fup_header.type = type;
	fup_packet->fup_header.seq = seq;
	//	fup_packet->fup_header.timestamp = timestamp;
	fup_packet->fup_header.adv_window = adv_window;
	fup_packet->fup_header.ack = ack;

	return n;
}

int recv_fup_packet(int sockfd, fup_packet_t* fup_packet) 
{
	int n = 0;
	n = recv(sockfd, fup_packet, sizeof(fup_packet_t), 0);
	//	sigblock(SIGALRM);
	if (n > 0) {
		fup_packet->fup_header.type = ntohl(fup_packet->fup_header.type);
		fup_packet->fup_header.seq = ntohl(fup_packet->fup_header.seq);
		//		fup_packet->fup_header.timestamp = ntohl(fup_packet->fup_header.timestamp);
		fup_packet->fup_header.adv_window = ntohl(fup_packet->fup_header.adv_window);
		fup_packet->fup_header.ack = ntohl(fup_packet->fup_header.ack);

		if (fup_packet->fup_header.type == FUP_TYPE_TX) {
			LOG_RECV("Receiving packet: Sequence Number: %d\n", fup_packet->fup_header.seq);
		} else if (fup_packet->fup_header.type == FUP_TYPE_ACK) {
			LOG_RECV("Receiving ack: Acknowledgement Number: %d, with advertised window = %d\n", fup_packet->fup_header.ack, fup_packet->fup_header.adv_window);
		} else if (fup_packet->fup_header.type == FUP_TYPE_FIN) {
			LOG_RECV("Receiving FIN\n");
		}

		/*
		LOG_RECV("Dumping packet contents:\n");
		LOG_RECV("Packet header:\n");
		LOG_RECV("\t\ttype = %u\n", fup_packet->fup_header.type);
		LOG_RECV("\t\tseq = %u\n", fup_packet->fup_header.seq);
		LOG_RECV("\t\tack = %u\n", fup_packet->fup_header.ack);
		LOG_RECV("\t\ttimestamp = %u\n", fup_packet->fup_header.timestamp);
		LOG_RECV("\t\tadv_window = %u\n", fup_packet->fup_header.adv_window);
		LOG_RECV("Packet payload:%s\n", fup_packet->fup_payload);
		*/
	} else {
		LOG("recv_fup_packet: Connection closed ");
	}
	return n;
}


int peek_fup_packet(int sockfd, fup_packet_t* fup_packet) 
{
	int n = 0;
	n = recv(sockfd, fup_packet, sizeof(fup_packet_t), MSG_PEEK);

	//	sigblock(SIGALRM);
	if (n > 0) {
		fup_packet->fup_header.type = ntohl(fup_packet->fup_header.type);
		fup_packet->fup_header.seq = ntohl(fup_packet->fup_header.seq);
		//		fup_packet->fup_header.timestamp = ntohl(fup_packet->fup_header.timestamp);
		fup_packet->fup_header.adv_window = ntohl(fup_packet->fup_header.adv_window);
		//		LOG("Receiving packet with payload: %s\n", fup_packet->fup_payload);
	} else {
		LOG("peek_fup_packet: Connection closed ");
	}
	return n;
}

int peekfrom_fup_packet(int sockfd, fup_packet_t* fup_packet, struct sockaddr* destaddr, socklen_t* destaddrlen) 
{
	int n = 0;
	n = recvfrom(sockfd, fup_packet, sizeof(fup_packet_t), MSG_PEEK, destaddr, destaddrlen);

	//	sigblock(SIGALRM);
	if (n > 0) {
		fup_packet->fup_header.type = ntohl(fup_packet->fup_header.type);
		fup_packet->fup_header.seq = ntohl(fup_packet->fup_header.seq);
		//		fup_packet->fup_header.timestamp = ntohl(fup_packet->fup_header.timestamp);
		fup_packet->fup_header.adv_window = ntohl(fup_packet->fup_header.adv_window);
		LOG("Receiving packet with payload: ####################\n%s and type %d####################\n\n", fup_packet->fup_payload, fup_packet->fup_header.type);
	} else {
		LOG("peek_fup_packet: Connection closed ");
	}
	return n;
}

void populate_fup_header(fup_packet_t* fup_packet, uint32_t type, uint32_t seq, uint32_t ack, uint32_t adv_window)
{
	fup_packet->fup_header.type = type;
	fup_packet->fup_header.seq = seq;
	fup_packet->fup_header.ack = ack;
	//	fup_packet->fup_header.timestamp = timestamp;
	fup_packet->fup_header.adv_window = adv_window;
}

void populate_fup_payload(fup_packet_t* fup_packet, uint8_t* buff)
{
	memcpy(fup_packet->fup_payload, buff, PAYLOAD_SIZE);
}

/*
 * Utility functions. 
 */
void log_addr(struct sockaddr_in* addr_in)
{
	char clistr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(addr_in->sin_addr), clistr, INET_ADDRSTRLEN);
	printf("IP : %s, port (assuming host order) : %u, port (assuming network order) : %u\n", clistr, addr_in->sin_port, ntohs(addr_in->sin_port));
}

void ntoh_addr(struct sockaddr_in* addr_in)
{
	char clistr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(addr_in->sin_addr), clistr, INET_ADDRSTRLEN);
	inet_pton(AF_INET, clistr, &(addr_in->sin_addr));
}
