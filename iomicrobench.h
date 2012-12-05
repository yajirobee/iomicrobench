#ifndef __SCHEME_IOMICROBENCH__
#define __SCHEME_IOMICROBENCH__

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define CPUCORES 32
#define BLOCK_SIZE 512
#define PRNG_BUFSZ 64
#define OPEN_FLG_R O_RDONLY | O_DIRECT

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

void sequential_read(seqread_t *readinfo);
void random_read(randread_t *readinfo);

#endif // __SCHEME_IOMICROBENCH__
