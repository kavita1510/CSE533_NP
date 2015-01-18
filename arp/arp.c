/*
 * ARP protocol
 */

#include "arp.h"
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int if_index;
void populate_interfaces(void)
{
	struct hwa_info	*hwa, *hwahead;
	struct sockaddr	*sa;
	char   *ptr;
	int    i, prflag;
	node_if_t* node_if;

	for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {
		if (strcmp(hwa->if_name, "eth0") == 0) {
			printf("%s :%s", hwa->if_name, ((hwa->ip_alias) == IP_ALIAS) ? " (alias)\n" : "\n");

			if ( (sa = hwa->ip_addr) != NULL) {
				printf("         IP addr = %s\n", Sock_ntop_host(sa, sizeof(*sa)));
			}

			prflag = 0;
			i = 0;
			do {
				if (hwa->if_haddr[i] != '\0') {
					prflag = 1;
					break;
				}
			} while (++i < IF_HADDR);

			if (prflag) {
				printf("         HW addr = ");
				ptr = hwa->if_haddr;
				i = IF_HADDR;
				do {
					printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
				} while (--i > 0);
			}

			printf("\n         interface index = %d\n\n", hwa->if_index);

			node_if = malloc(sizeof(node_if_t));
			node_if->if_index = hwa->if_index;
			if_index = hwa->if_index;       // Temperary
			for(i=0; i<6; i++) { 
				node_if->if_hwaddr[i] = hwa->if_haddr[i];
			}
			strncpy(node_if->if_ip, Sock_ntop_host(sa, sizeof(*sa)), IF_NAME);

			add_node_if(node_if);
			
		}
	}

	free_hwa_info(hwahead);
	//printf("The canonical IP for this node is %s\n", canonical_name);
	return;
}

