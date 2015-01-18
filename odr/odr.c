/*
 * The ODR protocol
 */

#include "odr.h"
#include "hw_addrs.h"

#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char* canonical_name;
int broadcast_id;
char odr_vm_name[IP_ADDR_LEN];
int staleness_value = 0; 

int ephemeral_port_value = 10000;

seen_rrex_t* seen_rreq_list;

void get_vm_name(char* addr, char *result)
{
        struct in_addr ipv4addr;
        struct hostent *h;
        inet_pton(AF_INET, addr, &ipv4addr);
        h = gethostbyaddr(&ipv4addr, sizeof ipv4addr, AF_INET);
        strncpy(result, h->h_name, IP_ADDR_LEN);
}

void populate_bind_interfaces(void)
{
	struct hwa_info	*hwa, *hwahead;
	struct sockaddr	*sa;
	char   *ptr;
	int    i, prflag, result;
	int sockfd;
	struct sockaddr_ll* sock_addr;
	socklen_t sock_len; 
	sock_if_t* sock_if;

	for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {
		if (strcmp(hwa->if_name, "eth0") == 0) {
			canonical_name = malloc(IP_ADDR_LEN);
			strncpy(canonical_name, Sock_ntop_host(hwa->ip_addr, sizeof(hwa->ip_addr)), IP_ADDR_LEN);
			canonical_name[IP_ADDR_LEN-1] = '\0';
		} else if (strcmp(hwa->if_name, "lo") == 0) {
			continue;
		} else {	
			printf("%s :%s", hwa->if_name, ((hwa->ip_alias) == IP_ALIAS) ? " (alias)\n" : "\n");
		
			if ( (sa = hwa->ip_addr) != NULL)
				printf("         IP addr = %s\n", Sock_ntop_host(sa, sizeof(*sa)));
				
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

			// Create the socket and bind it to the interface.
			sockfd = socket(AF_PACKET, SOCK_RAW, htons(ODR_PROTO));
			if (sockfd < 0) {
				goto error;
			}
			sock_addr = malloc(sizeof(struct sockaddr_ll));
			memset(sock_addr, 0, sizeof(struct sockaddr_ll));
			// The manual says, "Only the sll_protocol and the sll_ifindex address fields are used for purposes of binding", but if you don't specify the sll_family it refuses to bind. 
			sock_addr->sll_family = AF_PACKET;
			sock_addr->sll_protocol = htons(ODR_PROTO);
			sock_addr->sll_ifindex = hwa->if_index; // bind to this interface
			sock_len = sizeof(struct sockaddr_ll);
			result = bind(sockfd, (SA*)sock_addr, sock_len);
			if (result < 0) {
				goto error;
			}
			sock_if = malloc(sizeof(sock_if_t));
			sock_if->sock_fd = sockfd;
			sock_if->if_index = hwa->if_index;
			// Unsmart way to copy!
			sock_if->if_hwaddr[0] = hwa->if_haddr[0];
			sock_if->if_hwaddr[1] = hwa->if_haddr[1];
			sock_if->if_hwaddr[2] = hwa->if_haddr[2];
			sock_if->if_hwaddr[3] = hwa->if_haddr[3];
			sock_if->if_hwaddr[4] = hwa->if_haddr[4];
			sock_if->if_hwaddr[5] = hwa->if_haddr[5];
			add_sock_if_binding(sock_if);
		}
	}

	free_hwa_info(hwahead);

	printf("The canonical IP for this node is %s\n", canonical_name);
	return;
 error:
	perror("Error retrieving interface info and binding sockets on each. Cannot proceed. Exiting.\n");
	exit(0);
}

// This is global because I'm a lazy coder (just for today).

int wk_un_sock = -1; // The wellknown unix domain socket for the ODR to receive messages.

struct sockaddr_un wk_addr;

