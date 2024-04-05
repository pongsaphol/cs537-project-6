#include "common.h"
#include "ring_buffer.h"

typedef struct __node_t {
  key_type k;
  value_type v;
  struct __node_t *next;
} node;

typedef struct __list_t {
  node *head;
  pthread_mutex_t plock;
  pthread_mutex_t glock;
} list;

void put(key_type key, value_type value);
value_type get(key_type key);