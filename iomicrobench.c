#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include "iomicrobench.h"

void
sequential_read(seqread_t *readinfo)
{
  int i;

  //set affinity
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &readinfo->cpuset) != 0) {
    perror("pthread_setaffinity_np()");
    exit(1);
  }

  gettimeofday(&readinfo->stime, NULL);
  for (i = 0; i < readinfo->iterate; i++) {
    read(readinfo->fd, readinfo->buf, readinfo->iosize);
  }
  gettimeofday(&readinfo->ftime, NULL);
}

void
random_read(randread_t *readinfo)
{
  int i, tmp;

  //set affinity
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &readinfo->cpuset) != 0) {
    perror("pthread_setaffinity_np()");
    exit(1);
  }

  gettimeofday(&readinfo->stime, NULL);
  for (i = 0; i < readinfo->iterate; i++) {
    random_r(&readinfo->random_states, &tmp);
    pread(readinfo->fd, readinfo->buf, readinfo->iosize,
          (tmp % readinfo->seekmax) * BLOCK_SIZE);
  }
  gettimeofday(&readinfo->ftime, NULL);
}
