#ifndef __SCHEME_IOREPLAYER__
#define __SCHEME_IOREPLAYER__

#include <sys/types.h>
#include <pthread.h>

#include "arrayqueue.h"

#define PRNG_BUFSZ 64
#define MAX_THREADS 2048
#define MAX_ROW_LENGTH 1024
#define MAX_PATH_LENGTH 512

typedef enum {
  READ_IO = 'R',
  WRITE_IO = 'W'
} iotype_t;

typedef struct {
  queue_t tasks;
  queue_control_t control;
} task_queue_t;

typedef struct {
  char filepath[MAX_PATH_LENGTH];
  iotype_t iotype;
  off_t offset;
  size_t iosize;
  long iteration;
} iotask_t;

typedef struct {
  queue_t finthreads;
  pthread_mutex_t mtx;
  pthread_cond_t cnd;
} cleanup_queue_t;

typedef struct {
  long operated_tasks;
  long read_ops, write_ops;
  long read_byte, write_byte;
} stats_t;

typedef struct {
  pthread_t pt;
  cpu_set_t cpuset;
  char *buf;
  task_queue_t *iotaskque;
  cleanup_queue_t *cleanupque;
  stats_t stats;
} threadconf_t;

#endif // __SCHEME_IOREPLAYER__
