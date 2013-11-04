#ifndef __SCHEME_QUEUE__
#define __SCHEME_QUEUE__

typedef struct{
  void **ringbuf;
  long length, max_length;
  long head, tail;
} queue_t;

typedef struct {
  int active;
  pthread_mutex_t mtx;
  pthread_cond_t more, less;
} queue_control_t;

int init_queue(queue_t *que, size_t buf_size);
int delete_queue(queue_t *que);
int queue_isempty(queue_t *que);
int queue_isfull(queue_t *que);
void queue_push(queue_t *que, void *item);
void *queue_pop(queue_t *que);

int init_quecontrol(queue_control_t *ctrl);
int control_deactivate(queue_control_t *ctrl);

#endif // __SCHEME_QUEUE__
