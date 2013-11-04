#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <assert.h>

#include "arrayqueue.h"

int
init_queue(queue_t *que, size_t max_length)
{
  if ((que->ringbuf = (void **) calloc(max_length, sizeof(void *))) == NULL){
    perror("malloc");
    exit(0);
  }
  que->max_length = max_length;
  que->length = 0;
  que->head = que->tail = 0;
  return 0;
}

int
delete_queue(queue_t *que)
{
  free(que->ringbuf);
  return 0;
}

int
queue_isempty(queue_t *que)
{
  return (que->length == 0) ? 1 : 0;
}

int
queue_isfull(queue_t *que)
{
  return (que->length == que->max_length) ? 1 : 0;
}

void
queue_push(queue_t *que, void *item)
{
  assert(que->length < que->max_length);
  que->ringbuf[que->head++] = item;
  if (que->head == que->max_length) { que->head = 0; }
  que->length++;
}

void *
queue_pop(queue_t *que)
{
  void *item = NULL;

  assert(que->length > 0);
  item = que->ringbuf[que->tail++];
  if (que->tail == que->max_length) { que->tail = 0; }
  que->length--;
  return item;
}

int
init_quecontrol(queue_control_t *ctrl)
{
  pthread_mutex_init(&ctrl->mtx, NULL);
  pthread_cond_init(&ctrl->more, NULL);
  pthread_cond_init(&ctrl->less, NULL);
  ctrl->active = 1;
  return 0;
}

int
control_deactivate(queue_control_t *ctrl)
{
  pthread_mutex_lock(&ctrl->mtx);
  ctrl->active = 0;
  pthread_cond_broadcast(&ctrl->more);
  pthread_mutex_unlock(&ctrl->mtx);
  return 0;
}
