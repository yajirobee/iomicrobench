#ifndef __SCHEME_IOMICROBENCH__
#define __SCHEME_IOMICROBENCH__

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>

#define PRNG_BUFSZ 64

#define TV2USEC(tv) (((double) (tv).tv_sec) * 1000000.0 + (double) (tv).tv_usec)
#define GETTIMEOFDAY(tv_ptr)                                    \
    {                                                           \
        if (0 != gettimeofday(tv_ptr, NULL)) {                  \
            perror("gettimeofday(3) failed ");                  \
            fprintf(stderr, " @%s:%d\n", __FILE__, __LINE__);   \
        }                                                       \
    }

typedef struct{
  int fd;
  char *buf;
  long iosize, iterate;
  double stime, ftime;
  cpu_set_t cpuset;
} seqread_t;

typedef struct{
  int fd;
  char *buf;
  long iosize, iterate;
  struct random_data random_states;
  char statebuf[PRNG_BUFSZ];
  long seekmax;
  double stime, ftime;
  cpu_set_t cpuset;
} randread_t;

long procsuffix(char *);

#endif // __SCHEME_IOMICROBENCH__