int main (int argc, char** argv)
{
	fd_set fd_sset;
	sock_if_t* temp = NULL;
	int maxfd = -1;
	char *buf = NULL;	
	odr_mesg_t* odr_msg_ptr = NULL;

	struct sockaddr_ll src_addr;
	socklen_t addrlen;
	int result = 0;

	sock_if_t* sock_if = NULL;

	// The unix domain sockets on which the ODR will receive messages from the server/client.
	// We need to bind the socket to a well known file name. We need one sockaddr_un for this (wk_addr).
	// Then, we need another sockaddr_un to know the temporary file name of the process calling us (app_addr).

	socklen_t sock_un_len;
	struct sockaddr_un app_addr;

	api_mesg_t* api_mesg = NULL;
	unsigned char src_mac[6]; 

	port_sunpath_t* port_sunpath = NULL;
	int src_port = -1;
        char *source_addr = malloc(sizeof(IP_ADDR_LEN));
        char *destination_addr = malloc(sizeof(IP_ADDR_LEN));
        char src[IP_ADDR_LEN];
        char dst[IP_ADDR_LEN];

	if (argc != 2) {
		fprintf(stderr, "Usage: odr <staleness value>\n");
		return -1;
	}

	staleness_value = atoi(argv[1]);
	
	printf("Started odr with staleness value = %d\n", staleness_value);

	port_sunpath = malloc(sizeof(port_sunpath_t));
	strncpy(port_sunpath->sun_path, WK_SFN, UNIX_PATH_MAX);
	port_sunpath->port = 13;
	add_port_sunpath(port_sunpath);

	print_port_sunpaths();
	unlink(WK_OFN);
	memset(&wk_addr, 0, sizeof(struct sockaddr_un));
	wk_addr.sun_family = AF_LOCAL;
	strncpy(wk_addr.sun_path, WK_OFN, sizeof(WK_OFN) - 1);

	wk_un_sock = socket(AF_LOCAL, SOCK_DGRAM, 0);

	result = bind(wk_un_sock, (SA*) &wk_addr, sizeof(wk_addr));
	sock_un_len = sizeof(app_addr);

	if (result < 0) {
		goto un_error;
	}

	populate_bind_interfaces();
        get_vm_name(canonical_name, odr_vm_name);
	for (;;) {
		FD_ZERO(&fd_sset);
		// Add all sockfd's to the fd_sset
		temp = sock_if_list;
		FD_SET(wk_un_sock, &fd_sset);
		while(temp) {
			FD_SET(temp->sock_fd, &fd_sset);
			maxfd = temp->sock_fd;
			temp = temp->next;
		}
		result = select(maxfd+1, &fd_sset, NULL, NULL, NULL);
		if (result < 1) {
			if (errno == EINTR)
				continue;
			goto error;
		}

		temp = sock_if_list;

		// Did I receive anything on any of my interfaces?
		while(temp) {
			if (FD_ISSET(temp->sock_fd, &fd_sset)) {
				buf = malloc(ETH_FRAME_LEN);
				memset(buf, 0, ETH_FRAME_LEN);
				memset(&src_addr, 0, sizeof(struct sockaddr_ll));
				recvfrom(temp->sock_fd, buf, ETH_FRAME_LEN, 0, (struct sockaddr*)&src_addr, &addrlen);
				memcpy(src_mac, ((void*)(buf+ETH_ALEN)), 6);
				odr_msg_ptr = (odr_mesg_t*)(buf+14);
                               
                                memset(source_addr, 0, IP_ADDR_LEN);
                                memset(destination_addr, 0, IP_ADDR_LEN);
                               
                                strncpy(source_addr, odr_msg_ptr->src_addr, IP_ADDR_LEN);
                                strncpy(destination_addr, odr_msg_ptr->dst_addr, IP_ADDR_LEN);
                                
                                get_vm_name(source_addr, src);
                                get_vm_name(destination_addr, dst);

                                //printf(" Source address %s\n", src);
                                //printf(" destination address %s\n", dst);

	                        printf(" ODR at node %s : Received ODR msg: type %d, src %s, dst %s\n", odr_vm_name, odr_msg_ptr->type, src, dst);
				//printf("ODR: Received ODR packet of type: %d\n from source canon name = %s, dest canon name = %s\n", odr_msg_ptr->type, odr_msg_ptr->src_addr, odr_msg_ptr->dst_addr);
				//printf("Src_mac = %x:%x:%x:%x:%x:%x\n", src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4], src_mac[5]);
				sock_if = get_by_sockfd(temp->sock_fd);
				handle_odr_msg(odr_msg_ptr, sock_if, src_mac);
			}
			temp = temp->next;
		}
		// Did I receive anything on the unix domain sockets?
		if (FD_ISSET(wk_un_sock, &fd_sset)) {
			// From a client or server process
			buf = malloc(sizeof(api_mesg_t));
			memset(buf, 0, sizeof(api_mesg_t));
			memset(&app_addr, 0, sizeof(struct sockaddr_un));
			recvfrom(wk_un_sock, buf, sizeof(api_mesg_t), 0, (struct sockaddr*)&app_addr, &sock_un_len);
			// Update entry in the port_sunpath_tbl if required
			src_port = find_port(app_addr.sun_path);
			if (src_port == -1) {
				port_sunpath = malloc(sizeof(port_sunpath_t));
				src_port = ephemeral_port_value++;
				port_sunpath->port = src_port;
				strncpy(port_sunpath->sun_path, app_addr.sun_path, UNIX_PATH_MAX);
				add_port_sunpath(port_sunpath);
			}
			api_mesg = (api_mesg_t*)buf;
			api_mesg->src_port = src_port;

                        memset(destination_addr, 0, IP_ADDR_LEN);
                        strncpy(destination_addr, api_mesg->dst_addr, IP_ADDR_LEN);
                        get_vm_name(destination_addr, dst);
			//printf("Received api_mesg from client/server for dst_addr = %s, dst_port = %d, msg = %s, force_disc_flag = %d\n", api_mesg->dst_addr, api_mesg->dst_port, api_mesg->msg, api_mesg->force_disc_flag);
	                printf(" ODR at node %s : Received msg %s from client/server for dst %s with force discovery value %d\n",
                                 odr_vm_name, api_mesg->msg, dst, api_mesg->force_disc_flag);
			handle_send_msg(api_mesg->dst_addr, api_mesg->src_port, api_mesg->dst_port, api_mesg->msg, api_mesg->force_disc_flag);
		}
	}
	return 0;

 un_error:
	perror("Unix domain socket error:\n");
	return -1;
 error:
	fprintf(stderr, "Error happened.\n");
	return -1;
}


