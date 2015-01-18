
#ifndef ODR_H
#define ODR_H

#include "unp.h"
#include <assert.h>
#include "api.h"

#define ODR_PROTO 0x6872

extern int staleness_value;

struct sock_if_t;

/*
 * This structure will
 * 1. Maintain the list of interfaces we should flood on, on receive of a RREQ.
 * 2. Maintain the associated sock_fd.
 */
typedef struct sock_if_binding_t
{
	int sock_fd;
	int if_index;
	unsigned char if_hwaddr[6]; 
	struct sock_if_binding_t* next;
} sock_if_t;

sock_if_t* sock_if_list;

void add_sock_if_binding(sock_if_t* sock_if_binding)
{
	sock_if_t* temp = sock_if_list;
	sock_if_binding->next = NULL;
	if (!temp) {
		sock_if_list = sock_if_binding;
		return;
	}
	while(temp && temp->next) {
		temp = temp->next;
	}
	temp->next = sock_if_binding;
}

sock_if_t* get_by_sockfd(int sock_fd)
{
	sock_if_t* temp = sock_if_list;
	while(temp) {
		if (temp->sock_fd == sock_fd) {
			return temp;
		}
		temp = temp->next;
	}
	assert(0);
	return NULL;
}

sock_if_t* get_by_ifindex(int if_index)
{
	sock_if_t* temp = sock_if_list;
	while(temp) {
		if (temp->if_index == if_index) {
			return temp;
		}
		temp = temp->next;
	}
	assert(0);
	return NULL;
}

/**** The ODR message types start ****/
#define RREQ_TYPE 1
#define RREP_TYPE 2
#define APP_TYPE 3

typedef struct odr_mesg_rreq_t
{
	int type;
	char src_addr[IP_ADDR_LEN];
	char dst_addr[IP_ADDR_LEN];
	int hop_cnt;
	int broadcast_id;
	int rrep_already_sent;
	int force_discovery;
} odr_mesg_rreq_t;

typedef struct odr_mesg_rrep_t
{
	int type;
	char src_addr[IP_ADDR_LEN];
	char dst_addr[IP_ADDR_LEN];
	int hop_cnt;
	int broadcast_id;
	int force_discovery;
} odr_mesg_rrep_t;

typedef struct odr_mesg_app_t
{
	int type;
	char src_addr[IP_ADDR_LEN];
	char dst_addr[IP_ADDR_LEN];
	int hop_cnt;
	int src_port;
	int dst_port;
	int num_bytes; 
	char msg[API_MSG_LEN];
} odr_mesg_app_t;

typedef struct odr_mesg_t
{
	int type;
	char src_addr[IP_ADDR_LEN];
	char dst_addr[IP_ADDR_LEN];
	int hop_cnt;
} odr_mesg_t;

/**** The ODR message types end ****/
struct parked_msg_t;

typedef struct parked_msg_t
{
	int forced_disc_flag; // Does this parked request require me to do a force discovery?
	odr_mesg_t* odr_msg;
	struct parked_msg_t* pm_next;
} parked_msg_t;

typedef struct rt_entry_t
{
	char cn_dst_name[IP_ADDR_LEN];
	unsigned char next_hop_mac[6];
	int if_index;     // Which interface of mine do I send it out to?
	int hop_count;
	long timestamp;
	parked_msg_t* parked_list;
	struct rt_entry_t* rt_next;
} rt_entry_t;


rt_entry_t* routing_table;

void add_parked_msg(parked_msg_t** head, parked_msg_t* parked_msg)
{
	parked_msg_t* temp = *head;
	parked_msg->pm_next = NULL;
	while(temp && temp->pm_next) {
		temp = temp->pm_next;
	}
	if (temp) {
		temp->pm_next = parked_msg;
	} else {
		// First entry
		*head = parked_msg;
	}
}

parked_msg_t* remove_parked_msg(parked_msg_t** head)
{
	parked_msg_t* first = NULL;
	if (*head) {
		first = *head;
		*head = (*head)->pm_next;
	}
	return first;
}

void delete_route(rt_entry_t*);

void add_route(rt_entry_t* rt_entry)
{
	rt_entry_t* temp = routing_table;
	rt_entry->rt_next = NULL;
	if (!temp) {
		routing_table = rt_entry;
		return;
	}
	while (temp && temp->rt_next) {
		temp = temp->rt_next;
	}
	temp->rt_next = rt_entry;
}

rt_entry_t* find_route(char* cn_dst_name)
{
	rt_entry_t* temp = routing_table;
	rt_entry_t* route_to_del = NULL;
	long curr_time = time(NULL);
	while (temp) {
		if (strncmp(temp->cn_dst_name, cn_dst_name, IP_ADDR_LEN) == 0) {
			long time_diff = curr_time - temp->timestamp;
			if (time_diff > staleness_value && (temp->hop_count != -1)) {
				route_to_del = temp;
			} else {
				return temp;
		        }
		}
		temp = temp->rt_next;
	}
	if (route_to_del) {
		delete_route(route_to_del);
	}
	return NULL;
}

