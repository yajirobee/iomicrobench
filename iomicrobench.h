#ifndef __SCHEME_IOMICROBENCH__
#define __SCHEME_IOMICROBENCH__

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PRNG_BUFSZ 64

typedef struct{
  int fd;
  char *buf;
  long iosize, iterate;
  struct timeval stime, ftime;
  cpu_set_t cpuset;
} seqread_t;

typedef struct{
  int fd;
  char *buf;
  long iosize, iterate;
  struct random_data random_states;
  char statebuf[PRNG_BUFSZ];
  long seekmax;
  struct timeval stime, ftime;
  cpu_set_t cpuset;
} randread_t;

long procsuffix(char *);

#endif // __SCHEME_IOMICROBENCH__