void send_arp_request(int pf_fd, arp_entry_t* arp_entry) 
{
	struct in_addr temp_addr;
	uint8_t broadcast_mac[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	uint8_t my_mac[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Needed cause 8 v/s 6 problem
	// Find out who I am
	if (eth0_list == NULL) {
		fprintf(stderr, "No eth0 interfaces found to send out the ARP request.\n");
		exit(-1);
	}
	
	int i = 0;
	node_if_t eth0_if = *eth0_list;
	
	// Build up the arp packet
	arp_hdr_t arp_pack;
	arp_pack.arp_id = htons(ARP_ID);
	arp_pack.htype = htons(ARP_ETH_HARD_TYPE);
	arp_pack.ptype = htons(ARP_IP_PROT_TYPE);
	arp_pack.hlen = htons(ARP_ETH_HARD_SIZE);
	arp_pack.plen = htons(ARP_IP_PROT_SIZE);
	arp_pack.opcode = htons(ARP_OP_REQUEST);
	
	for (i=0; i<6; i++) {
		arp_pack.sender_mac[i] = eth0_if.if_hwaddr[i];
	}

	//	printf("My ip address is %s\n", eth0_if.if_ip);
	inet_pton(AF_INET, eth0_if.if_ip , &temp_addr);
	long *ip = (long*)arp_pack.sender_ip;
	*ip = temp_addr.s_addr;

	for (i=0; i<6; i++) {
		arp_pack.target_mac[i] = 0x00;
	}

	inet_pton(AF_INET, arp_entry->ip_addr, &arp_pack.target_ip);
	for (i = 0; i<6; i++) {
		my_mac[i] = eth0_if.if_hwaddr[i];
	}
	printf("Sending ARP Request\n");
	printf("ARP ID = %x\n", arp_pack.arp_id);
	printf("ARP HTYPE = %x\n", arp_pack.htype);
	printf("ARP PTYPE = %x\n", arp_pack.ptype);
	printf("ARP HLEN = %x\n", arp_pack.hlen);
	printf("ARP PLEN = %x\n", arp_pack.plen);
	printf("ARP OPCODE = %x\n", arp_pack.opcode);
	printf("ARP SENDER MAC = %x:%x:%x:%x:%x:%x\n", arp_pack.sender_mac[0], arp_pack.sender_mac[1],
	       arp_pack.sender_mac[2], arp_pack.sender_mac[3],
	       arp_pack.sender_mac[4], arp_pack.sender_mac[5]);
	printf("ARP SENDER IP = %ld\n", (long)arp_pack.sender_ip);
	send_eth_frame(pf_fd, (unsigned char*) &arp_pack, sizeof(arp_hdr_t), my_mac, eth0_if.if_index, broadcast_mac);
}

int process_arp_query(int u_fd, int pf_fd, arp_entry_t** ret_arp_entry, int* ret_fd)
{
	// Read what the request.
	int n = 0;
	char ipstr[INET_ADDRSTRLEN];
	struct sockaddr_in req_ipaddr; 
	struct sockaddr_in other_addr;
	socklen_t other_addr_len;
	arp_entry_t* arp_entry = NULL;

	int* sockfd2 = malloc(sizeof(int));

	other_addr_len = sizeof(struct sockaddr_in);
	*sockfd2 = accept(u_fd, (struct sockaddr *) &other_addr, &other_addr_len);


	if (*sockfd2 < 0) {
		goto error;
	}

	n = recv(*sockfd2, &req_ipaddr, sizeof(req_ipaddr), 0);
	if (n < 0) {
		goto error;
	}
	printf("ARP: Received translation request for %s\n", inet_ntop(AF_INET, &(req_ipaddr.sin_addr), ipstr, INET_ADDRSTRLEN));

	arp_entry = get_arp_entry(ipstr);
	if (arp_entry == NULL) {
		// Make an incomplete arp entry in the arp cache.
		arp_entry = malloc(sizeof(arp_entry_t));
		memset(arp_entry, 0, sizeof(arp_entry_t));
		strncpy(arp_entry->ip_addr, ipstr, INET_ADDRSTRLEN);
		arp_entry->client_fd = *sockfd2;
		arp_entry->arp_next = NULL;
		add_arp_entry(arp_entry);

		// Send out a arp request
		send_arp_request(pf_fd, arp_entry);
	} else {
		// Return that entry
		(*ret_arp_entry) = arp_entry;
	}
	*ret_fd = *sockfd2;
	return 0;

 error:
	perror("Receive error : ");
	return -1;
}

void signal_callback_handler(int signum){
	printf("SIGPIPE happened\n");
	// Do nothing.
	// Sigpipe may happen if the client who called us went away.
	// In the case of tour, it can happen if the client sent an ARP request and then quit after receiving the mcast messages.
}

int main() {

	fd_set fd_sset;
	// The wellknown unix domain socket for communicating with areq function of tour module.
	int wk_un_sock = -1; 
	int maxfd = -1;
	int pf_pack_sock = -1;
	struct sockaddr_un wk_addr;
	//socklen_t sock_un_len;
	//struct sockaddr_un app_addr;
	socklen_t sock_len;
	struct sockaddr_ll* sock_addr;
	int result = 0;
	int ret_fd = 0, i = 0;

	struct hwaddr *target_hwaddr = NULL;

	// Populate bind interfaces
	populate_interfaces();

	unlink(WK_AFN);
	signal(SIGPIPE, signal_callback_handler);

	// Create a Unix Domain socket of type SOCK_STREAM, listening socket for
	// bound to a well known sun_path file
	memset(&wk_addr, 0, sizeof(struct sockaddr_un));
	wk_addr.sun_family = AF_LOCAL;
	strncpy(wk_addr.sun_path, WK_AFN, sizeof(WK_AFN) - 1);

	// Bind it.
	wk_un_sock = socket(AF_LOCAL, SOCK_STREAM, 0);
	result = bind(wk_un_sock, (SA*) &wk_addr, sizeof(wk_addr));

	if (result < 0) {
		goto un_error;
	}

	// Listen on it.
	result = listen(wk_un_sock, 1);
    
	if (result < 0) {
		goto un_error;
	}

	// Create a PF_PACKET socket of type SOCK_RAW
	pf_pack_sock = socket(AF_PACKET, SOCK_RAW, htons(ARP_PROTO));
	if (pf_pack_sock < 0) {
		goto error;
	}
	sock_addr = malloc(sizeof(struct sockaddr_ll));
	memset(sock_addr, 0, sizeof(struct sockaddr_ll));
	// The manual says, "Only the sll_protocol and the sll_ifindex address fields are used for purposes of binding", but if you don't specify the sll_family it refuses to bind. 
	sock_addr->sll_family = AF_PACKET;
	sock_addr->sll_protocol = htons(ARP_PROTO);
	sock_addr->sll_ifindex = if_index;
	sock_len = sizeof(struct sockaddr_ll);
	result = bind(pf_pack_sock, (SA*)sock_addr, sock_len);
	if (result < 0) {
		goto error;
	}

	maxfd = pf_pack_sock;

	for (;;) { 
		FD_ZERO(&fd_sset);
		FD_SET(wk_un_sock, &fd_sset);
		FD_SET(pf_pack_sock, &fd_sset);

		result = select(maxfd+1, &fd_sset, NULL, NULL, NULL);
		if (result < 1) {
			if (errno == EINTR)
				continue;
			goto error;
		}
		if (FD_ISSET(wk_un_sock, &fd_sset)) {
			// Read the request
			arp_entry_t* arp_entry = NULL;
			result = process_arp_query(wk_un_sock, pf_pack_sock, &arp_entry, &ret_fd);
			if (result < 0) {
				goto un_error;
			}
			if (arp_entry != NULL) {
				target_hwaddr = malloc(sizeof(struct hwaddr));
				for (i=0; i<6; i++) {
					target_hwaddr->sll_addr[i] = arp_entry->hw_addr[i];
				}
				target_hwaddr->sll_addr[6] = 0x00;
				target_hwaddr->sll_addr[7] = 0x00;
				target_hwaddr->sll_ifindex = arp_entry->sll_if_index;
				send(ret_fd, target_hwaddr, sizeof(struct hwaddr), 0);
				close(ret_fd);
			}
		}
		else if (FD_ISSET(pf_pack_sock, &fd_sset)) {
			result = process_arp_mesg(pf_pack_sock);
		}
	}

	return 0;

 un_error:
	perror("Unix domain socket error:\n");
	return -1;
 error:
	perror("Error happened:\n");
	return -1;

}

void add_node_if(node_if_t* node_if)
{
	node_if_t* temp = eth0_list;
	node_if->next = NULL;
	if (temp ==  NULL) {
		eth0_list = node_if;
		return;
	}
	while(temp && temp->next) {
		temp = temp->next;
	}
	temp->next = node_if;
}

void add_arp_entry(arp_entry_t* arp_entry)
{
	arp_entry_t* temp = NULL;
	arp_entry->arp_next = NULL;
	if (arp_entry_list == NULL) {
		arp_entry_list = arp_entry;
	} else {
		temp = arp_entry_list;
		while (temp->arp_next != NULL) {
			temp = temp->arp_next;
		}
		temp->arp_next = arp_entry;
	}
}

arp_entry_t* get_arp_entry(char* ip)
{
	arp_entry_t* temp = arp_entry_list;
	while (temp != NULL) {
		if (strncmp(temp->ip_addr, ip, INET_ADDRSTRLEN) == 0) {
			return temp;
		} 
		temp = temp->arp_next;
	}
	return NULL;
}

void send_eth_frame(int sockfd, unsigned char *buf, int buflen, uint8_t* src_mac, int src_ifindex, uint8_t* dest_mac)
{
	struct sockaddr_ll dest_addr;
	int result = -1;
	int j = 0;
	unsigned char* eth_buf = NULL;

	unsigned char* ether_head = NULL;
	unsigned char* ether_data = NULL;
	
	/*another pointer to ethernet header*/
	struct ethhdr *eh = NULL;

	eth_buf = malloc(ETH_FRAME_LEN);
	memset(eth_buf, 0, ETH_FRAME_LEN);
	ether_head = eth_buf;
	ether_data = eth_buf + 14;
	eh = (struct ethhdr *)ether_head;

	memset(&dest_addr, 0, sizeof(struct sockaddr_ll));


	dest_addr.sll_family   = PF_PACKET;	
	dest_addr.sll_protocol = htons(ARP_PROTO);	
	dest_addr.sll_ifindex  = src_ifindex;
	dest_addr.sll_hatype   = ARPHRD_ETHER;
	dest_addr.sll_pkttype  = PACKET_OTHERHOST;
	dest_addr.sll_halen    = ETH_ALEN;	
	for (j=0; j<6; j++) {
		dest_addr.sll_addr[j] = dest_mac[j];
	}
	dest_addr.sll_addr[6]  = 0x00;/*not used*/
	dest_addr.sll_addr[7]  = 0x00;/*not used*/
	
	/*set the frame header*/
	memcpy((void*)eth_buf, (void*)dest_mac, ETH_ALEN);
	memcpy((void*)(eth_buf+ETH_ALEN), (void*)src_mac, ETH_ALEN);
	eh->h_proto = htons(ARP_PROTO);
	
	memcpy(ether_data, buf, buflen);
	

	printf("Sending out ethernet frame with src mac : %x:%x:%x:%x:%x:%x:%x:%x and dest mac : %x:%x:%x:%x:%x:%x:%x:%x and proto = %d on src_ifindex = %d\n",
	       src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4], src_mac[5], src_mac[6], src_mac[7],
	       dest_mac[0], dest_mac[1], dest_mac[2], dest_mac[3], dest_mac[4], dest_mac[5], dest_mac[6], dest_mac[7],
	       eh->h_proto, src_ifindex);

	result = sendto(sockfd, eth_buf, ETH_FRAME_LEN, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
	if (result < 0) {
		goto send_error;
	}
	return;
 send_error:
	perror("Send error: ");
	exit(0);

}

int process_arp_mesg(int pf_fd)
{
	char *buf = NULL;
	int i = -1;
	struct sockaddr_ll src_addr;
	socklen_t addrlen;
	
	struct ethhdr *eh = NULL;
	arp_entry_t* arp_entry = NULL;
	arp_hdr_t* arp_mesg = NULL;

	char ipstr[INET_ADDRSTRLEN];

	struct node_if_t* my_aliases = eth0_list;
	int for_me = 0;

	arp_hdr_t arp_pack;

	struct in_addr temp_addr;
	long *ip = NULL;

	struct hwaddr *target_hwaddr = NULL;

	uint8_t sender_mac_cpy[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t target_mac_cpy[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	// Read the message
	buf = malloc(ETH_FRAME_LEN);
	memset(buf, 0, ETH_FRAME_LEN);
	memset(&src_addr, 0, sizeof(struct sockaddr_ll));
	addrlen = sizeof(struct sockaddr_ll);
	recvfrom(pf_fd, buf, ETH_FRAME_LEN, 0, (SA*) &src_addr, &addrlen);
	

	arp_mesg = (arp_hdr_t*)(buf+14);
	eh = (struct ethhdr *) buf;
	
	// Convert everything to host byte order
	arp_mesg->arp_id = ntohs(arp_mesg->arp_id);
	arp_mesg->htype = ntohs(arp_mesg->htype);
	arp_mesg->ptype = ntohs(arp_mesg->ptype);
	arp_mesg->hlen = ntohs(arp_mesg->hlen);
	arp_mesg->plen = ntohs(arp_mesg->plen);
	arp_mesg->opcode = ntohs(arp_mesg->opcode);
	
	eh->h_proto = ntohs(eh->h_proto);
	if (eh->h_proto == ARP_PROTO && arp_mesg->arp_id == ARP_ID) {
		// If it is a ARP request for me, send the reply
		inet_ntop(AF_INET, arp_mesg->target_ip, ipstr, INET_ADDRSTRLEN);
		while (my_aliases != NULL) {
			if ((strncmp(my_aliases->if_ip, ipstr, INET_ADDRSTRLEN)) == 0) {
				for_me = 1;
				break;
			}
			my_aliases = my_aliases->next;
		}
		if (arp_mesg->opcode == ARP_OP_REQUEST) { 
			if (for_me) {
				// Build a ARP reply packet
				arp_pack.arp_id = htons(ARP_ID);
				arp_pack.htype = htons(ARP_ETH_HARD_TYPE);
				arp_pack.ptype = htons(ARP_IP_PROT_TYPE);
				arp_pack.hlen = htons(ARP_ETH_HARD_SIZE);
				arp_pack.plen = htons(ARP_IP_PROT_SIZE);
				arp_pack.opcode = htons(ARP_OP_REPLY);
	
				for (i=0; i<6; i++) {
					arp_pack.sender_mac[i] = my_aliases->if_hwaddr[i];
				}
				inet_pton(AF_INET, my_aliases->if_ip , &temp_addr);
				ip = (long *) arp_pack.sender_ip;
				*ip = temp_addr.s_addr;
				for (i=0; i<6; i++) {
					arp_pack.target_mac[i] = arp_mesg->sender_mac[i];
				}
				ip = (long *) arp_pack.target_ip;
				*ip = *((long *) &(arp_mesg->sender_ip));
				
				// Extend the target and sender mac for 48 bits
				for (i=0; i<6; i++) {
					target_mac_cpy[i] = arp_pack.target_mac[i];
					sender_mac_cpy[i] = arp_pack.sender_mac[i];
				}

				printf("Sending ARP Response\n");
				printf("ARP ID = %x\n", arp_pack.arp_id);
				printf("ARP HTYPE = %x\n", arp_pack.htype);
				printf("ARP PTYPE = %x\n", arp_pack.ptype);
				printf("ARP HLEN = %x\n", arp_pack.hlen);
				printf("ARP PLEN = %x\n", arp_pack.plen);
				printf("ARP OPCODE = %x\n", arp_pack.opcode);
				printf("ARP SENDER MAC = %x:%x:%x:%x:%x:%x\n", arp_pack.sender_mac[0], arp_pack.sender_mac[1],
				       arp_pack.sender_mac[2], arp_pack.sender_mac[3],
				       arp_pack.sender_mac[4], arp_pack.sender_mac[5]);
				printf("ARP SENDER IP = %ld\n", (long)arp_pack.sender_ip);
				printf("ARP TARGET MAC = %x:%x:%x:%x:%x:%x\n", arp_pack.target_mac[0], arp_pack.target_mac[1],
				       arp_pack.target_mac[2], arp_pack.target_mac[3],
				       arp_pack.target_mac[4], arp_pack.target_mac[5]);
				printf("ARP TARGET IP = %ld\n", (long)arp_pack.target_ip);

				send_eth_frame(pf_fd, (unsigned char*) &arp_pack, sizeof(arp_hdr_t), sender_mac_cpy, my_aliases->if_index, target_mac_cpy);
			} else {
				// Update arp route of the sender, if I already have a route
				ip = (long *) arp_mesg->sender_ip;
				temp_addr.s_addr = *ip;
				inet_ntop(AF_INET, &(temp_addr.s_addr), ipstr, INET_ADDRSTRLEN);
				
				arp_entry = get_arp_entry(ipstr);
				if (arp_entry != NULL) {
					// Do the update!
					for (i = 0; i < 6; i++) {
						arp_entry->hw_addr[i] = arp_mesg->sender_mac[i];
					}
					arp_entry->sll_if_index = my_aliases->if_index;
					arp_entry->sll_ha_type = src_addr.sll_hatype;
				}
			}
		} else if (arp_mesg->opcode == ARP_OP_REPLY && for_me) {
			// If it is a ARP reply, add to the arp cache and send out the response to the client.
			// First get the Sender IP address
			ip = (long *) arp_mesg->sender_ip;
			temp_addr.s_addr = *ip;
			inet_ntop(AF_INET, &(temp_addr.s_addr), ipstr, INET_ADDRSTRLEN);
				
			arp_entry = get_arp_entry(ipstr);
			if (arp_entry != NULL) {
				// Get the Sender HW address
				for (i = 0; i < 6; i++) {
					arp_entry->hw_addr[i] = arp_mesg->sender_mac[i];
				}
				arp_entry->sll_if_index = my_aliases->if_index;
				arp_entry->sll_ha_type = src_addr.sll_hatype;
				// Send it out to the client waiting for it.
				if (arp_entry->client_fd > 0) {
					target_hwaddr = malloc(sizeof(struct hwaddr));
					for (i=0; i<6; i++) {
						target_hwaddr->sll_addr[i] = arp_entry->hw_addr[i];
					}
					target_hwaddr->sll_addr[6] = 0x00;
					target_hwaddr->sll_addr[7] = 0x00;
					target_hwaddr->sll_ifindex = arp_entry->sll_if_index;
					send(arp_entry->client_fd, target_hwaddr, sizeof(struct hwaddr), 0);
					close(arp_entry->client_fd);
					/* if (n < 0) { */
					/* 	goto un_error; */
					/* } */
				}
			}
			
		}
	}
	return 0;

}
