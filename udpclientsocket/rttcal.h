#ifndef	__rtt_h
#define	__rtt_h

#include	"unp.h"

struct rtt_info_plus {
  int		rtt_rtt;
  int		rtt_srtt;
  int		rtt_rttvar;
  int		rtt_rto;
  int		rtt_nrexmt;
  uint32_t	rtt_base;

};

#define	RTT_RXTMIN      1000	/* min retransmit timeout value, in milliseconds */
#define	RTT_RXTMAX      3000	/* max retransmit timeout value, in milliseconds */
#define	RTT_MAXNREXMT 	12	/* max # times to retransmit */

				/* function prototypes */
void	 rtt_debug_plus(struct rtt_info_plus *);
void	 rtt_init_plus(struct rtt_info_plus *);
void	 rtt_newpack_plus(struct rtt_info_plus *);
int		 rtt_start_plus(struct rtt_info_plus *);
void	 rtt_stop_plus(struct rtt_info_plus *, uint32_t);
int		 rtt_timeout_plus(struct rtt_info_plus *);
uint32_t rtt_ts_plus(struct rtt_info_plus *);

extern int	rtt_d_flag_plus;	/* can be set to nonzero for addl info */

#endif	/* __unp_rtt_h */
