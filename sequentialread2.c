#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include "iomicrobench2.h"

void sequential_read(tskcnf_t *cnf){
  rinfo_t *rinfo;

  // set affinity
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cnf->cpuset) != 0){
    perror("pthread_setaffinity_np()");
    exit(1);
  }

  // perform sequential read
  while (1){
    pthread_mutex_lock(cnf->que_mtx);
    while (cnf->rinfoque->size == 0){ pthread_cond_wait(cnf->que_cnd, cnf->que_mtx); }
    rinfo = pop(cnf->rinfoque);
    pthread_mutex_unlock(cnf->que_mtx);

    assert(rinfo->iterate > rinfo->count);

    read(rinfo->fd, cnf->buf, cnf->iosize);
    rinfo->count++;

    if (rinfo->iterate == rinfo->count){
      pthread_mutex_lock(cnf->ftsk_mtx);
      (*cnf->nftsk)++;
      pthread_cond_signal(cnf->ftsk_cnd);
      pthread_mutex_unlock(cnf->ftsk_mtx);
    }
    else {
      pthread_mutex_lock(cnf->que_mtx);
      push(cnf->rinfoque, rinfo);
      pthread_cond_signal(cnf->que_cnd);
      pthread_mutex_unlock(cnf->que_mtx);
    }
  }
}

int main(int argc, char **argv){
  int i;
  int nthread;
  long iosize, iterate, fsize;
  int fd;
  pthread_t *pt;
  tskcnf_t *tskcnfs;
  rinfo_t *rinfos;
  queue_t rinfoque;
  pthread_mutex_t que_mtx = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t que_cnd = PTHREAD_COND_INITIALIZER;
  int nftsk = 0;
  pthread_mutex_t ftsk_mtx = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t ftsk_cnd = PTHREAD_COND_INITIALIZER;
  struct timeval stime, ftime;
  double elatime, mbps, iops, latency = 0.0;

  if (argc != 5){
    printf("Usage : %s filepath iosize iterate nthread\n", argv[0]);
    exit(1);
  }
  iosize = atol(argv[2]);
  iterate = atol(argv[3]);
  nthread = atoi(argv[4]);
  assert(iosize % BLOCK_SIZE == 0);

  // check file size
  if ((fd = open(argv[1], O_RDONLY)) < 0){
    perror("open");
    exit(1);
  }
  if ((fsize = lseek(fd, 0, SEEK_END)) < 0){
    perror("lseek");
    exit(1);
  }
  printf("size of %s = %ld\n", argv[1], fsize);
  close(fd);
  assert(fsize >= (iosize * iterate * nthread));

  // init queue
  initque(&rinfoque, nthread);

  // allocate memory for pthread_t
  if (posix_memalign((void **)&pt, BLOCK_SIZE, sizeof(pthread_t) * nthread) != 0){
    perror("posix_memalign");
    exit(1);
  }

  // allocate memory for readinfo
  if (posix_memalign((void **)&rinfos, BLOCK_SIZE, sizeof(rinfo_t) * nthread) != 0){
    perror("posix_memalign");
    exit(1);
  }

  // allocate memory for task configuration
  if (posix_memalign((void **)&tskcnfs, BLOCK_SIZE, sizeof(tskcnf_t) * nthread) != 0){
    perror("posix_memalign");
    exit(1);
  }

  // set readinfo and task configuration
  for (i = 0; i < nthread; i++){
    int j;
    // open file
    if((rinfos[i].fd = open(argv[1], OPEN_FLG_R)) < 0){
      perror("open");
      exit(1);
    }
    // seek to assigned place
    if (lseek(rinfos[i].fd, i * iosize * iterate, SEEK_SET) < 0){
      perror("lseek");
      exit(1);
    }
    rinfos[i].iterate = iterate;
    rinfos[i].count = 0;
    push(&rinfoque, rinfos + i);

    // allocate buffer aligned by BLOCK_SIZE
    if (posix_memalign((void **)&tskcnfs[i].buf, BLOCK_SIZE, iosize) != 0){
      perror("posix_memalign");
      exit(1);
    }
    // set cpuset
    CPU_ZERO(&tskcnfs[i].cpuset);
    for (j = 0; j < CPUCORES; j++){ CPU_SET(j, &tskcnfs[i].cpuset); }
    tskcnfs[i].iosize = iosize;
    tskcnfs[i].rinfoque = &rinfoque;
    tskcnfs[i].que_mtx = &que_mtx;
    tskcnfs[i].ftsk_mtx = &ftsk_mtx;
    tskcnfs[i].que_cnd = &que_cnd;
    tskcnfs[i].ftsk_cnd = &ftsk_cnd;
    tskcnfs[i].nftsk = &nftsk;
  }

  // sequential read
  printf("sequential read\n");
  gettimeofday(&stime, NULL);
  for (i = 0; i < nthread; i++){
    pthread_create(&pt[i], NULL,
                   (void *(*)(void *))sequential_read, (void *)(tskcnfs + i));
  }
  pthread_mutex_lock(&ftsk_mtx);
  while (nftsk < nthread){ pthread_cond_wait(&ftsk_cnd, &ftsk_mtx); }
  pthread_mutex_unlock(&ftsk_mtx);
  gettimeofday(&ftime, NULL);

  // get profiled information
  elatime = ((ftime.tv_sec - stime.tv_sec) * 1000000.0 +
             (ftime.tv_usec - stime.tv_usec));
  mbps = (iosize * iterate * nthread) / elatime;
  iops = (iterate * nthread) / (elatime / 1000000);
  latency = elatime / (iterate * nthread);
  printf("stime = %ld.%06d\n",
         (unsigned long)stime.tv_sec, (unsigned int)stime.tv_usec);
  printf("ftime = %ld.%06d\n",
         (unsigned long)ftime.tv_sec, (unsigned int)ftime.tv_usec);
  printf("elapsed = %.1f(us)\nmbps = %f(MB/s)\niops = %f(io/s)\nlatency = %f(us)\n",
         elatime, mbps, iops, latency);

  for (i = 0; i < nthread; i++){
    close(rinfos[i].fd);
    free(tskcnfs[i].buf);
  }
  free(tskcnfs);
  free(rinfos);
  delque(&rinfoque);
  free(pt);
  return 0;
}
