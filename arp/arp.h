#ifndef ARP_H
#define ARP_H

#include "unp.h"
#include "hw_addrs.h"
#include <assert.h>

#define ARP_PROTO 0x6872
#define ARP_ID 0x3381

// Do we really need these?
#define ARP_ETH_HARD_TYPE 1
#define ARP_IP_PROT_TYPE 0x0800
#define ARP_ETH_HARD_SIZE 6
#define ARP_IP_PROT_SIZE 4

#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY 2

void send_eth_frame(int sockfd, unsigned char *buf, int buflen, uint8_t* src_mac, int src_ifindex, uint8_t* dest_mac);

struct sock_if_t;

/*
 * This structure will maintain a list of all eth0 interfaces
 * <IPaddr, HW addr> pairs of a node, along with the alias IP
 * addresses pairs, if any.
 */
typedef struct node_if_t
{
	int if_index;
        char if_ip[IF_NAME];
	unsigned char if_hwaddr[6]; 
	struct node_if_t* next;
} node_if_t;

node_if_t* eth0_list;

void add_node_if(node_if_t* node_if);
void populate_bind_interfaces(void);

typedef struct arp_hdr_t {
	uint16_t arp_id;
	uint16_t htype;
	uint16_t ptype;
	uint8_t hlen;
	uint8_t plen;
	uint16_t opcode;
	uint8_t sender_mac[6];
	uint8_t sender_ip[4];
	uint8_t target_mac[6];
	uint8_t target_ip[4];
} arp_hdr_t;

#define ARP_HDRLEN 28      // ARP header length

struct hwaddr {
	int             sll_ifindex;	 /* Interface number */
	unsigned short  sll_hatype;	 /* Hardware type */
	unsigned char   sll_halen;		 /* Length of address */
	unsigned char   sll_addr[8];	 /* Physical layer address */
};

int areq (struct sockaddr *IPaddr, socklen_t sockaddrlen, struct hwaddr *HWaddr);

struct arp_entry_t;

typedef struct arp_entry_t
{
	char ip_addr[INET_ADDRSTRLEN];
	unsigned char hw_addr[8];
	int sll_if_index;
	int sll_ha_type;
	int client_fd;
	struct arp_entry_t* arp_next;
} arp_entry_t;

arp_entry_t* arp_entry_list;

void add_arp_entry(arp_entry_t* arp_entry);

arp_entry_t* get_arp_entry(char* ip);

int process_arp_query(int u_fd, int pf_fd, arp_entry_t** ret_arp_entry, int* ret_fd);

void send_arp_request(int pf_fd, arp_entry_t* arp_entry);

int process_arp_mesg(int pf_fd);

#endif
