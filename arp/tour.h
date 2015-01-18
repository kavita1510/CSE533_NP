#include <setjmp.h>
#include <sys/utsname.h>
#include "unp.h"
#include "hw_addrs.h"

#define MAX_TOUR_NODES 25
#define IP_ADDR_LEN 16
#define IP4_HDRLEN 20
#define PROTO 110
#define NODE_NAME_LEN 5
#define MAX_MSG_LEN 150

typedef struct tour_pack_t {
    int curr_pointer;
    int size;
    char tour_nodes[MAX_TOUR_NODES][IP_ADDR_LEN];
}tour_pack_t;

typedef struct multicast_info_t {
    char multicast_addr[IP_ADDR_LEN];
    char multicast_port[5];
}multicast_info_t; 

void find_self_ip();
int send_tour_packet(int, tour_pack_t*, multicast_info_t*);
int get_tour_nodes(int, char **, char [][IP_ADDR_LEN]);
uint16_t checksum (uint16_t* , int );
int setup_mcast_sockets(char *, char *);
int mcast_udp_client(char*, char *, SA **, socklen_t *);
int mcast_join(int, const SA *, socklen_t, const char*, u_int);
void recv_mcast_udp(int, socklen_t);
