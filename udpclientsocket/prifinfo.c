#include "unpifi_plus.h"
#include <inttypes.h>
#include <math.h>

extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern        void      free_ifi_info_plus(struct ifi_info *ifihead);

int get_interfaces(interfaces_t **interfaces)
{

        struct ifi_info *ifi, *ifihead;
        struct sockaddr_in *sa;
        u_char *ptr;
        int i = 0, j = 0;

        printf("\n-------------------------- Interface Information -------------------------\n");
        for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1);
             ifi != NULL; ifi = ifi->ifi_next) {


                interfaces[i] = (struct interfaces_t*)malloc(sizeof(interfaces_t));
                printf("%s: ", ifi->ifi_name);
                if (ifi->ifi_index != 0)
                        printf("(%d) ", ifi->ifi_index);
                printf("<");
                /* *INDENT-OFF* */
                if (ifi->ifi_flags & IFF_UP)            printf("UP ");
                if (ifi->ifi_flags & IFF_BROADCAST)     printf("BCAST ");
                if (ifi->ifi_flags & IFF_MULTICAST)     printf("MCAST ");
                if (ifi->ifi_flags & IFF_LOOPBACK)      printf("LOOP ");
                if (ifi->ifi_flags & IFF_POINTOPOINT)   printf("P2P ");
                printf(">\n");
                /* *INDENT-ON* */

                if ( (j = ifi->ifi_hlen) > 0) {
                        ptr = ifi->ifi_haddr;
                        do {
                                printf("%s%x", (j == ifi->ifi_hlen) ? "  " : ":", *ptr++);
                        } while (--j > 0);
                        printf("\n");
                }
	    
	       if ( (sa = (struct sockaddr_in*)ifi->ifi_addr) != NULL) {
                        interfaces[i]->ip_addr = malloc(sizeof(struct sockaddr_in));
                        interfaces[i]->ip_addr->sin_addr.s_addr = sa->sin_addr.s_addr;
                        printf(" IP Address: %s\n", inet_ntoa((interfaces[i]->ip_addr)->sin_addr));
                        sa = NULL;
                }

                if ( (sa = (struct sockaddr_in*)ifi->ifi_ntmaddr) != NULL) {
                        interfaces[i]->net_mask = malloc(sizeof(struct sockaddr_in));
                        interfaces[i]->net_mask->sin_addr.s_addr = sa->sin_addr.s_addr;
                        printf(" Net Mask: %s\n", inet_ntoa((interfaces[i]->net_mask)->sin_addr));
                        sa = NULL;
                }

                sa = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
                sa->sin_addr.s_addr = ((interfaces[i]->ip_addr)->sin_addr.s_addr) & ((interfaces[i]->net_mask)->sin_addr.s_addr);
                interfaces[i]->subnet_addr = malloc(sizeof(struct sockaddr_in));
                interfaces[i]->subnet_addr->sin_addr.s_addr = sa->sin_addr.s_addr;
                printf(" Subnet Address: %s\n", inet_ntoa((interfaces[i]->subnet_addr)->sin_addr));

                i++;

        }
        free_ifi_info_plus(ifihead);
        return i;
}