void handle_odr_msg(odr_mesg_t* odr_msg, sock_if_t* sock_if, unsigned char* which_if_mac)
{
	sock_if_t* temp_sock_if = NULL;

	odr_mesg_rreq_t* odr_msg_rreq = NULL;
	odr_mesg_rrep_t* odr_msg_rrep = NULL;
	odr_mesg_app_t* odr_msg_app = NULL;
	parked_msg_t* parked_mesg = NULL;

	rt_entry_t* dst_route = NULL;

	int which_if_index = -1;

	int has_dst_route = 0, is_dst = 0;
        int set_rrep_already_sent = 0;
	int force_discovery_flag = 0;
	api_mesg_t* api_msg = NULL;
	int n = -1;

	which_if_index = sock_if->if_index;
	
	// Whatever be the type of the message, I build or update a reverse route.
	build_update_reverse_route(odr_msg, sock_if, which_if_mac);
	// Check the type
	switch(odr_msg->type) {
	case RREQ_TYPE:
		odr_msg_rreq = (odr_mesg_rreq_t*) odr_msg;

		force_discovery_flag = odr_msg_rreq->force_discovery;
                dst_route = find_route(odr_msg_rreq->dst_addr);
                if (dst_route) {
			has_dst_route = 1;
                }
                if (strncmp(odr_msg_rreq->dst_addr, canonical_name, IP_ADDR_LEN) == 0) {
			is_dst = 1;
                }
		if (should_respond(odr_msg_rreq, seen_rreq_list)) {
			if ((is_dst && !odr_msg_rreq->rrep_already_sent) ||
			    (has_dst_route && !odr_msg_rreq->rrep_already_sent && !force_discovery_flag)) {
				// If I'm the destination or I have the route, send back the rrep
				//printf("Reached home or found a way to home!!!\n");
				odr_mesg_rrep_t* rrep_msg = malloc(sizeof(odr_mesg_rrep_t));
				rrep_msg->type = RREP_TYPE;
				strncpy(rrep_msg->src_addr, odr_msg_rreq->dst_addr, IP_ADDR_LEN);
				strncpy(rrep_msg->dst_addr, odr_msg_rreq->src_addr, IP_ADDR_LEN);
				rrep_msg->broadcast_id = odr_msg_rreq->broadcast_id;
				rrep_msg->force_discovery = odr_msg_rreq->force_discovery;
				if (is_dst) {
					rrep_msg->hop_cnt = 0;
				} else {
					rrep_msg->hop_cnt = dst_route->hop_count;
					set_rrep_already_sent = 1;
				}

				dst_route = find_route(rrep_msg->dst_addr);
				if (!dst_route) {
					//					assert(staleness_value == 0);
					rediscover_route((odr_mesg_t*)rrep_msg);
				} else if ((dst_route != NULL) && (dst_route->hop_count == -1)) {
					// Park the message.
					parked_mesg = malloc(sizeof(parked_msg_t));
					parked_mesg->forced_disc_flag = rrep_msg->force_discovery;
					parked_mesg->odr_msg = (odr_mesg_t*) rrep_msg;
					parked_mesg->pm_next = NULL;
					add_parked_msg(&dst_route->parked_list, parked_mesg);
				} else {
					generic_send_msg((odr_mesg_t*) rrep_msg, dst_route);
				}
			} 
			// If I am not the destination and dont have the route, or if I have a route
			// continue sending RREQs. In latter case set the rrep_already_sent = 1
			if ((is_dst == 0 && has_dst_route == 0) || set_rrep_already_sent || (force_discovery_flag && !is_dst)) {
				// flood out on all other interfaces @TODO - check that not a repeat RREQ
				temp_sock_if = sock_if_list;
				while (temp_sock_if) {
					if (temp_sock_if->if_index != which_if_index) {
						// Send out the rreq after incrementing the hopcount
						odr_msg_rreq->hop_cnt++;
						odr_msg_rreq->rrep_already_sent = set_rrep_already_sent;
						flood_rreq(temp_sock_if, odr_msg_rreq);
					}
					temp_sock_if = temp_sock_if->next;
				}
			}
			// Note the rreq we flooded
			seen_rrex_t* seen_rreq = malloc(sizeof(seen_rrex_t));
			strncpy(seen_rreq->cn_name, odr_msg_rreq->src_addr, IP_ADDR_LEN);
			seen_rreq->hop_count = odr_msg_rreq->hop_cnt;
			seen_rreq->broadcast_id = odr_msg_rreq->broadcast_id;
			add_seen_rrex(seen_rreq, &seen_rreq_list);
		} else {
			//printf("Killing the RREQ because of broadcast id check!!!\n");
		}
		break;
	case RREP_TYPE: 
                
                // Update the routing table with the route and send out the parked packet 
		odr_msg_rrep = (odr_mesg_rrep_t*) odr_msg;
                    
                if (!strncmp(odr_msg_rrep->dst_addr, canonical_name, IP_ADDR_LEN) == 0) {
			// Find the route for this RREP's destination.
			// Send it off on its way.
			dst_route = find_route(odr_msg_rrep->dst_addr);
			if (!dst_route) {
				//				assert(staleness_value == 0);
				// Start route discovery
				rediscover_route((odr_mesg_t*) odr_msg_rrep);
			} else if ((dst_route != NULL) && (dst_route->hop_count == -1)) {
				// Park the message.
				parked_mesg = malloc(sizeof(parked_msg_t));
				parked_mesg->forced_disc_flag = odr_msg_rrep->force_discovery;
				parked_mesg->odr_msg = (odr_mesg_t*) odr_msg_rrep;
				parked_mesg->pm_next = NULL;
				add_parked_msg(&dst_route->parked_list, parked_mesg);
			}else {
                                odr_msg_rrep->hop_cnt +=1;
				generic_send_msg((odr_mesg_t*) odr_msg_rrep, dst_route);
			}
		}
		break;
	case APP_TYPE:
		odr_msg_app = (odr_mesg_app_t*) odr_msg; 

		// If I'm not the final destination, then forward the packet. Else forward it in the general direction of the destination.
                if (strncmp(odr_msg_app->dst_addr, canonical_name, IP_ADDR_LEN) == 0) {
			// Someone should be listening to it.
			struct sockaddr_un other_addr;
			other_addr.sun_family = AF_LOCAL;
			strncpy(other_addr.sun_path, (find_sunpath(odr_msg_app->dst_port)), UNIX_PATH_MAX);
			api_msg = malloc(sizeof(api_mesg_t));
			strncpy(api_msg->dst_addr, odr_msg_app->dst_addr, IP_ADDR_LEN);
			strncpy(api_msg->src_addr, odr_msg_app->src_addr, IP_ADDR_LEN);
			api_msg->dst_port = odr_msg_app->dst_port;
			api_msg->src_port = odr_msg_app->src_port;
			strncpy(api_msg->msg, odr_msg_app->msg, API_MSG_LEN);
			api_msg->force_disc_flag = 0;
			n = sendto(wk_un_sock, api_msg, sizeof(api_mesg_t), 0, (SA*) &other_addr, sizeof(struct sockaddr_un));
			if (n < 0) {
				goto missing_server_error;
			}
		} else {
			// Find the route for this APP type's destination.
			// Send it off on its way.
			dst_route = find_route(odr_msg_app->dst_addr);
			if (!dst_route) {
				//				assert(staleness_value == 0); Not applicable for intermediary nodes.
				// Start route discovery
				rediscover_route((odr_mesg_t*) odr_msg_app);
			} else if ((dst_route != NULL) && (dst_route->hop_count == -1)) {
				// Park the message.
				parked_mesg = malloc(sizeof(parked_msg_t));
				parked_mesg->forced_disc_flag = 0;
				parked_mesg->odr_msg = (odr_mesg_t*) odr_msg_app;
				parked_mesg->pm_next = NULL;
				add_parked_msg(&dst_route->parked_list, parked_mesg);
			       //printf("Added to the parked list = %p\n", dst_route->parked_list);
			} else {
                                odr_msg_app->hop_cnt +=1;
				generic_send_msg((odr_mesg_t*) odr_msg_app, dst_route);
			}
		}
		break;
	}
	print_routing_table();
	return;
 missing_server_error:
	perror("Couldn't communicate with the time server: ");
	exit(0);
}

