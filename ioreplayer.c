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
#include "ioreplayer.h"

//
// implement queue
//

void initque(queue_t *que, size_t limit){
  if ((que->a = (rinfo_t *)calloc(limit, sizeof(rinfo_t))) == NULL){
    perror("malloc");
    exit(0);
  }
  que->limit = limit;
  que->head = 0;
  que->tail = 0;
  que->size = 0;
  pthread_mutex_init(&que->mtx, NULL);
  pthread_cond_init(&que->more, NULL);
  pthread_cond_init(&que->less, NULL);
}

void delque(queue_t *que){
  free(que->a);
}

void push(queue_t *que, rinfo_t *rinfo){
  assert(que->limit > que->size);
  que->a[que->tail++] = *rinfo;
  if (que->tail >= que->limit){ que->tail = 0; } // que->tail %= que->limit;
  que->size++;
}

rinfo_t pop(queue_t *que){
  rinfo_t head;

  assert(que->size > 0);
  head = que->a[que->head++];
  if (que->head >= que->limit){ que->head = 0; } // que->head %= que->limit;
  que->size--;
  return head;
}

//
// replaying read operation on multiple threads
//

void reader(tskcnf_t *cnf){
  rinfo_t rinfo;

  // set affinity
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cnf->cpuset) != 0){
    perror("pthread_setaffinity_np()");
    exit(1);
  }

  // perform read operation
  while (1){
    pthread_mutex_lock(&cnf->rinfoque->mtx);
    if (cnf->rinfoque->size == 0){
      if (++cnf->waitmng->nwait == cnf->waitmng->nthread){
        pthread_cond_signal(&cnf->waitmng->cnd);
      }
      do {
        pthread_cond_wait(&cnf->rinfoque->more, &cnf->rinfoque->mtx);
        if (cnf->waitmng->nwait == -1){
          pthread_mutex_unlock(&cnf->rinfoque->mtx);
          pthread_exit(NULL);
        }
      } while (cnf->rinfoque->size == 0);
      --cnf->waitmng->nwait;
    }
    rinfo = pop(cnf->rinfoque);
    pthread_cond_signal(&cnf->rinfoque->less);
    pthread_mutex_unlock(&cnf->rinfoque->mtx);

    pread(cnf->fd, cnf->buf, cnf->iosize, rinfo.offset);
  }
}

int getnext(FILE *fp, rinfo_t *rinfo){
  static int count = 0;
  off_t offset;
  char buf[MAX_STRING];

  if (rinfo == NULL){ return count; }
  if (fgets(buf, MAX_STRING, fp) != NULL){
    count++;
    offset = atol(buf);
    rinfo->offset = offset;
    return count;
  }
  else {
    return -1;
  }
}

void read_replayer(int nthread, tskcnf_t *tskcnfs, FILE *fp){
  int i;
  rinfo_t rinfo;
  queue_t *que = tskcnfs[0].rinfoque;
  waitmng_t *waitmng = tskcnfs[0].waitmng;

  // create reader threads
  for (i = 0; i < nthread; i++){
    pthread_create(&tskcnfs[i].pt, NULL,
                   (void *(*)(void *))reader, (void *)&tskcnfs[i]);
  }

  for (i = getnext(fp, &rinfo); i >= 0; i = getnext(fp, &rinfo)){
    pthread_mutex_lock(&que->mtx);
    while (que->size >= que->limit){ pthread_cond_wait(&que->less, &que->mtx); }
    push(que, &rinfo);
    pthread_cond_signal(&que->more);
    pthread_mutex_unlock(&que->mtx);
  }
  pthread_mutex_lock(&waitmng->mtx);
  while (waitmng->nwait < waitmng->nthread){
    pthread_cond_wait(&waitmng->cnd, &waitmng->mtx);
  }
  pthread_mutex_unlock(&waitmng->mtx);
  waitmng->nwait = -1;
  pthread_mutex_lock(&que->mtx);
  pthread_cond_broadcast(&que->more);
  pthread_mutex_unlock(&que->mtx);
  for (i = 0; i < nthread; i++){
    pthread_join(tskcnfs[i].pt, NULL);
  }
}

