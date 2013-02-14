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
#include "iomicrobench.h"

int main(int argc, char **argv){
  int i;
  int nthread;
  long iosize, iterate;
  int fd;
  long fsize, seekmax;
  pthread_t *pt;
  randread_t *readinfos;
  struct timeval stime, ftime;
  double elatime, mbps, iops, latency = 0.0;

  if (argc == 5){
    iosize = atol(argv[2]);
    iterate = atol(argv[3]);
    nthread = atoi(argv[4]);
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
  }
  else if (argc == 6){
    iosize = atol(argv[2]);
    iterate = atol(argv[3]);
    nthread = atoi(argv[4]);
    fsize = atol(argv[5]);
  }
  else{
    printf("Usage : %s filepath iosize iterate nthread [fsize]\n", argv[0]);
    exit(1);
  }
  assert(iosize % BLOCK_SIZE == 0);
  assert(fsize >= iosize);

  //set seekmax
  if (((fsize - iosize) / BLOCK_SIZE) <= RAND_MAX){
    seekmax = (fsize - iosize) / BLOCK_SIZE;
  }
  else{
    seekmax = RAND_MAX;
  }

  // allocate memory for pthread_t
  if (posix_memalign((void **)&pt, BLOCK_SIZE, sizeof(pthread_t) * nthread) != 0){
    perror("posix_memalign");
    exit(1);
  }

  // allocate memory for readinfo
  if (posix_memalign((void **)&readinfos, BLOCK_SIZE, sizeof(randread_t) * nthread) != 0){
    perror("posix_memalign");
    exit(1);
  }

  // set readinfo
  for (i = 0; i < nthread; i++){
    int j;
    // allocate buffer aligned by BLOCK_SIZE
    if (posix_memalign((void **)&readinfos[i].buf, BLOCK_SIZE, iosize) != 0){
      perror("posix_memalign");
      exit(1);
    }
    //open file
    if((readinfos[i].fd = open(argv[1], OPEN_FLG_R)) < 0){
      perror("open");
      exit(1);
    }
    // set cpuset
    CPU_ZERO(&readinfos[i].cpuset);
    for (j = 0; j < CPUCORES; j++){ CPU_SET(j, &readinfos[i].cpuset); }

    readinfos[i].iosize = iosize;
    readinfos[i].iterate = iterate;
    initstate_r(i + 1, readinfos[i].statebuf, PRNG_BUFSZ, &readinfos[i].random_states);
    readinfos[i].seekmax = seekmax;
  }

  // random read
  printf("random read\n");
  for (i = 0; i < nthread; i++){
    pthread_create(&pt[i], NULL,
                   (void *(*)(void *))random_read, (void *)(readinfos + i));
  }
  for (i = 0; i < nthread; i++){
    pthread_join(pt[i], NULL);
  }

  // get profile information
  stime = readinfos[0].stime;
  ftime = readinfos[0].ftime;
  for (i = 1; i < nthread; i++){
    if ((stime.tv_sec > readinfos[i].stime.tv_sec) ||
        ((stime.tv_sec == readinfos[i].stime.tv_sec) &&
         (stime.tv_usec > readinfos[i].stime.tv_usec))){
      stime = readinfos[i].stime;
    }
    if ((ftime.tv_sec < readinfos[i].ftime.tv_sec) ||
        ((ftime.tv_sec == readinfos[i].ftime.tv_sec) &&
         (ftime.tv_usec < readinfos[i].ftime.tv_usec))){
      ftime = readinfos[i].ftime;
    }
  }
  elatime = ((ftime.tv_sec - stime.tv_sec) * 1000000.0 + (ftime.tv_usec - stime.tv_usec));
  mbps = (iosize * iterate * nthread) / elatime;
  iops = (iterate * nthread) / (elatime / 1000000);
  for (i = 0; i < nthread; i++){
    latency += ((readinfos[i].ftime.tv_sec - readinfos[i].stime.tv_sec) * 1000000.0
                + (readinfos[i].ftime.tv_usec - readinfos[i].stime.tv_usec)) / iterate;
  }
  latency /= nthread;
  printf("stime = %ld.%06d\n", (unsigned long)stime.tv_sec, (unsigned int)stime.tv_usec);
  printf("ftime = %ld.%06d\n", (unsigned long)ftime.tv_sec, (unsigned int)ftime.tv_usec);
  printf("elapsed = %.1f(us)\nmbps = %f(MB/s)\niops = %f(io/s)\nlatency = %f(us)\n",
         elatime, mbps, iops, latency);

  for (i = 0; i < nthread; i++){
    close(readinfos[i].fd);
    free(readinfos[i].buf);
  }
  free(readinfos);
  free(pt);
  return 0;
}
