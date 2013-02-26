#ifndef __SCHEME_IOMICROBENCH2__
#define __SCHEME_IOMICROBENCH2__

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#define CPUCORES 32
#define BLOCK_SIZE 512
#define PRNG_BUFSZ 64
#define OPEN_FLG_R O_RDONLY | O_DIRECT

typedef struct{
  int fd;
  long iterate, count;
  struct random_data random_states; // following variables are used by random io
  char statebuf[PRNG_BUFSZ];
  long seekmax;
} rinfo_t;

typedef struct{
  rinfo_t **a;
  int limit, size, head, tail;
} queue_t;

typedef struct{
  cpu_set_t cpuset;
  char *buf;
  long iosize;
  queue_t *rinfoque;
  pthread_mutex_t *que_mtx, *ftsk_mtx;
  pthread_cond_t *que_cnd, *ftsk_cnd;
  int *nftsk;
} tskcnf_t;

void initque(queue_t *que, size_t limit);
void delque(queue_t *que);
void push(queue_t *que, rinfo_t *readinfo);
rinfo_t *pop(queue_t *que);

#endif // __SCHEME_IOMICROBENCH__
