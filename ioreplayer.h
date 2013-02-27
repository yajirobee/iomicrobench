#ifndef __SCHEME_IOREPLAYER__
#define __SCHEME_IOREPLAYER__

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#define CPUCORES 32
#define BLOCK_SIZE 512
#define PRNG_BUFSZ 64
#define QUE_SIZE 1024
#define MAX_STRING 128
#define OPEN_FLG_R O_RDONLY | O_DIRECT

typedef struct{
  off_t offset;
} rinfo_t;

typedef struct{
  rinfo_t *a;
  int limit, size, head, tail;
  pthread_mutex_t mtx;
  pthread_cond_t more, less;
} queue_t;

void initque(queue_t *que, size_t limit);
void delque(queue_t *que);
void push(queue_t *que, rinfo_t *readinfo);
rinfo_t pop(queue_t *que);

typedef struct{
  int nwait, nthread;
  pthread_mutex_t mtx;
  pthread_cond_t cnd;
} waitmng_t;

typedef struct{
  pthread_t pt;
  cpu_set_t cpuset;
  int fd;
  char *buf;
  size_t iosize;
  queue_t *rinfoque;
  waitmng_t *waitmng;
} tskcnf_t;

#endif // __SCHEME_IOREPLAYER__
