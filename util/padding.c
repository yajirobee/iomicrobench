#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#define IOSIZE 2048
#define NTHREAD 32

typedef struct{
  int fd;
  char buf[IOSIZE];
  long wsize;
} write_t;

void padding(write_t writeinfo){
  long wsize = writeinfo.wsize;
  while (wsize > 0){
    memset(writeinfo.buf, random() % 94 + 32, IOSIZE);
    if (wsize > IOSIZE) wsize -= write(writeinfo.fd, writeinfo.buf, IOSIZE);
    else wsize -= write(writeinfo.fd, writeinfo.buf, wsize);
  }
}

int main(int argc, char **argv){
  int i;
  write_t *writeinfos;
  int nthread = NTHREAD;
  pthread_t *pt;
  int fd;
  long fsize, wsizept;

  if (argc != 2){
    printf("Usage : %s fpath\n", argv[0]);
    exit(0);
  }

  if ((fd = open(argv[1], O_RDONLY)) < 0){
    perror("open");
    exit(1);
  }

  // check file size
  if ((fsize = lseek(fd, 0, SEEK_END)) < 0){
    perror("lseek");
    exit(1);
  }
  printf("size of %s = %ld\n", argv[1], fsize);
  close(fd);
  wsizept = fsize / nthread;

  if ((pt = (pthread_t *)calloc(nthread, sizeof(pthread_t))) == NULL){
    perror("calloc");
    exit(1);
  }
  // allocate memory for readinfo
  if ((writeinfos = (write_t *)calloc(nthread, sizeof(write_t))) == NULL){
    perror("calloc");
    exit(1);
  }

  // set readinfo
  for (i = 0; i < nthread; i++){
    // open file
    if((writeinfos[i].fd = open(argv[1], O_WRONLY)) < 0){
      perror("open");
      exit(1);
    }
    // seek to assigned place
    if (lseek(writeinfos[i].fd, i * wsizept, SEEK_SET) < 0){
      perror("lseek");
      exit(1);
    }
    if (i == nthread - 1){
      writeinfos[i].wsize = wsizept + (fsize % nthread);
    }
    else{
      writeinfos[i].wsize = wsizept;
    }
  }
  for (i = 0; i < nthread; i++){
    pthread_create(&pt[i], NULL, (void *(*)(void *))padding, &writeinfos[i]);
  }
  for (i = 0; i < nthread; i++){
    pthread_join(pt[i], NULL);
  }

  for (i = 0; i < nthread; i++){
    close(writeinfos[i].fd);
  }
  free(pt);
  free(writeinfos);
  return 0;
}
