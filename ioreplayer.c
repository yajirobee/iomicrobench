#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>

#include "ioreplayer.h"
#include "arrayqueue.h"
#include "util.h"

struct {
  int cpu_cores;
  int openflg;
  int nthread;
  char *iodumpfile;
  long quelength;
  long bufsize;
} option;

void
printusage(const char *cmd)
{
  fprintf(stderr, "Usage : %s [-d] [-m nthread] [-q queue_length] iodumpfile\n", cmd);
}

void
parsearg(int argc, char **argv)
{
  int opt;

  option.cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
  option.openflg = O_RDWR;
  option.nthread = 1;
  option.iodumpfile = NULL;
  option.quelength = 1 << 18;
  option.bufsize = 1 << 22;

  while ((opt = getopt(argc, argv, "dm:q:")) != -1) {
    switch (opt) {
    case 'd':
      option.openflg |= O_DIRECT;
      break;
    case 'm':
      option.nthread = atoi(optarg);
      break;
    case 'q':
      option.quelength = procsuffix(optarg);
      break;
    default:
      printusage(argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if (optind == argc - 1) {
    option.iodumpfile = argv[optind];
  }
  else {
    printusage(argv[0]);
    exit(EXIT_FAILURE);
  }

  // print options
  printf("iodump_file\t%s\n"
         "num_thread\t%d\n"
         "enable_odirect\t%s\n"
         "task_queue_length\t%ld\n",
         option.iodumpfile,
         option.nthread,
         (option.openflg & O_DIRECT) ? "TRUE" : "FALSE",
         option.quelength);
}

//
// replaying io operation on multiple threads
//

void
ioreplayer(task_queue_t *tskque, char *buf, stats_t *stats)
{
  int i;
  iotask_t *curtask;
  int fd;

  // perform read operation
  while (1){
    pthread_mutex_lock(&tskque->control.mtx);
    while (queue_isempty(&tskque->tasks) && tskque->control.active) {
      pthread_cond_wait(&tskque->control.more, &tskque->control.mtx);
    }
    if (queue_isempty(&tskque->tasks) && !tskque->control.active) {
      pthread_mutex_unlock(&tskque->control.mtx);
      break;
    }
    curtask = (iotask_t *) queue_pop(&tskque->tasks);
    pthread_cond_signal(&tskque->control.less);
    pthread_mutex_unlock(&tskque->control.mtx);
    stats->operated_tasks++;
    fd = open(curtask->filepath, option.openflg);
    lseek(fd, curtask->offset, SEEK_SET);
    switch (curtask->iotype) {
    case READ_IO:
      for (i = 0; i < curtask->iteration; i++) { read(fd, buf, curtask->iosize); }
      stats->read_ops += i;
      stats->read_byte += curtask->iosize * i;
      break;
    case WRITE_IO:
      for (i = 0; i < curtask->iteration; i++) { write(fd, buf, curtask->iosize); }
      stats->write_ops += i;
      stats->write_byte += curtask->iosize * i;
      break;
    default:
      fprintf(stderr, "invalid task type: %c\n", curtask->iotype);
    }
    free(curtask);
  }
}

void *
thread_worker(void *args)
{
  threadconf_t *cnf = (threadconf_t *) args;
  task_queue_t *tskque = cnf->iotaskque;
  cleanup_queue_t *cleanupque = cnf->cleanupque;

  // set affinity
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cnf->cpuset) != 0){
    perror("pthread_setaffinity_np()");
    exit(EXIT_FAILURE);
  }

  ioreplayer(tskque, cnf->buf, &cnf->stats);
  pthread_mutex_lock(&cleanupque->mtx);
  queue_push(&cleanupque->finthreads, cnf);
  pthread_cond_signal(&cleanupque->cnd);
  pthread_mutex_unlock(&cleanupque->mtx);
  pthread_exit(NULL);
}

// task is expressed like following
// filepath, [R|W], offset, iosize, iteration
iotask_t *
getnext(FILE *fp){
  int i;
  char buf[MAX_ROW_LENGTH], *bufptr;
  iotask_t *task;

  if ((task = (iotask_t *) malloc(sizeof(iotask_t))) == NULL) {
    perror("malloc");
    return NULL;
  }

  if (fgets(buf, MAX_ROW_LENGTH, fp) != NULL) {
    for (i = 0; !(buf[i] == ',' || isblank(buf[i])); i++) { }
    memcpy(task->filepath, buf, i);
    for (; buf[i] == ',' || isblank(buf[i]); i++) { }
    task->iotype = buf[i++];
    for (; buf[i] == ',' || isblank(buf[i]); i++) { }
    task->offset = strtol(&buf[i], &bufptr, 10);
    for (i = 0; bufptr[i] == ',' || isblank(bufptr[i]); i++) { }
    task->iosize = strtol(&bufptr[i], &bufptr, 10);
    for (i = 0; bufptr[i] == ',' || isblank(bufptr[i]); i++) { }
    task->iteration = strtol(&bufptr[i], NULL, 10);
    return task;
  }
  else {
    return NULL;
  }
}

long
iotaskproducer(task_queue_t *tskque)
{
  long numtasks = 0;
  iotask_t *curtask;
  FILE *fp = fopen(option.iodumpfile, "r");

  for (curtask = getnext(fp); curtask != NULL; curtask = getnext(fp)) {
    pthread_mutex_lock(&tskque->control.mtx);
    while (queue_isfull(&tskque->tasks)) {
      pthread_cond_wait(&tskque->control.less, &tskque->control.mtx);
    }
    queue_push(&tskque->tasks, curtask);
    numtasks++;
    pthread_cond_signal(&tskque->control.more);
    pthread_mutex_unlock(&tskque->control.mtx);
  }
  fclose(fp);
  return numtasks;
}

long
replayio(int nthread, threadconf_t *thrdcnfs)
{
  int i;
  long generatedtasks;
  task_queue_t *tskque = thrdcnfs[0].iotaskque;
  cleanup_queue_t *cleanupque = thrdcnfs[0].cleanupque;
  threadconf_t *curthread;

  // create replayer threads
  for (i = 0; i < nthread; i++){
    pthread_create(&thrdcnfs[i].pt, NULL,
                   thread_worker, (void *)&thrdcnfs[i]);
  }

  generatedtasks = iotaskproducer(tskque);
  control_deactivate(&tskque->control);

  // join replayer threads
  for (i = nthread; i > 0; i--) {
    pthread_mutex_lock(&cleanupque->mtx);
    while (queue_isempty(&cleanupque->finthreads)){
      pthread_cond_wait(&cleanupque->cnd, &cleanupque->mtx);
    }
    curthread = (threadconf_t *) queue_pop(&cleanupque->finthreads);
    pthread_mutex_unlock(&cleanupque->mtx);
    pthread_join(curthread->pt, NULL);
  }
  return generatedtasks;
}

int
main(int argc, char **argv)
{
  int i;
  long generatedtasks;
  task_queue_t tskque;
  cleanup_queue_t cleanupque;
  threadconf_t *thrdcnfs;
  struct timespec stime, ftime;

  parsearg(argc, argv);
  // init queues
  init_queue(&tskque.tasks, option.quelength);
  init_quecontrol(&tskque.control);
  init_queue(&cleanupque.finthreads, MAX_THREADS);
  pthread_mutex_init(&cleanupque.mtx, NULL);
  pthread_cond_init(&cleanupque.cnd, NULL);

  // allocate memory for task configuration
  if (posix_memalign((void **) &thrdcnfs,
                     BLOCK_SIZE,
                     sizeof(threadconf_t) * option.nthread) != 0) {
    perror("posix_memalign");
    exit(EXIT_FAILURE);
  }

  // set task configurations for each threads
  for (i = 0; i < option.nthread; i++){
    int j;

    // set cpuset
    CPU_ZERO(&thrdcnfs[i].cpuset);
    for (j = 0; j < option.cpu_cores; j++){ CPU_SET(j, &thrdcnfs[i].cpuset); }

    // allocate buffer aligned by BLOCK_SIZE
    if (posix_memalign((void **) &thrdcnfs[i].buf, BLOCK_SIZE, option.bufsize) != 0) {
      perror("posix_memalign");
      exit(EXIT_FAILURE);
    }

    memset(thrdcnfs[i].buf, 'a', option.bufsize);
    thrdcnfs[i].iotaskque = &tskque;
    thrdcnfs[i].cleanupque = &cleanupque;
    thrdcnfs[i].stats = (stats_t){0, 0, 0, 0, 0};
  }

  // perform io replay
  fprintf(stderr, "started measurement\n");
  CLOCK_GETTIME(&stime);
  generatedtasks = replayio(option.nthread, thrdcnfs);
  CLOCK_GETTIME(&ftime);

  // print statistics information
  {
    long operatedtasks = 0;
    long totalrops = 0, totalwops = 0;
    long totalrbyte = 0, totalwbyte = 0;
    double exectime;
    double usptsk;
    double riops, wiops;
    double rmbps, wmbps;
    double ruspio, wuspio;

    for (i = 0; i < option.nthread; i++) {
      operatedtasks += thrdcnfs[i].stats.operated_tasks;
      totalrops += thrdcnfs[i].stats.read_ops;
      totalwops += thrdcnfs[i].stats.write_ops;
      totalrbyte += thrdcnfs[i].stats.read_byte;
      totalwbyte += thrdcnfs[i].stats.write_byte;
    }
    exectime = TIMEINTERVAL_SEC(stime, ftime);
    usptsk = exectime * 1000000 / operatedtasks;
    riops = totalrops / exectime;
    wiops = totalwops / exectime;
    rmbps = totalrbyte / exectime;
    wmbps = totalwbyte / exectime;
    ruspio = (totalrops == 0.0) ? 0.0 : exectime * 1000000 / totalrops;
    wuspio = (totalwops == 0.0) ? 0.0 : exectime * 1000000 / totalwops;

    printf("start_time\t%.9f\n"
           "finish_time\t%.9f\n",
           TS2SEC(stime), TS2SEC(ftime));
    printf("exec_time_sec\t%.9f\n"
           "generated_tasks\t%ld\n"
           "operated_tasks\t%ld\n"
           "usec_per_task\t%f\n"
           "read_io_per_sec\t%f\n"
           "read_mb_per_sec\t%f\n"
           "read_usec_per_io\t%.3f\n"
           "write_io_per_sec\t%f\n"
           "write_mb_per_sec\t%f\n"
           "write_usec_per_io\t%.3f\n",
           exectime, generatedtasks, operatedtasks, usptsk,
           riops, rmbps, ruspio, wiops, wmbps, wuspio);
  }

  // release resources
  for (i = 0; i < option.nthread; i++){ free(thrdcnfs[i].buf); }
  free(thrdcnfs);
  delete_queue(&cleanupque.finthreads);
  delete_queue(&tskque.tasks);
  return 0;
}
