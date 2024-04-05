#include "ring_buffer.h"
#include "common.h"
#include "kv_store.h"


list* hash_table;
int table_size;

int init_hash_table() {
  hash_table = (list *)malloc(table_size * sizeof(list));
  if (hash_table == NULL) {
    return -1;
  }
  for (int i = 0; i < table_size; i++) {
    hash_table[i].head = NULL;
    if (pthread_mutex_init(&hash_table[i].plock, NULL) != 0) {
      return -1;
    }
    if (pthread_mutex_init(&hash_table[i].glock, NULL) != 0) {
      return -1;
    }
  }
  return 0;
}

void put(key_type key, value_type value) {
  index_t bucket = hash_function(key, table_size);
  node *new_node = (node *)malloc(sizeof(node));
  new_node->k = key;
  new_node->v = value;
  pthread_mutex_lock(&hash_table[bucket].plock);
  new_node->next = hash_table[bucket].head;
  hash_table[bucket].head = new_node;
  pthread_mutex_unlock(&hash_table[bucket].plock);
}

value_type get(key_type key) {
  index_t bucket = hash_function(key, table_size);
  pthread_mutex_lock(&hash_table[bucket].glock);
  node *current = hash_table[bucket].head;
  while (current != NULL) {
    if (current->k == key) {
      value_type value = current->v;
      pthread_mutex_unlock(&hash_table[bucket].glock);
      return value;
    }
    current = current->next;
  }
  pthread_mutex_unlock(&hash_table[bucket].glock);
  return 0;
}