void handle_send_msg(char* dst_addr, int src_port, int dst_port, char* msg, int flag) 
{
	rt_entry_t* route = NULL;
	odr_mesg_app_t* odr_mesg = NULL;
	parked_msg_t* parked_mesg = NULL;
	rt_entry_t* new_rt_entry = NULL;

	int i = 0;

	// Create a ODR App type message. 
	odr_mesg = malloc(sizeof(odr_mesg_app_t));
	odr_mesg->type = APP_TYPE;
	odr_mesg->hop_cnt = 0;
	// My name!
	strncpy(odr_mesg->src_addr, canonical_name, IP_ADDR_LEN);
	// Destination!
	strncpy(odr_mesg->dst_addr, dst_addr, IP_ADDR_LEN);
	odr_mesg->src_port = src_port;
	odr_mesg->dst_port = dst_port;
	strncpy(odr_mesg->msg, msg, API_MSG_LEN);

	if (flag) {
		printf("Attemptimg force discovery.\n");
		route = find_route(dst_addr);
		if (route && route->hop_count != -1) {
			// Existing route. Can never have pending messages.
			delete_route(route);
		} else if (route && route->hop_count == -1) {
			// Incomplete route. Can have pending messages.
			// Just flood.
			first_flood_rreq(dst_addr, flag);
		}
	}

	// Check if route exists
	route = find_route(dst_addr);
	if (!route) {
		// The specs say --
		// "calls to msg_send by time servers should never cause ODR at the server node to initiate RREQs, 
		// since the receipt of a time client request implies that a route back to the client node should now 
		// exist in the routing table."
		// Let me check that.

		// The response from the server will have some content in the msg, unlike that from the client
		// which is just a poke.
		if (msg && msg[0] != '\0') {
			assert(0);
		}
		// Create a placeholder for the parked message
		parked_mesg = malloc(sizeof(parked_msg_t));
		parked_mesg->forced_disc_flag = flag;
		parked_mesg->odr_msg = (odr_mesg_t*) odr_mesg;
		parked_mesg->pm_next = NULL;

		// If not, create a incomplete route entry and park the message with that entry. 
		new_rt_entry = malloc(sizeof(rt_entry_t));
		strncpy(new_rt_entry->cn_dst_name, dst_addr, IP_ADDR_LEN);
		for (i = 0; i< 6; i++) 
			new_rt_entry->next_hop_mac[i] = 0x00;
		new_rt_entry->if_index = -1;
		new_rt_entry->hop_count = -1;
		new_rt_entry->timestamp = -1;
		new_rt_entry->parked_list = NULL;
		add_parked_msg(&new_rt_entry->parked_list, parked_mesg);
		//printf("Added to the parked list = %p\n", new_rt_entry->parked_list);
		new_rt_entry->rt_next = NULL;
		add_route(new_rt_entry); // Add the incomplete route

		//         Send a RREQ on all interfaces
		first_flood_rreq(dst_addr, flag);

		print_routing_table();
	} else if ((route != NULL) && (route->hop_count == -1)) {
		// Park the message.
		parked_mesg = malloc(sizeof(parked_msg_t));
		parked_mesg->forced_disc_flag = flag;
		parked_mesg->odr_msg = (odr_mesg_t*) odr_mesg;
		parked_mesg->pm_next = NULL;
		add_parked_msg(&route->parked_list, parked_mesg);
		//printf("Added to the parked list = %p\n", route->parked_list);
	} else {
		// Else, send the message.
		generic_send_msg((odr_mesg_t*)odr_mesg, route);
	}
}

