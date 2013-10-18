#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define BUFSIZE 1 << 15

int main(int argc, char **argv){
  long i;
  char buf[BUFSIZE];
  FILE *fp;

  char *outpath;
  size_t size;

  if (argc != 3) {
    fprintf(stderr, "Usage : %s outputpath size\n", argv[0]);
    exit(1);
  }
  outpath = argv[1];
  size = atol(argv[2]);
  printf("outputsize = %ld\n", size);
  assert(size % BUFSIZE == 0);

  memset(buf, 'a', BUFSIZE);

  if ((fp = fopen(outpath, "w")) == NULL) {
    fprintf(stderr, "file open error : %s cannot open\n", outpath);
    exit(1);
  }
  for (i = 0; i < size; i += BUFSIZE) { fputs(buf, fp); }
  fclose(fp);

  return 0;
}