int main(int argc, char **argv){
  int i, count = 0;
  int nthread;
  int fd;
  off_t iosize, fsize, seekmax;
  int mode;
  FILE *fp;
  queue_t rinfoque;
  waitmng_t waitmng;
  tskcnf_t *tskcnfs;
  struct timeval stime, ftime;
  double elatime, mbps, iops, latency = 0.0;

  if (argc != 6){
    printf("Usage : %s filepath iosize nthread mode (iterate|offsetlistpath)\n", argv[0]);
    exit(1);
  }
  iosize = atol(argv[2]);
  nthread = atoi(argv[3]);
  mode = atoi(argv[4]);
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

  // set speekmax
  if (((fsize - iosize) / BLOCK_SIZE) <= RAND_MAX){
    seekmax = (fsize - iosize) / BLOCK_SIZE;
  }
  else{
    seekmax = RAND_MAX;
  }

  // init queue
  initque(&rinfoque, QUE_SIZE);

  // init counter for cheking whether each threads wait for next task
  waitmng.nwait = 0;
  waitmng.nthread = nthread;
  pthread_mutex_init(&waitmng.mtx, NULL);
  pthread_cond_init(&waitmng.cnd, NULL);

  // allocate memory for task configuration
  if (posix_memalign((void **)&tskcnfs, BLOCK_SIZE, sizeof(tskcnf_t) * nthread) != 0){
    perror("posix_memalign");
    exit(1);
  }

  // set readinfo and task configuration
  for (i = 0; i < nthread; i++){
    int j;

    // set cpuset
    CPU_ZERO(&tskcnfs[i].cpuset);
    for (j = 0; j < CPUCORES; j++){ CPU_SET(j, &tskcnfs[i].cpuset); }

    // open file
    if((tskcnfs[i].fd = open(argv[1], OPEN_FLG_R)) < 0){
      perror("open");
      exit(1);
    }

    // allocate buffer aligned by BLOCK_SIZE
    if (posix_memalign((void **)&tskcnfs[i].buf, BLOCK_SIZE, iosize) != 0){
      perror("posix_memalign");
      exit(1);
    }
    tskcnfs[i].iosize = iosize;
    tskcnfs[i].rinfoque = &rinfoque;
    tskcnfs[i].waitmng = &waitmng;
  }

  if (mode == 1){ // sequential read
    int iterate = atoi(argv[5]);
    if ((fp = tmpfile()) == NULL){
      perror("tmpfile");
      exit(1);
    }
    for (i = 0; i < iterate; i++){ fprintf(fp, "%ld\n", iosize * i); }
    rewind(fp);
  }
  else if (mode == 2){ // random read
    int iterate = atoi(argv[5]);
    if ((fp = tmpfile()) == NULL){
      perror("tmpfile");
      exit(1);
    }
    for (i = 0; i < iterate; i++){
      fprintf(fp, "%ld\n", (random() % seekmax) * BLOCK_SIZE);
    }
    rewind(fp);
  }
  else if (mode == 3){ // replay read operation
    if ((fp = fopen(argv[5], "r")) == NULL){
      perror("fopen");
      exit(1);
    }
  }
  else {
    fprintf(stderr, "wrong mode number : %d\n", mode);
    exit(1);
  }

  // perform read
  printf("started measurement\n");
  gettimeofday(&stime, NULL);
  read_replayer(nthread, tskcnfs, fp);
  gettimeofday(&ftime, NULL);

  // get profile
  count = getnext(NULL, NULL);
  elatime = ((ftime.tv_sec - stime.tv_sec) * 1000000.0 +
             (ftime.tv_usec - stime.tv_usec));
  mbps = (iosize * count) / elatime;
  iops = count / (elatime / 1000000);
  latency = elatime / count;
  printf("stime = %ld.%06d\n",
         (unsigned long)stime.tv_sec, (unsigned int)stime.tv_usec);
  printf("ftime = %ld.%06d\n",
         (unsigned long)ftime.tv_sec, (unsigned int)ftime.tv_usec);
  printf("count = %d\nelapsed = %.1f(us)\nmbps = %f(MB/s)\niops = %f(io/s)\nlatency = %f(us)\n",
         count, elatime, mbps, iops, latency);

  // release resources
  fclose(fp);
  for (i = 0; i < nthread; i++){
    free(tskcnfs[i].buf);
    close(tskcnfs[i].fd);
  }
  free(tskcnfs);
  delque(&rinfoque);
  return 0;
}