void first_flood_rreq(char* dst_addr, int force_discovery)
{
	sock_if_t* temp_sock_if = NULL;

	odr_mesg_rreq_t* rreq_msg = malloc(sizeof(odr_mesg_rreq_t));
	rreq_msg->type = RREQ_TYPE;
	strncpy(rreq_msg->src_addr, canonical_name, IP_ADDR_LEN);
	strncpy(rreq_msg->dst_addr, dst_addr, IP_ADDR_LEN);
	rreq_msg->broadcast_id = ++broadcast_id;
	rreq_msg->hop_cnt = 0;
	rreq_msg->rrep_already_sent = 0;
	rreq_msg->force_discovery = force_discovery;

	temp_sock_if = sock_if_list;
	// Note the rreq we flooded
	seen_rrex_t* seen_rreq = malloc(sizeof(seen_rrex_t));
	strncpy(seen_rreq->cn_name, rreq_msg->src_addr, IP_ADDR_LEN);
	seen_rreq->hop_count = rreq_msg->hop_cnt;
	seen_rreq->broadcast_id = rreq_msg->broadcast_id;
	add_seen_rrex(seen_rreq, &seen_rreq_list);

	while (temp_sock_if) {
		flood_rreq(temp_sock_if, rreq_msg);
		temp_sock_if = temp_sock_if->next;
	}
}

