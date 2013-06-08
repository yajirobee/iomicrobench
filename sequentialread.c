#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
  long iosize, maxiter, fsize;
  double timeout;
  char *filepath;
} option;

void
printusage(const char *cmd)
{
  fprintf(stderr,
          "Usage : %s [-d] [-s iosize] [-i maxiter] [-t timeout] "
          "[-m nthread ] [-S fsize] filepath\n",
          cmd);
}

void
parsearg(int argc, char **argv)
{
  int opt;

  option.cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
  option.block_size = 512;
  option.openflg = O_RDONLY;
  option.nthread = 1;
  option.iosize = option.block_size;
  option.maxiter = 4096;
  option.timeout = 60 * 60;
  option.fsize = -1;

  while ((opt = getopt(argc, argv, "ds:i:t:m:S:")) != -1) {
    switch (opt) {
    case 'd':
      option.openflg |= O_DIRECT;
      break;
    case 's':
      option.iosize = procsuffix(optarg);
      break;
    case 'i':
      option.maxiter = atol(optarg);
      break;
    case 't':
      option.timeout = atof(optarg);
      break;
    case 'm':
      option.nthread = atoi(optarg);
      break;
    case 'S':
      option.fsize = procsuffix(optarg);
      break;
    default:
      printusage(argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if (argc - 1 == optind) {
    option.filepath = argv[optind];
  } else {
    printusage(argv[0]);
    exit(EXIT_FAILURE);
  }
}

void
sequential_read(seqread_t *readinfo)
{
  int i;
  struct timespec stime, ftime;

  //set affinity
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &readinfo->cpuset) != 0) {
    perror("pthread_setaffinity_np()");
    exit(EXIT_FAILURE);
  }

  CLOCK_GETTIME(&stime);
  do {
    for (i = 0; i < 1024; i++) {
      read(readinfo->fd, readinfo->buf, option.iosize);
    }
    readinfo->ops += 1024;
    CLOCK_GETTIME(&ftime);
  } while ((TIMEINTERVAL_SEC(stime, ftime) < option.timeout) &&
           (readinfo->ops < option.maxiter));

  readinfo->stime = TS2SEC(stime);
  readinfo->ftime = TS2SEC(ftime);
}


int
main(int argc, char **argv)
{
  int i;
  pthread_t *pt;
  seqread_t *readinfos;

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
    close(fd);
  }

  assert(option.fsize >= (option.iosize * option.maxiter * option.nthread));

  fprintf(stderr, "Sequential read I/O microbenchmark\n");
  printf("io_size\t%ld\n"
         //"iteration\t%ld\n"
         "num_thread\t%d\n"
         "file_path\t%s\n"
         "enable_odirect\t%s\n"
         "target_size\t%ld\n",
         option.iosize,
         //option.maxiter,
         option.nthread,
         option.filepath,
         (option.openflg & O_DIRECT) ? "TRUE" : "FALSE",
         option.fsize);

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
    if (lseek(readinfos[i].fd, i * option.iosize * option.maxiter, SEEK_SET) < 0) {
      perror("lseek");
      exit(EXIT_FAILURE);
    }
    // set cpuset
    CPU_ZERO(&readinfos[i].cpuset);
    for (j = 0; j < option.cpu_cores; j++) { CPU_SET(j, &readinfos[i].cpuset); }

    readinfos[i].ops = 0;
  }

  // sequential read
  for (i = 0; i < option.nthread; i++) {
    pthread_create(&pt[i], NULL,
                   (void *(*)(void *))sequential_read, (void *)(readinfos+ i));
  }
  for (i = 0; i < option.nthread; i++) {
    pthread_join(pt[i], NULL);
  }

  // get profile information
  {
    double stime, ftime;
    long ops = 0;
    double exectime, mbps, iops, latency = 0.0;

    stime = readinfos[0].stime;
    ftime = readinfos[0].ftime;
    for (i = 1; i < option.nthread; i++) {
      if (stime > readinfos[i].stime) { stime = readinfos[i].stime; }
      if (ftime < readinfos[i].ftime) { ftime = readinfos[i].ftime; }
    }
    exectime = ftime - stime;
    for (i = 0; i < option.nthread; i++) { ops += readinfos[i].ops; }
    iops = ops / exectime;
    mbps = (option.iosize * ops) / exectime / 1000000;
    for (i = 0; i < option.nthread; i++){
      latency += (readinfos[i].ftime - readinfos[i].stime);
    }
    latency /= ops;
    printf("start_time\t%.9f\n"
           "finish_time\t%.9f\n",
           stime, ftime);
    printf("exec_time_sec\t%.9f\n"
           "total_ops\t%ld\n"
           "mb_per_sec\t%f\n"
           "io_per_sec\t%f\n"
           "usec_per_io\t%f\n",
           exectime, ops, mbps, iops, latency * 1000000);
  }

  for (i = 0; i < option.nthread; i++){
    close(readinfos[i].fd);
    free(readinfos[i].buf);
  }
  free(readinfos);
  free(pt);
  return 0;
}
