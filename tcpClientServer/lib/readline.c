/* include readline1 */
#include	"common.h"

static pthread_key_t	rl_key;
static pthread_once_t	rl_once = PTHREAD_ONCE_INIT;

void
Pthread_setspecific(pthread_key_t key, const void *value)
{
        int             n;

        if ( (n = pthread_setspecific(key, value)) == 0)
                return;
        errno = n;
        perror("pthread_setspecific error");
	exit(1);
}

void
Pthread_once(pthread_once_t *ptr, void (*func)(void))
{
        int             n;

        if ( (n = pthread_once(ptr, func)) == 0)
                return;
        errno = n;
        perror("pthread_once error");
	exit(errno);
}

void
Pthread_key_create(pthread_key_t *key, void (*func)(void *))
{
        int             n;

        if ( (n = pthread_key_create(key, func)) == 0)
                return;
        errno = n;
        perror("pthread_key_create error");
	exit(errno);
}

static void
readline_destructor(void *ptr)
{
	free(ptr);
}

static void
readline_once(void)
{
	Pthread_key_create(&rl_key, readline_destructor);
}

typedef struct {
  int	 rl_cnt;			/* initialize to 0 */
  char	*rl_bufptr;			/* initialize to rl_buf */
  char	 rl_buf[MAXLINE];
} Rline;
/* end readline1 */

/* include readline2 */
static ssize_t
my_read(Rline *tsd, int fd, char *ptr)
{
	if (tsd->rl_cnt <= 0) {
again:
		if ( (tsd->rl_cnt = read(fd, tsd->rl_buf, MAXLINE)) < 0) {
			if (errno == EINTR)
				goto again;
			return(-1);
		} else if (tsd->rl_cnt == 0)
			return(0);
		tsd->rl_bufptr = tsd->rl_buf;
	}

	tsd->rl_cnt--;
	*ptr = *tsd->rl_bufptr++;
	return(1);
}

ssize_t
readline(int fd, void *vptr, size_t maxlen)
{
	size_t		n, rc;
	char	c, *ptr;
	Rline	*tsd;

	Pthread_once(&rl_once, readline_once);
	if ( (tsd = pthread_getspecific(rl_key)) == NULL) {
		tsd = calloc(1, sizeof(Rline));		/* init to 0 */
		if( tsd == NULL)
		    { perror("Calloc error"); exit(errno);}
		Pthread_setspecific(rl_key, tsd);
	}

	ptr = vptr;
	for (n = 1; n < maxlen; n++) {
		if ( (rc = my_read(tsd, fd, &c)) == 1) {
			*ptr++ = c;
			if (c == '\n')
				break;
		} else if (rc == 0) {
			*ptr = 0;
			return(n - 1);		/* EOF, n - 1 bytes read */
		} else
			return(-1);		/* error, errno set by read() */
	}

	*ptr = 0;
	return(n);
}
/* end readline2 */
