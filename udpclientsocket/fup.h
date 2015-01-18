#ifndef UNP_H
#define UNP_H

/**
 * The header file for the File Upload Protocol.
 */


#include "unp.h"
#include <assert.h>

#define PAYLOAD_SIZE 384

/*
 * FUP packet types.
 * FUP_TYPE_TX: A general (non-handshake packet) packet for file transfer.
 * FUP_TYPE_FH: A packet containing the first handshake.
 * FUP_TYPE_SH: A packet containing the second handshake.
 * FUP_TYPE_TH: A packet containing the third handshake.
 * FUP_TYPE_ACK: An acknowledgement packet.
 */

#define FUP_TYPE_TX 1
#define FUP_TYPE_FH 2
#define FUP_TYPE_SH 3
#define FUP_TYPE_TH 4
#define FUP_TYPE_ACK 5
#define FUP_TYPE_FIN 6

// The FUP header. Each datagram packet sent out will have this at the start of it.
typedef struct fup_header_t {
	uint32_t type;
	uint32_t seq;
	uint32_t ack;
	uint32_t adv_window; 
} fup_header_t;

/*
 * The packet has a header and a fixed length payload.
 * The payload should ALWAYS be in ASCII character format, no matter what it is sending.
 */
typedef struct fup_packet_t {
	fup_header_t fup_header;
	uint8_t fup_payload[PAYLOAD_SIZE];
} fup_packet_t;

// For unconnected sockets
int sendto_fup_packet(int, fup_packet_t*, struct sockaddr*, socklen_t);
int recvfrom_fup_packet(int, fup_packet_t*, struct sockaddr*, socklen_t*);

// For connected sockets
int send_fup_packet(int, fup_packet_t*);
int recv_fup_packet(int, fup_packet_t*);
int peek_fup_packet(int, fup_packet_t*);

void populate_fup_header(fup_packet_t*, uint32_t, uint32_t, uint32_t, uint32_t);
void populate_fup_payload(fup_packet_t*, uint8_t*);

/* Utility functions. */
void log_addr(struct sockaddr_in*);
void ntoh_addr(struct sockaddr_in*);

#define LOG_SEND(...) fprintf(stderr, __VA_ARGS__);		
#define LOG_RECV(...)  fprintf(stderr, __VA_ARGS__);		
#define LOG_ERROR(...)  fprintf(stderr, __VA_ARGS__);
#define LOG_MESG(...)  fprintf(stderr, __VA_ARGS__);

#ifdef FUP_LOG
#define LOG(...) fprintf(stderr, "%s : ", __func__); fprintf(stderr, __VA_ARGS__);		
#else
#define LOG(...) ;
#endif

#endif