void update_route(char* cn_dst_name, char* next_hop_mac, int if_index, int hop_count)
{
	// First locate the rt_entry for the cn_dst_name
	rt_entry_t* temp = routing_table;
	assert(!temp); // I should never get here if I didn't start a route discovery for this.
	while (strncmp(cn_dst_name, temp->cn_dst_name, IP_ADDR_LEN) != 0) {
		temp = temp->rt_next;
	}
	// Update
	temp->next_hop_mac[0] = next_hop_mac[0];
	temp->next_hop_mac[1] = next_hop_mac[1];
	temp->next_hop_mac[2] = next_hop_mac[2];
	temp->next_hop_mac[3] = next_hop_mac[3];
	temp->next_hop_mac[4] = next_hop_mac[4];
	temp->next_hop_mac[5] = next_hop_mac[5];
	temp->if_index = if_index;
	temp->hop_count = hop_count;
	temp->timestamp = time(NULL);
}

void delete_route(rt_entry_t* rt_entry)
{
	rt_entry_t* temp = routing_table;
	rt_entry_t* prev = NULL;
	assert(temp); // Should not be empty
	while (temp) {
		if (temp == rt_entry) {
			if (prev != NULL) {
				prev->rt_next = temp->rt_next;
			} else {
				// Deleting the first entry.
				if (temp->rt_next == NULL) {
					// Deleting the only entry in the list.
					routing_table = NULL;
				} else {
					routing_table = temp->rt_next;
				}
			}
			free(temp);
			return;
		}
		prev = temp;
		temp = temp->rt_next;
	}
	assert(0);
}

struct seen_rrex_t;

typedef struct seen_rrex_t
{
	char cn_name[IP_ADDR_LEN];
	int hop_count;
	int broadcast_id;
	struct seen_rrex_t* next;
} seen_rrex_t;

void add_seen_rrex(seen_rrex_t* seen_rrex, seen_rrex_t** seen_rrex_list) 
{
	seen_rrex_t* temp = (*seen_rrex_list);
	seen_rrex->next = NULL;
	if (temp) {
		while(temp->next) {
			temp = temp->next;
		}
		temp->next = seen_rrex;
	} else {
		(*seen_rrex_list) = seen_rrex;
	}
}

seen_rrex_t* get_seen_rrex(seen_rrex_t* seen_rrex_list, char* cn_name, int broadcast_id)
{
	seen_rrex_t* temp = seen_rrex_list;
	while (temp) {
		if ((strncmp(cn_name, temp->cn_name, IP_ADDR_LEN) == 0)
		    && (broadcast_id == temp->broadcast_id)) {
			return temp;
		}
		temp = temp->next;
	}
	return NULL;
}

int should_respond(odr_mesg_rreq_t* odr_rreq, seen_rrex_t* seen_rreq_list)
{
	seen_rrex_t* seen_rreq = NULL;
	if ( (seen_rreq = (get_seen_rrex(seen_rreq_list, odr_rreq->src_addr, odr_rreq->broadcast_id))) != NULL) {
		if (seen_rreq->hop_count > (odr_rreq->hop_cnt + 1)) {
			return 1;
		} else {
			return 0;
		}
	} else {
		return 1;
	}
}

struct port_sunpath_t;

#define UNIX_PATH_MAX 108
typedef struct port_sunpath_t
{
	int port;
	char sun_path[UNIX_PATH_MAX];
	struct port_sunpath_t* next;
} port_sunpath_t;

port_sunpath_t* port_tbl;

int find_port(char* sun_path)
{
	port_sunpath_t* temp = port_tbl;
	while (temp) {
		if (strncmp(sun_path, temp->sun_path, UNIX_PATH_MAX) == 0) {
			return temp->port;
		}
		temp = temp->next;
	}
	return -1;
}

char* find_sunpath(int port)
{
	port_sunpath_t* temp = port_tbl;
	while (temp) {
		if (temp->port == port) {
			return temp->sun_path;
		}
		temp = temp->next;
	}
	return NULL;

}

void add_port_sunpath(port_sunpath_t* port_sunpath)
{
	port_sunpath_t* temp = port_tbl;
	if (temp == NULL) {
		port_tbl = port_sunpath;
		return;
	}
	while(temp) {
		if (temp->next == NULL) {
			temp->next = port_sunpath;
			break;
		}
		temp = temp->next;
	}
}

void populate_bind_interfaces(void);

void handle_send_msg(char* dst_addr, int src_port, int dst_port, char* msg, int flag);
void handle_odr_msg(odr_mesg_t* odr_msg, sock_if_t*, unsigned char* src_mac);

void first_flood_rreq(char* dst_addr, int);

void flood_rreq(sock_if_t*, odr_mesg_rreq_t*);
void generic_send_msg(odr_mesg_t*, rt_entry_t*);
void rediscover_route(odr_mesg_t* odr_msg);
void print_routing_table(void);

void build_update_reverse_route(odr_mesg_t*, sock_if_t*, unsigned char*);

void print_port_sunpaths(void);
#endif
