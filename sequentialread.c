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

struct {
  int cpu_cores;
  int block_size;
  int openflg;
  int nthread;
  long iosize, iterate, fsize;
  char *filepath;
} option;

void
parsearg(int argc, char **argv)
{
  int opt;

  option.cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
  option.block_size = 512;
  option.openflg = O_RDONLY;
  option.nthread = 1;
  option.iosize = option.block_size;
  option.iterate = 4096;
  option.fsize = -1;

  while ((opt = getopt(argc, argv, "ds:i:m:S:")) != -1) {
    switch (opt) {
    case 'd':
      option.openflg |= O_DIRECT;
      break;
    case 's':
      option.iosize = procsuffix(optarg);
      break;
    case 'i':
      option.iterate = atol(optarg);
      break;
    case 'm':
      option.nthread = atoi(optarg);
      break;
    case 'S':
      option.fsize = procsuffix(optarg);
      break;
    default:
      printf("Usage : %s [-d] [-s iosize] [-i iterate] [-m nthread ] [-S fsize] filepath\n",
             argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if (argc - 1 == optind) {
    option.filepath = argv[optind];
  } else {
    printf("Usage : %s [-d] [-s iosize] [-i iterate] [-m nthread ] [-S fsize] filepath\n",
           argv[0]);
    exit(EXIT_FAILURE);
  }
}

void
sequential_read(seqread_t *readinfo)
{
  int i;

  //set affinity
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &readinfo->cpuset) != 0) {
    perror("pthread_setaffinity_np()");
    exit(EXIT_FAILURE);
  }

  gettimeofday(&readinfo->stime, NULL);
  for (i = 0; i < readinfo->iterate; i++) {
    read(readinfo->fd, readinfo->buf, readinfo->iosize);
  }
  gettimeofday(&readinfo->ftime, NULL);
}

int
main(int argc, char **argv)
{
  int i;
  pthread_t *pt;
  seqread_t *readinfos;
  struct timeval stime, ftime;
  double elatime, mbps, iops, latency = 0.0;

  parsearg(argc, argv);
  assert(option.iosize % option.block_size == 0);

  // check file size
  {
    int fd;
    if ((fd = open(option.filepath, O_RDONLY)) < 0) {
      perror("open");
      exit(EXIT_FAILURE);
    }
    if ((option.fsize = lseek(fd, 0, SEEK_END)) < 0) {
      perror("lseek");
      exit(EXIT_FAILURE);
    }
    fprintf(stderr, "size of %s = %ld\n", option.filepath, option.fsize);
    close(fd);
  }

  assert(option.fsize >= (option.iosize * option.iterate * option.nthread));

  // allocate memory for pthread_t
  if (posix_memalign((void **)&pt,
                     option.block_size,
                     sizeof(pthread_t) * option.nthread) != 0) {
    perror("posix_memalign");
    exit(EXIT_FAILURE);
  }

  // allocate memory for readinfo
  if (posix_memalign((void **)&readinfos,
                     option.block_size,
                     sizeof(seqread_t) * option.nthread) != 0) {
    perror("posix_memalign");
    exit(EXIT_FAILURE);
  }

  // set readinfo
  for (i = 0; i < option.nthread; i++) {
    int j;
    // allocate buffer aligned by BLOCK_SIZE
    if (posix_memalign((void **)&readinfos[i].buf,
                       option.block_size,
                       option.iosize) != 0){
      perror("posix_memalign");
      exit(EXIT_FAILURE);
    }
    // open file
    if ((readinfos[i].fd = open(option.filepath, option.openflg)) < 0) {
      perror("open");
      exit(EXIT_FAILURE);
    }
    // seek to assigned place
    if (lseek(readinfos[i].fd, i * option.iosize * option.iterate, SEEK_SET) < 0) {
      perror("lseek");
      exit(EXIT_FAILURE);
    }
    // set cpuset
    CPU_ZERO(&readinfos[i].cpuset);
    for (j = 0; j < option.cpu_cores; j++) { CPU_SET(j, &readinfos[i].cpuset); }

    readinfos[i].iosize = option.iosize;
    readinfos[i].iterate = option.iterate;
  }

  // sequential read
  printf("sequential read\n");
  for (i = 0; i < option.nthread; i++) {
    pthread_create(&pt[i], NULL,
                   (void *(*)(void *))sequential_read, (void *)(readinfos+ i));
  }
  for (i = 0; i < option.nthread; i++) {
    pthread_join(pt[i], NULL);
  }

  // get profile information
  stime = readinfos[0].stime;
  ftime = readinfos[0].ftime;
  for (i = 1; i < option.nthread; i++){
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
  mbps = (option.iosize * option.iterate * option.nthread) / elatime;
  iops = (option.iterate * option.nthread) / (elatime / 1000000);
  for (i = 0; i < option.nthread; i++){
    latency += ((readinfos[i].ftime.tv_sec - readinfos[i].stime.tv_sec) * 1000000.0
                + (readinfos[i].ftime.tv_usec - readinfos[i].stime.tv_usec)) / option.iterate;
  }
  latency /= option.nthread;
  printf("stime = %ld.%06d\n", (unsigned long)stime.tv_sec, (unsigned int)stime.tv_usec);
  printf("ftime = %ld.%06d\n", (unsigned long)ftime.tv_sec, (unsigned int)ftime.tv_usec);
  printf("elapsed = %.1f(us)\n"
         "mbps = %f(MB/s)\n"
         "iops = %f(io/s)\n"
         "latency = %f(us)\n",
         elatime, mbps, iops, latency);

  for (i = 0; i < option.nthread; i++){
    close(readinfos[i].fd);
    free(readinfos[i].buf);
  }
  free(readinfos);
  free(pt);
  return 0;
}
