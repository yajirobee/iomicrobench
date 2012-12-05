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

#define DROP_CACHES "/proc/sys/vm/drop_caches"
#define BLOCK_SIZE 512

void clearcache(){
  FILE *fp;

  if ((fp = fopen(DROP_CACHES, "w")) == NULL){
    perror("drop cache");
    exit(1);
  }
  putc('3', fp);
  fclose(fp);
  printf("cache droped\n");
}

void printspec(struct timeval stime, struct timeval ftime, long iosize, long iterate){
  double elatime, mbps, iops;
  elatime = ((ftime.tv_sec - stime.tv_sec) * 1000000.0 + (ftime.tv_usec - stime.tv_usec));
  mbps = (iosize * iterate) / elatime;
  iops = iterate / (elatime / 1000000);
  printf("elapsed = %.1f(us)\nmbps = %f(MB/s)\niops = %f(io/s)\n",
         elatime, mbps, iops);
}

int main(int argc, char **argv){
  int i;
  int mode;
  long iosize, iterate;
  int fd;
  long fsize, seekmax;
  char *buf;
  struct timeval stime, ftime;

  if (argc != 5){
    printf("Usage : %s mode filepath iosize iterate\n", argv[0]);
    exit(1);
  }
  mode = atoi(argv[1]);
  iosize = atol(argv[3]);
  iterate = atol(argv[4]);
  assert(iosize % BLOCK_SIZE == 0);
  if ((mode < 0) || (mode > 4)){
    printf("Invalid mode number\n");
    exit(1);
  }

  // sync data before cache to be droped
  sync();

  // allocate buffer aligned by BLOCK_SIZE
  if (posix_memalign((void **)&buf, BLOCK_SIZE, iosize) != 0){
    perror("posix_memalign");
    exit(1);
  }

  //open file
  if ((fd = open(argv[2], O_RDWR | O_CREAT | O_DIRECT)) < 0){
    perror("open");
    exit(1);
  }

  // check file size
  if ((fsize = lseek(fd, 0, SEEK_END)) < 0){
    perror("lseek");
    exit(1);
  }
  printf("size of %s = %ld\n", argv[2], fsize);

  //set seekmax
  assert((fsize - iosize) > 0);
  if (((fsize - iosize) / BLOCK_SIZE) <= RAND_MAX){
    seekmax = (fsize - iosize) / BLOCK_SIZE;
  }
  else{
    seekmax = RAND_MAX;
  }

  // sequential read
  if ((mode == 0) || (mode == 1)){
    assert(fsize >= (iosize * iterate));
    clearcache();
    if (lseek(fd, 0, SEEK_SET) < 0){
      perror("lseek");
      exit(1);
    }
    printf("sequential read\n");
    gettimeofday(&stime, NULL);
    for (i = 0; i < iterate; i++){
      read(fd, buf, iosize);
    }
    gettimeofday(&ftime, NULL);
    printspec(stime, ftime, iosize, iterate);
  }

  // random read
  if ((mode == 0) || (mode == 2)){
    assert(fsize >= (iosize * iterate));
    clearcache();
    printf("random read\n");
    gettimeofday(&stime, NULL);
    for (i = 0; i < iterate; i++){
      pread(fd, buf, iosize, (random() % seekmax) * BLOCK_SIZE);
    }
    gettimeofday(&ftime, NULL);
    printspec(stime, ftime, iosize, iterate);
  }

  if ((mode == 0) || (mode == 3) || (mode == 4)){
    for (i = 0; i < iosize; i++){
      buf[i] = random() % 94 + 32;
    }
  }

  // sequential write
  if ((mode == 0) || (mode == 3)){
    assert(strcmp(argv[2], "/dev/sda") != 0);
    clearcache();
    if (lseek(fd, 0, SEEK_SET) < 0){
      perror("lseek");
      exit(1);
    }
    printf("sequential write\n");
    gettimeofday(&stime, NULL);
    for (i = 0; i < iterate; i++){
      write(fd, buf, iosize);
    }
    gettimeofday(&ftime, NULL);
    printspec(stime, ftime, iosize, iterate);
  }

  // random write
  if ((mode == 0) || (mode == 4)){
    assert(strcmp(argv[2], "/dev/sda") != 0);
    clearcache();
    printf("random write\n");
    gettimeofday(&stime, NULL);
    for (i = 0; i < iterate; i++){
      pwrite(fd, buf, iosize, (random() % seekmax) * BLOCK_SIZE);
    }
    gettimeofday(&ftime, NULL);
    printspec(stime, ftime, iosize, iterate);
  }

  free(buf);
  close(fd);
  return 0;
}
