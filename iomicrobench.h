#ifndef __SCHEME_IOMICROBENCH__
#define __SCHEME_IOMICROBENCH__

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>

#define PRNG_BUFSZ 64

#define TS2SEC(ts) (((double) (ts).tv_sec) + ((double) (ts).tv_nsec * 1e-09))
#define TIMEINTERVAL_SEC(sts, fts)                                      \
  ((fts.tv_sec - sts.tv_sec) + (fts.tv_nsec - sts.tv_nsec) * 1e-09)
#define CLOCK_GETTIME(ts_ptr)                                    \
  {                                                              \
    if (clock_gettime(CLOCK_REALTIME, ts_ptr) != 0) {            \
      perror("clock_gettime(3) failed ");                        \
      fprintf(stderr, " @%s:%d\n", __FILE__, __LINE__);          \
    }                                                            \
  }

typedef struct{
  int fd;
  char *buf;
  double stime, ftime;
  long ops;
  cpu_set_t cpuset;
} seqread_t;

typedef struct{
  int fd;
  char *buf;
  double stime, ftime;
  long ops;
  cpu_set_t cpuset;
  struct random_data random_states;
  char statebuf[PRNG_BUFSZ];
} randread_t;

long procsuffix(char *);

#endif // __SCHEME_IOMICROBENCH__
