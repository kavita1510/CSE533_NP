/* Our own header for the programs that need interface configuration info.
   Include this file, instead of "unp.h". */

#ifndef	__unp_ifi_plus_h
#define	__unp_ifi_plus_h

#include	"unp.h"
#include	"fup.h"
#include	<net/if.h>

#define	IFI_NAME	16			/* same as IFNAMSIZ in <net/if.h> */
#define	IFI_HADDR	 8			/* allow for 64-bit EUI-64 in future */
#define MAX_INTERFACES	 5

struct ifi_info {
  char    ifi_name[IFI_NAME];	/* interface name, null-terminated */
  short   ifi_index;			/* interface index */
  short   ifi_mtu;				/* interface MTU */
  u_char  ifi_haddr[IFI_HADDR];	/* hardware address */
  u_short ifi_hlen;				/* # bytes in hardware address: 0, 6, 8 */
  short   ifi_flags;			/* IFF_xxx constants from <net/if.h> */
  short   ifi_myflags;			/* our own IFI_xxx flags */
  struct sockaddr  *ifi_addr;	/* primary address */
  struct sockaddr  *ifi_brdaddr;/* broadcast address */
  struct sockaddr  *ifi_dstaddr;/* destination address */

/*================ cse 533 Assignment 2 modifications =================*/

  struct sockaddr  *ifi_ntmaddr;  /* netmask address */

/*=====================================================================*/

  struct ifi_info  *ifi_next;	/* next of these structures */
};

/* Contains info about the interfaces */
typedef struct interfaces_t
{
	int sockfd;
	struct sockaddr_in *ip_addr;
	struct sockaddr_in *net_mask;
	struct sockaddr_in *subnet_addr;
} interfaces_t;

#define	IFI_ALIAS	1			/* ifi_addr is an alias */

					/* function prototypes */
struct ifi_info	*get_ifi_info_plus(int, int);
struct ifi_info	*Get_ifi_info_plus(int, int);
void   free_ifi_info_plus(struct ifi_info *);
int get_interfaces(interfaces_t**);
#endif	/* __unp_ifi_plus_h */
