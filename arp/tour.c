#include "ping.h"
#include "tour.h"
#include "arp.h"
#include "ping.h"
#include <linux/if_ether.h>

int visited_flag;
int mcast_udp_sendfd, mcast_udp_recvfd;
socklen_t			salen;
struct sockaddr_in		*m_send_addr;
char node_name[NODE_NAME_LEN];
char self_ip[IP_ADDR_LEN];
unsigned char self_mac[8];
int self_ifindex;

static sigjmp_buf jmpbuf;

void find_self_ip() {
    struct hwa_info	*hwa;
    struct in_addr ipv4addr;
    struct hostent *h;
    int i = 0;
    for (hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {
        if (strcmp(hwa->if_name, "eth0") == 0 && hwa->ip_addr != NULL) {
            strncpy(self_ip, Sock_ntop_host(hwa->ip_addr,sizeof(hwa->ip_addr)), IP_ADDR_LEN);                    
	    for(i=0;i<IF_HADDR;i++) {
		    self_mac[i] = hwa->if_haddr[i];
	    }
	    self_mac[6] = 0x00;
	    self_mac[7] = 0x00;
            /* Find the self node name */
            inet_pton(AF_INET, self_ip, &ipv4addr);
	    self_ifindex = hwa->if_index;
            h = gethostbyaddr(&ipv4addr, sizeof ipv4addr, AF_INET);
            strncpy(node_name, h->h_name, NODE_NAME_LEN);
            break;
        }
    }
}

void handle_timeout(int signo)
{
	siglongjmp(jmpbuf, 1);
}

int ping_count;

void process_ping_packet(int sockfd_pg)
{
	char *buf = NULL;
	int ping_data_len, len = 0;
	struct sockaddr_in ping_src;
	socklen_t ping_srclen;
	struct ip *ip;
	int hlenl, icmplen;

	struct icmp *icmp = NULL;
	char ipstr[INET_ADDRSTRLEN];

	buf = malloc(1500);
	ping_data_len = 1500;
	ping_srclen = sizeof(struct sockaddr_in);

	len = recvfrom(sockfd_pg, buf, ping_data_len, 0, (SA*)&ping_src, &ping_srclen);
	if (len < 0) {
		goto error;
	}
	ip = (struct ip*) buf;
	hlenl = ip->ip_hl << 2;

	if (ip->ip_p != IPPROTO_ICMP) {
		return;
	}

	icmp = (struct icmp*)(buf + hlenl);
	
	icmplen = len - hlenl;
	
	inet_ntop(AF_INET, &(ping_src.sin_addr), ipstr, INET_ADDRSTRLEN);

	if (icmp->icmp_type == ICMP_ECHOREPLY && icmp->icmp_id == ntohs(ICMP_ID)) { // icmp->icmp_type == ICMP_ECHOREPLY && 
		printf("PING: Received %d bytes from %s: seq = %u, ttl = %d\n", icmplen, ipstr, ntohs(icmp->icmp_seq), ip->ip_ttl);
	}
	return;
 error:
	perror("Error receiving ping packet: ");
}

int main(int argc , char **argv) {

    int sockfd_rt = -1;
    int sockfd_pg = -1;
    fd_set fd_sset;
    struct sockaddr_in src;
    char tour_nodes[MAX_TOUR_NODES][IP_ADDR_LEN];
    void *buf;
    int one = 1;
    const int *val = &one;
    int maxfd = -1;
    int result = 0;
    int i =  0;
    socklen_t srclen;
    multicast_info_t *multicast_info = malloc(sizeof(multicast_info_t));
    struct hostent *h;
    char msg[MAX_MSG_LEN];
    time_t ticks;
    struct timeval tv;
    struct sockaddr_in* dest = NULL;
    char dest_ip[INET_ADDRSTRLEN];
    int sockfd = -1;

    socklen_t sockaddrlen;

    // Find the mac address of the dest, given the ip
    struct hwaddr HWaddr;

    int recv_data_len = IP4_HDRLEN + sizeof(tour_pack_t) + sizeof(multicast_info_t);
    
    tour_pack_t *tour_pack = malloc(sizeof(tour_pack_t));
    tour_pack_t *tp = malloc(sizeof(tour_pack_t));
  
    int tour_should_end = 0;

    find_self_ip();
    srclen = sizeof(src); 
    buf = malloc(recv_data_len); 
    sockfd_rt = Socket(AF_INET, SOCK_RAW, PROTO);
    sockfd_pg = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

    if((setsockopt(sockfd_rt, IPPROTO_IP, IP_HDRINCL, val, sizeof(one))) < 0)
    {
        perror("setsockopt() error");
        exit(-1);
    }
    
    if(argc > 1) {
        get_tour_nodes(argc, argv, tour_nodes);
        tour_pack->curr_pointer = 1;
        tour_pack->size = argc;
        for (i=0;i < argc;i++) {
            memcpy(tour_pack->tour_nodes[i], tour_nodes[i], IP_ADDR_LEN);
        }
        strncpy(multicast_info->multicast_addr, "239.192.0.10", IP_ADDR_LEN);
        strncpy(multicast_info->multicast_port, "6872", 5);
        if(visited_flag == 0) {
            setup_mcast_sockets(multicast_info->multicast_addr, multicast_info->multicast_port);
            visited_flag = 1;
        }
        send_tour_packet(sockfd_rt, tour_pack, multicast_info);
    }
    

    for(;;) {
 repeat:
        FD_ZERO(&fd_sset);
	if(sockfd_rt > maxfd)
		maxfd = sockfd_rt;
	if (sockfd_pg > maxfd)
		maxfd = sockfd_pg;
	if (mcast_udp_recvfd > maxfd)
		maxfd = mcast_udp_recvfd;

        FD_SET(sockfd_rt, &fd_sset);
	FD_SET(sockfd_pg, &fd_sset);

	if (mcast_udp_recvfd > 0) {
		FD_SET(mcast_udp_recvfd, &fd_sset);
	}
        result = select(maxfd + 1, &fd_sset, NULL, NULL, NULL);
        if (result < 1) {
            if (errno == EINTR)
                continue;
            goto error;   
        
        }

        if(FD_ISSET(sockfd_rt, &fd_sset)) { 
           recvfrom(sockfd_rt, buf, recv_data_len, 0, (SA*)&src, &srclen); 
           //@TODO:: Check the id field 
           h = gethostbyaddr(&(src.sin_addr), sizeof(struct in_addr), AF_INET); 

           ticks = time(NULL);

           printf("%s Received source routing packet from %s \n", ctime(&ticks), h->h_name);
           memcpy(tp, buf + IP4_HDRLEN, sizeof(tour_pack_t));
           memcpy(multicast_info, buf + IP4_HDRLEN + sizeof(tour_pack_t), sizeof(multicast_info_t));
          
           // Join the multicast group the first time this node is being visited. 
           if(visited_flag == 0) {
                setup_mcast_sockets(multicast_info->multicast_addr, multicast_info->multicast_port);
           }
           // If the last node has not been reached, tour under progress 
           if (tp->curr_pointer < (tp->size -1)) {
                tp->curr_pointer +=1;
                send_tour_packet(sockfd_rt, tp, multicast_info);
	   } else if(tp->curr_pointer == (tp->size-1)){
		   tour_should_end = 1;
	   }
	   if (visited_flag == 0) {
		   dest = &src;
		   // Start pinging
		   signal(SIGALRM, handle_timeout);

		   HWaddr.sll_ifindex = 0;
		   HWaddr.sll_hatype = 0;
		   HWaddr.sll_halen = 0;
		   for (i = 0 ; i<10; i++) {
			   HWaddr.sll_addr[i] = 0;
		   }

		   sockaddrlen = sizeof(struct sockaddr_in);


	
		   sockfd = socket(AF_PACKET, SOCK_RAW, ETH_P_IP);

		   // To ping, we need the dest ip in char format, the dest mac, the source ip in char format, the source mac, the source ifindex. Let's get these things.
	
		   inet_ntop(AF_INET, &(dest->sin_addr), dest_ip, INET_ADDRSTRLEN);
		   alarm(1);
		   if (sigsetjmp(jmpbuf, 1) != 0) {
			   printf("PING %s : %d data bytes\n", dest_ip, DATALEN);
			   areq((struct sockaddr*)dest, sockaddrlen, &HWaddr);
			   do_ping(sockfd, self_ip, dest_ip, self_mac, HWaddr.sll_ifindex, HWaddr.sll_addr);
			   ping_count++;
			   if (ping_count > 5 && tour_should_end) {
				   snprintf(msg, sizeof(msg), "<<<This is node %s.Tour has ended. Group members please identify yourselves>>>\n", node_name);
				   printf("Node %s. Sending: %s\n", node_name, msg);
				   sendto(mcast_udp_sendfd, msg, strlen(msg), 0, (SA*) m_send_addr, salen);
			   }
			   alarm(1);
			   goto repeat;
		   }

                visited_flag = 1;
	   }

        }

	if (FD_ISSET(sockfd_pg, &fd_sset)) {
		process_ping_packet(sockfd_pg);
	}

        if(FD_ISSET(mcast_udp_recvfd, &fd_sset)) {
            recv_mcast_udp(mcast_udp_recvfd, salen);	
            memset(msg, 0, MAX_MSG_LEN);
            snprintf(msg, sizeof(msg), "<<<Node %s. I am a member of the group.>>>\n", node_name);
            printf("Node %s. Sending: %s\n", node_name, msg);
            sendto(mcast_udp_sendfd, msg, strlen(msg), 0, (SA*) m_send_addr, salen);
	    // No more pinging.
	    alarm(0);
	    // Clear the visited flag as this tour is over.
	    visited_flag = 0;
            break;
        }
    }

    for (;;) {                
        FD_ZERO(&fd_sset);
        FD_SET(mcast_udp_recvfd, &fd_sset);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        result = select(mcast_udp_recvfd +1, &fd_sset, NULL, NULL, &tv);
        if(FD_ISSET(mcast_udp_recvfd, &fd_sset)) {
            recv_mcast_udp(mcast_udp_recvfd, salen);	
        }
        else {
            printf("Node %s, Exiting the tour application\n", node_name);
            exit(0);
        }
    }
    return 0;

error: 
    perror("error in select");
    return -1;
}

int get_tour_nodes(int argc, char **argv, char tour_nodes[][IP_ADDR_LEN]) {
    int n = argc;
    struct in_addr **addrList;
    struct hostent *hp;
    int i = 1;
    char *ip = malloc(IP_ADDR_LEN);

    strncpy(tour_nodes[0], self_ip, IP_ADDR_LEN); 
    
    for (i = 1; i < n; i++) {
        hp = gethostbyname(argv[i]);  
        if ( hp == (struct hostent *)NULL ) { //Report lookup failure  
            printf("No IP found corresponding to this domain name!!\n");
            exit(EXIT_FAILURE);
        }
        else {
            addrList = (struct in_addr **)(hp->h_addr_list);
            ip = inet_ntoa(*addrList[0]);
            strncpy(tour_nodes[i], ip, IP_ADDR_LEN);
        }
    }
    return 0;
}

int send_tour_packet(int sockfd_rt, tour_pack_t *tour_pack, multicast_info_t* multicast_info) {
    
    struct ip *iphdr;
    int datalen = 0;
    void *buffer;
    int *ip_flags;
    int status = -1;
    struct sockaddr_in dest;
    
    ip_flags = malloc(sizeof(int) * 4);
    buffer = malloc(IP4_HDRLEN + sizeof(tour_pack_t)+ sizeof(multicast_info_t));

    datalen = sizeof(tour_pack_t) + sizeof(multicast_info_t);
    //printf("The data length is %d\n", datalen);

    iphdr = (struct ip*)buffer;

    // IPv4 header

    // IPv4 header length (4 bits): Number of 32-bit words in header = 5
    iphdr->ip_hl = IP4_HDRLEN / sizeof (uint32_t);

    // Internet Protocol version (4 bits): IPv4
    iphdr->ip_v = 4;

    // Type of service (8 bits)
    iphdr->ip_tos = 0;

    // Total length of datagram (16 bits): IP header + tour_pack
    iphdr->ip_len = IP4_HDRLEN + datalen;

    // ID sequence number (16 bits): unused, since single datagram
    iphdr->ip_id = htons(getpid());

    // Flags, and Fragmentation offset (3, 13 bits): 0 since single datagram

    // Zero (1 bit)
    ip_flags[0] = 0;

    // Do not fragment flag (1 bit)
    ip_flags[1] = 0;

    // More fragments following flag (1 bit)
    ip_flags[2] = 0;

    // Fragmentation offset (13 bits)
    ip_flags[3] = 0;

    iphdr->ip_off = htons ((ip_flags[0] << 15)
            + (ip_flags[1] << 14)
            + (ip_flags[2] << 13)
            +  ip_flags[3]);

    // Time-to-Live (8 bits): default to maximum value
    iphdr->ip_ttl = 255;

    // Transport layer protocol (8 bits): 1 for ICMP
    iphdr->ip_p = PROTO;
    
    // Source IPv4 address (32 bits)
    if ((status = inet_pton (AF_INET, tour_pack->tour_nodes[tour_pack->curr_pointer - 1], &(iphdr->ip_src))) != 1) {
        fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
        exit (EXIT_FAILURE);
    }

    // Destination IPv4 address (32 bits)
    if ((status = inet_pton (AF_INET, tour_pack->tour_nodes[tour_pack->curr_pointer], &(iphdr->ip_dst))) != 1) {
        fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
        exit (EXIT_FAILURE);
    }
    dest.sin_family = AF_INET;
    dest.sin_port = htons(9989);
    dest.sin_addr.s_addr = inet_addr(tour_pack->tour_nodes[tour_pack->curr_pointer]);


    memcpy(buffer + IP4_HDRLEN, tour_pack, sizeof(tour_pack_t));
    memcpy(buffer + IP4_HDRLEN + sizeof(tour_pack_t), multicast_info, sizeof(multicast_info_t));
    // IPv4 header checksum (16 bits): set to 0 when calculating checksum
    iphdr->ip_sum = 0;
    //iphdr->ip_sum = checksum ((uint16_t *) &iphdr, IP4_HDRLEN + datalen);

    if(sendto(sockfd_rt, buffer, iphdr->ip_len, 0, (struct sockaddr *)&dest, sizeof (dest)) < 0) {
            perror("sendto() error");
            exit(-1);
    }
    return 0;

}


int setup_mcast_sockets(char *multicast_addr, char *multicast_port) {

    const int			on = 1;
    struct sockaddr		*m_recv_addr;
    int result = 0;
    //    printf("We'll try to bind ourselves to %s at port %s\n", multicast_addr, multicast_port);
   
    // We need to setup the sending and receiving sockets
    // The sending socket will be bound to the group address and group port.
    // The receiving socket will also be bound to the group address and group port.
    
    salen = sizeof(struct sockaddr_in);
    m_send_addr = malloc(sizeof(struct sockaddr_in));
    memset(m_send_addr, 0, sizeof(struct sockaddr_in));

    m_send_addr->sin_family = AF_INET;
    m_send_addr->sin_port = htons(atoi(multicast_port));
    
    inet_pton(AF_INET, multicast_addr, &(m_send_addr->sin_addr));

    mcast_udp_sendfd = socket(AF_INET, SOCK_DGRAM, 0);
    mcast_udp_recvfd = Socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(mcast_udp_recvfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
   // setsockopt(mcast_udp_recvfd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv, sizeof(struct timeval));

    m_recv_addr = Malloc(salen);
    memcpy(m_recv_addr, m_send_addr, salen);
    result = bind(mcast_udp_recvfd, m_recv_addr, salen);

    if (result < 0) {
	    goto error;
    }
    mcast_join(mcast_udp_recvfd, (SA*)m_send_addr, salen, NULL, 0);
    return 0;

 error:
    perror("Mcast error happened: ");
    return -1;
}

int
mcast_join(int sockfd, const SA *grp, socklen_t grplen,
		   const char *ifname, u_int ifindex)
{
	struct group_req req;
	if (ifindex > 0) {
		req.gr_interface = ifindex;
	} else if (ifname != NULL) {
		if ( (req.gr_interface = if_nametoindex(ifname)) == 0) {
			errno = ENXIO;	/* i/f name not found */
			return(-1);
		}
	} else
		req.gr_interface = 0;
	if (grplen > sizeof(req.gr_group)) {
		errno = EINVAL;
		return -1;
	}
	memcpy(&req.gr_group, grp, grplen);
	return (setsockopt(sockfd, family_to_level(grp->sa_family),
			MCAST_JOIN_GROUP, &req, sizeof(req)));
}

void
recv_mcast_udp(int recvfd, socklen_t salen)
{
    int					n;
    char				line[MAXLINE+1];
    socklen_t			len;
    struct sockaddr		*safrom;

    safrom = malloc(salen);
        len = salen;
        n = recvfrom(recvfd, line, MAXLINE, 0, safrom, &len);
        line[n] = 0;	/* null terminate */
        printf("Node %s. Received: %s\n", node_name, line);
}

