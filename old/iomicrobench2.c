#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "iomicrobench2.h"

void initque(queue_t *que, size_t limit){
  if ((que->a = (rinfo_t **)calloc(limit, sizeof(rinfo_t))) == NULL){
    perror("malloc");
    exit(0);
  }
  que->limit = limit;
  que->head = 0;
  que->tail = 0;
  que->size = 0;
}

void delque(queue_t *que){
  free(que->a);
}

void push(queue_t *que, rinfo_t *rinfo){
  static count = 0;
  assert(que->limit > que->size);
  que->a[que->tail++] = rinfo;
  if (que->tail >= que->limit){ que->tail = 0; }
  que->size++;
}

rinfo_t *pop(queue_t *que){
  rinfo_t *head;

  assert(que->size > 0);
  head = que->a[que->head];
  que->a[que->head++] = NULL;
  if (que->head >= que->limit){ que->head = 0; }
  que->size--;
  return head;
}
