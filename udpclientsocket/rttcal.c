/* include rtt1 */
#include	"rttcal.h"
#include "fup.h"
int		rtt_d_flag_plus = 0;		/* debug flag; can be set by caller */

/*
 * Calculate the RTO value based on current estimators:
 *		smoothed RTT plus four times the deviation
 */
#define	RTT_RTOCALC(ptr) ((ptr)->rtt_srtt + (4.0 * (ptr)->rtt_rttvar))

static int
rtt_minmax(int rto)
{
	if (rto < RTT_RXTMIN)
		rto = RTT_RXTMIN;
	else if (rto > RTT_RXTMAX)
		rto = RTT_RXTMAX;
	return(rto);
}

void
rtt_init_plus(struct rtt_info_plus *ptr)
{
	struct timeval	tv;

	Gettimeofday(&tv, NULL);
	ptr->rtt_base = tv.tv_sec;		/* # sec since 1/1/1970 at start */

	ptr->rtt_rtt    = 0;
	ptr->rtt_srtt   = 0;
	ptr->rtt_rttvar = 750;
	ptr->rtt_rto = rtt_minmax(RTT_RTOCALC(ptr));
		/* first RTO at (srtt + (4 * rttvar)) = 3000 milliseconds */
}
/* end rtt1 */

/*
 * Return the current timestamp.
 * Our timestamps are 32-bit integers that count milliseconds since
 * rtt_init() was called.
 */

/* include rtt_ts */
uint32_t
rtt_ts_plus(struct rtt_info_plus *ptr)
{
	uint32_t ts;
	struct timeval	tv;

	Gettimeofday(&tv, NULL);
	ts = ((tv.tv_sec - ptr->rtt_base) * 1000) + (tv.tv_usec / 1000);
	LOG("TS value set to %d\n", ts);
	return(ts);
}

int
rtt_start_plus(struct rtt_info_plus *ptr)
{
	return(rtt_minmax(ptr->rtt_rto));		
		/* 4return value can be used as: alarm(rtt_start(&foo)) */
}
/* end rtt_ts */

/*
 * A response was received.
 * Stop the timer and update the appropriate values in the structure
 * based on this packet's RTT.  We calculate the RTT, then update the
 * estimators of the RTT and its mean deviation.
 * This function should be called right after turning off the
 * timer with alarm(0), or right after a timeout occurs.
 */

/* include rtt_stop */
void
rtt_stop_plus(struct rtt_info_plus *ptr, uint32_t ms)
{
	double		delta;

	//ptr->rtt_rtt = ms / 1000.0;		/* measured RTT in seconds */
	ptr->rtt_rtt = ms ;		/* measured RTT in milliseconds */

	/*
	 * Update our estimators of RTT and mean deviation of RTT.
	 * See Jacobson's SIGCOMM '88 paper, Appendix A, for the details.
	 */

	ptr->rtt_rtt -= (ptr->rtt_srtt >> 3);
	LOG("ptr->rtt_rtt %d\n", ptr->rtt_rtt);
	ptr->rtt_srtt += ptr->rtt_rtt;
	LOG("ptr->rtt_srtt %d\n", ptr->rtt_srtt);
	if(ptr->rtt_rtt < 0)
	    ptr->rtt_rtt = -ptr->rtt_rtt;
	ptr->rtt_rtt -=(ptr->rtt_rttvar >> 2);
	LOG("ptr->rtt_rtt %d\n", ptr->rtt_rtt);
	ptr->rtt_rttvar += ptr->rtt_rtt;
	LOG("ptr->rtt_rttvar %d\n", ptr->rtt_rttvar);
	ptr->rtt_rto = (ptr->rtt_srtt >> 3) + ptr->rtt_rttvar;
	LOG("ptr->rtt_rto %d\n", ptr->rtt_rto);
    
/*
	delta = ptr->rtt_rtt - ptr->rtt_srtt;
	ptr->rtt_srtt += delta / 8;		//g = 1/8 

	if (delta < 0.0)
		delta = -delta;				// |delta| 

	ptr->rtt_rttvar += (delta - ptr->rtt_rttvar) / 4;	// h = 1/4
*/
	LOG("RTO after recalculation of estimators is %d\n", rtt_minmax(ptr->rtt_rto));
}
/* end rtt_stop */

/*
 * A timeout has occurred.
 * Return -1 if it's time to give up, else return 0.
 */

/* include rtt_timeout */
int
rtt_timeout_plus(struct rtt_info_plus *ptr)
{
	ptr->rtt_rto *= 2;		/* next RTO */
	rtt_minmax(ptr->rtt_rto);
	LOG("After exponential backoff, RTO value being retuned is %d\n", rtt_minmax(ptr->rtt_rto));
	return(0);
}
/* end rtt_timeout */

/*
 * Print debugging information on stderr, if the "rtt_d_flag" is nonzero.
 */

void
rtt_debug_plus(struct rtt_info_plus *ptr)
{
	if (rtt_d_flag_plus == 0)
		return;

	fprintf(stderr, "rtt = %d, srtt = %d, rttvar = %d, rto = %d\n",
			ptr->rtt_rtt, ptr->rtt_srtt, ptr->rtt_rttvar, ptr->rtt_rto);
	fflush(stderr);
}
