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
#include "util.h"

typedef struct {
  int fd;
  char *buf;
  double stime, ftime;
  long ops;
  cpu_set_t cpuset;
  struct random_data random_states;
  char statebuf[PRNG_BUFSZ];
} randread_t;

struct {
  int cpu_cores;
  int openflg;
  int nthread;
  long iosize, maxiter, fsize;
  double timeout;
  char *filepath;
  long seekmax;
} option;

void
printusage(const char *cmd)
{
  printf("Usage : %s [-d] [-s iosize] [-i maxiter] [-t timeout] "
         "[-m nthread ] [-S fsize] filepath\n",
         cmd);
}

void
parsearg(int argc, char **argv)
{
  int opt;

  option.cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
  option.openflg = O_RDONLY;
  option.nthread = 1;
  option.iosize = BLOCK_SIZE;
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
  }
  else {
    printusage(argv[0]);
    exit(EXIT_FAILURE);
  }

  // check file size
  if (-1 == option.fsize) {
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

  //set seekmax
  if (((option.fsize - option.iosize) / BLOCK_SIZE) <= RAND_MAX) {
    option.seekmax = (option.fsize - option.iosize) / BLOCK_SIZE;
  }
  else {
    option.seekmax = RAND_MAX;
  }

  fprintf(stderr, "Random read I/O microbenchmark\n");
  // print options
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

  assert(option.iosize % BLOCK_SIZE == 0);
  assert(option.fsize >= option.iosize);
}

void
random_read(randread_t *readinfo)
{
  int i, tmp;
  struct timespec stime, ftime;

  //set affinity
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &readinfo->cpuset) != 0) {
    perror("pthread_setaffinity_np()");
    exit(EXIT_FAILURE);
  }

  CLOCK_GETTIME(&stime);
  do {
    for (i = 0; i < 1024; i++) {
      random_r(&readinfo->random_states, &tmp);
      pread(readinfo->fd, readinfo->buf, option.iosize,
            (tmp % option.seekmax) * BLOCK_SIZE);
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
  randread_t *readinfos;

  parsearg(argc, argv);

  // allocate memory for pthread_t
  if (posix_memalign((void **) &pt,
                     BLOCK_SIZE,
                     sizeof(pthread_t) * option.nthread) != 0) {
    perror("posix_memalign");
    exit(EXIT_FAILURE);
  }

  // allocate memory for readinfo
  if (posix_memalign((void **) &readinfos,
                     BLOCK_SIZE,
                     sizeof(randread_t) * option.nthread) != 0) {
    perror("posix_memalign");
    exit(EXIT_FAILURE);
  }

  // set readinfo
  for (i = 0; i < option.nthread; i++) {
    int j;
    // allocate buffer aligned by BLOCK_SIZE
    if (posix_memalign((void **) &readinfos[i].buf, BLOCK_SIZE, option.iosize) != 0) {
      perror("posix_memalign");
      exit(EXIT_FAILURE);
    }
    //open file
    if ((readinfos[i].fd = open(option.filepath, option.openflg)) < 0) {
      perror("open");
      exit(EXIT_FAILURE);
    }
    // set cpuset
    CPU_ZERO(&readinfos[i].cpuset);
    for (j = 0; j < option.cpu_cores; j++) { CPU_SET(j, &readinfos[i].cpuset); }

    initstate_r(i + 1, readinfos[i].statebuf, PRNG_BUFSZ, &readinfos[i].random_states);
    readinfos[i].ops = 0;
  }

  // random read
  for (i = 0; i < option.nthread; i++) {
    pthread_create(&pt[i], NULL,
                   (void *(*)(void *))random_read, (void *)(readinfos + i));
  }
  for (i = 0; i < option.nthread; i++) {
    pthread_join(pt[i], NULL);
  }

  // print statistics information
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

  for (i = 0; i < option.nthread; i++) {
    close(readinfos[i].fd);
    free(readinfos[i].buf);
  }
  free(readinfos);
  free(pt);
  return 0;
}