void flood_rreq(sock_if_t* sock_if, odr_mesg_rreq_t* msg) 
{
	struct sockaddr_ll dest_addr;
	int result = -1;
	int j = 0;
	unsigned char* buf = NULL;
	unsigned char src_mac[6];

	// Destination mac address = Flood
	unsigned char dest_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	unsigned char* ether_head = NULL;


	unsigned char* ether_data = NULL;
        char src[IP_ADDR_LEN];
        char dst[IP_ADDR_LEN];
       
        char *source_addr = malloc(sizeof(IP_ADDR_LEN));
        char *destination_addr = malloc(sizeof(IP_ADDR_LEN));
        strncpy(source_addr, msg->src_addr, IP_ADDR_LEN);
        strncpy(destination_addr, msg->dst_addr, IP_ADDR_LEN);
        get_vm_name(source_addr, src);
        get_vm_name(destination_addr, dst);
	
	/*another pointer to ethernet header*/
	struct ethhdr *eh = NULL;

	// Source mac address
	for(j=0; j<6; j++) {
		src_mac[j] = sock_if->if_hwaddr[j];
	}

	buf = malloc(ETH_FRAME_LEN);
	ether_head = buf;
	ether_data = buf + 14;
	eh = (struct ethhdr *)ether_head;

	memset(&dest_addr, 0, sizeof(struct sockaddr_ll));
	memset(buf, 0, ETH_FRAME_LEN);

	dest_addr.sll_family   = PF_PACKET;	
	dest_addr.sll_protocol = htons(ODR_PROTO);	
	dest_addr.sll_ifindex  = sock_if->if_index;
	dest_addr.sll_hatype   = ARPHRD_ETHER;
	dest_addr.sll_pkttype  = PACKET_OTHERHOST;
	dest_addr.sll_halen    = ETH_ALEN;	
	for (j=0; j<6; j++) {
		dest_addr.sll_addr[j] = 0xFF;
	}
	dest_addr.sll_addr[6]  = 0x00;/*not used*/
	dest_addr.sll_addr[7]  = 0x00;/*not used*/
	
	/*set the frame header*/
	memcpy((void*)buf, (void*)dest_mac, ETH_ALEN);
	memcpy((void*)(buf+ETH_ALEN), (void*)src_mac, ETH_ALEN);
	eh->h_proto = htons(ODR_PROTO);
	
	// The payload is the rreq message. Copy it, instead of doing funky stuff with pointers, so that life is simpler.
	memcpy(ether_data, msg, sizeof(odr_mesg_rreq_t));

	printf(" ODR at node %s : Sending frame header src %s, dest mac %x:%x:%x:%x:%x:%x \n", odr_vm_name, odr_vm_name,
                 dest_mac[0], dest_mac[1], dest_mac[2], dest_mac[3], dest_mac[4], dest_mac[5]);

        printf(" ODR msg: type %d, src %s, dst %s\n", msg->type, src, dst);
	//printf("Sending out flood rreq for destination address = %s\n", msg->dst_addr);
	result = sendto(sock_if->sock_fd, buf, ETH_FRAME_LEN, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
	if (result < 0) {
		goto send_error;
	}
        
	return;
 send_error:
	perror("Send error: ");
	exit(0);

}

void generic_send_msg(odr_mesg_t* msg, rt_entry_t* route) 
{
	struct sockaddr_ll dest_addr;
	int result = -1;
	int j = 0;
	unsigned char* buf = NULL;

	unsigned char src_mac[6];

	unsigned char dest_mac[6];
	unsigned char* ether_head = NULL;
	unsigned char* ether_data = NULL;

        char src[IP_ADDR_LEN];
        char dst[IP_ADDR_LEN];
	
	int mesg_size = -1;

        char *source_addr = malloc(sizeof(IP_ADDR_LEN));
        char *destination_addr = malloc(sizeof(IP_ADDR_LEN));
        memset(source_addr, 0, IP_ADDR_LEN);
        memset(destination_addr, 0, IP_ADDR_LEN);
        strncpy(source_addr, msg->src_addr, IP_ADDR_LEN);
        strncpy(destination_addr, msg->dst_addr, IP_ADDR_LEN);
        get_vm_name(source_addr, src);
        get_vm_name(destination_addr, dst);
        
        sock_if_t* sock_if = get_by_ifindex(route->if_index);

	/*another pointer to ethernet header*/
	struct ethhdr *eh = NULL;

	// Source mac address
	for(j=0; j<6; j++) {
		src_mac[j] = sock_if->if_hwaddr[j];
	}

        for(j=0; j<6; j++) {
		dest_mac[j] = route->next_hop_mac[j];
	}

	buf = malloc(ETH_FRAME_LEN);
	ether_head = buf;
	ether_data = buf + 14;
	eh = (struct ethhdr *)ether_head;

	memset(&dest_addr, 0, sizeof(struct sockaddr_ll));
	memset(buf, 0, ETH_FRAME_LEN);

	dest_addr.sll_family   = PF_PACKET;	
	dest_addr.sll_protocol = htons(ODR_PROTO);	
	dest_addr.sll_ifindex  = route->if_index;
	dest_addr.sll_hatype   = ARPHRD_ETHER;
	dest_addr.sll_pkttype  = PACKET_OTHERHOST;
	dest_addr.sll_halen    = ETH_ALEN;	
	for (j=0; j<6; j++) {
		dest_addr.sll_addr[j] = route->next_hop_mac[j];
	}
	dest_addr.sll_addr[6]  = 0x00;/*not used*/
	dest_addr.sll_addr[7]  = 0x00;/*not used*/
	
	/*set the frame header*/
	memcpy((void*)buf, (void*)dest_mac, ETH_ALEN);
	memcpy((void*)(buf+ETH_ALEN), (void*)src_mac, ETH_ALEN);
	eh->h_proto = htons(ODR_PROTO);
	
	switch(msg->type) {
	case RREQ_TYPE:
		mesg_size = sizeof(odr_mesg_rreq_t);
		break;
	case RREP_TYPE:
		mesg_size = sizeof(odr_mesg_rrep_t);
		break;
	case APP_TYPE:
		mesg_size = sizeof(odr_mesg_app_t);
		break;
	default:
		assert(0);
	}
	memcpy(ether_data, msg, mesg_size);
	
	printf(" ODR at node %s : Sending frame header src %s, dest mac %x:%x:%x:%x:%x:%x \n", odr_vm_name, odr_vm_name,
                 dest_mac[0], dest_mac[1], dest_mac[2], dest_mac[3], dest_mac[4], dest_mac[5]);
        printf(" ODR msg: type %d, src %s, dst %s \n", msg->type, src, dst);
	result = sendto(sock_if->sock_fd, buf, ETH_FRAME_LEN, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
	if (result < 0) {
		goto send_error;
	}
	return;
 send_error:
	perror("Send error: ");
	exit(0);
}

void build_update_reverse_route(odr_mesg_t* odr_msg, sock_if_t* sock_if, unsigned char* which_if_mac)
{
	rt_entry_t* route = NULL;
	rt_entry_t* new_route = NULL;
	int force_discovery_flag = 0;
	int i = 0;
	odr_mesg_rrep_t* odr_msg_rrep = NULL;
	odr_mesg_rreq_t* odr_msg_rreq = NULL;

	parked_msg_t *parked_msg = NULL;
	int which_if_index = sock_if->if_index;

	// Force discovery?
	if (odr_msg->type == RREP_TYPE) {
		odr_msg_rrep = (odr_mesg_rrep_t*) odr_msg;
		if (odr_msg_rrep->force_discovery == 1) {
			force_discovery_flag = 1;
		}
	}
	if (odr_msg->type == RREQ_TYPE) {
		odr_msg_rreq = (odr_mesg_rreq_t*) odr_msg;
		if (odr_msg_rreq->force_discovery == 1) {
			force_discovery_flag = 1;
		}
	}

	route = find_route(odr_msg->src_addr);
	if (!route) {
		// Build the reverse route
		new_route = malloc(sizeof(rt_entry_t));
		strncpy(new_route->cn_dst_name, odr_msg->src_addr, IP_ADDR_LEN);
		for (i = 0; i< 6; i++) 
			new_route->next_hop_mac[i] = which_if_mac[i];
		new_route->if_index = which_if_index;
		new_route->hop_count = odr_msg->hop_cnt + 1;
		new_route->timestamp = time(NULL);
		new_route->parked_list = NULL;
		new_route->rt_next = NULL;
		add_route(new_route);
	} else { 
		// If an incomplete route exists, then fill it out
		if (route->hop_count == -1) {
			for (i = 0; i< 6; i++) 
				route->next_hop_mac[i] = which_if_mac[i];
			route->if_index = which_if_index;
			route->hop_count = odr_msg->hop_cnt + 1;
			route->timestamp = time(NULL);
			// Send out the parked messages.
			while (route->parked_list) {
				parked_msg = remove_parked_msg(&(route->parked_list));
				generic_send_msg(parked_msg->odr_msg, route);
			}
		} else {
			// If a reverse route exists, update if a better route is found.
			if(route->hop_count > (odr_msg->hop_cnt + 1) || 
			   ((memcmp(route->next_hop_mac, which_if_mac, IP_ADDR_LEN) != 0)
			    && route->hop_count == (odr_msg->hop_cnt + 1)) ||
			   (force_discovery_flag == 1)) {
				// If a the rreq has a better hop count or a different neighbour then update the existing reverse route
				for (i = 0; i< 6; i++) 
					route->next_hop_mac[i] = which_if_mac[i];
				route->if_index = which_if_index;
				route->hop_count = odr_msg->hop_cnt + 1;
				route->timestamp = time(NULL);
			}
		}
	}

}

void rediscover_route(odr_mesg_t* odr_msg)
{
	parked_msg_t *parked_msg = NULL;
	rt_entry_t* new_rt_entry = NULL;
	int i = 0;

	// Start route discovery
	// Create a placeholder for the parked message
	parked_msg = malloc(sizeof(parked_msg_t));
	parked_msg->forced_disc_flag = 0; // not forced discovery
	parked_msg->odr_msg = (odr_mesg_t*)odr_msg;
	parked_msg->pm_next = NULL;

	// If not, create a incomplete route entry and park the message with that entry. 
	new_rt_entry = malloc(sizeof(rt_entry_t));
	strncpy(new_rt_entry->cn_dst_name, odr_msg->dst_addr, IP_ADDR_LEN);
	for (i = 0; i< 6; i++) 
		new_rt_entry->next_hop_mac[i] = 0x00;
	new_rt_entry->if_index = -1;
	new_rt_entry->hop_count = -1;
	new_rt_entry->timestamp = -1;
	new_rt_entry->parked_list = NULL;
	add_parked_msg(&new_rt_entry->parked_list, parked_msg);
	new_rt_entry->rt_next = NULL;
	add_route(new_rt_entry); // Add the incomplete route

	// Send a RREQ on all interfaces
	first_flood_rreq(odr_msg->dst_addr, 0);

}


void print_routing_table(void)
{
	rt_entry_t* temp = routing_table;
	printf("######## Routing table #########\n");
	printf("[cn_destination] [next_hop_mac] [if_index] [hop_count] [timestamp]\n"); 
	while(temp) {
		printf("%s %x:%x:%x:%x:%x:%x %d %d %ld \n", 
		       temp->cn_dst_name,
		       temp->next_hop_mac[0],
		       temp->next_hop_mac[1],
		       temp->next_hop_mac[2],
		       temp->next_hop_mac[3],
		       temp->next_hop_mac[4],
		       temp->next_hop_mac[5],
		       temp->if_index,
		       temp->hop_count,
		       temp->timestamp);
		temp = temp->rt_next;
	}

}

void print_port_sunpaths(void)
{
	port_sunpath_t* temp = port_tbl;
	printf("Port v/s sun_path mapping -\n");
	while(temp != NULL) {
		printf("%d\t%s\n", temp->port, temp->sun_path);
		temp = temp->next;
	}
}
