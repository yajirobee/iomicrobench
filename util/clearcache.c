#include <stdio.h>
#include <stdlib.h>
#define drop_cache "/proc/sys/vm/drop_caches"

int main(){
  FILE *fp;
  
  if ((fp = fopen(drop_cache,"w")) == NULL ){
    perror(drop_cache);
    exit(1);
  }
  putc('3', fp);
  fclose(fp);
}
