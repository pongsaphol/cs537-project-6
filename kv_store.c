#include <stdio.h>
#include <sys/mman.h>
#include <pthread.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <string.h>

#include "ring_buffer.h"
#include "common.h"
#include "kv_store.h"

list* hash_table;
int table_size;
int num_threads;
struct ring *ring;
char* shmem_area;

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
  node *current = hash_table[bucket].head;
  while (current != NULL) {
    if (current->k == key) {
      value_type value = current->v;
      return value;
    }
    current = current->next;
  }
  return 0;
}

char shm_file[] = "shmem_file";

/*
 * Initialize the shared memory ring buffer
 * Sets the shmem_area global variable to the beginning of the shared region
 * Sets the ring global variable the beginning of the shared region 
 * Shared memory area is organized as follows:
 * | RING | TID_0_COMPLETIONS | TID_1_COMPLETIONS | ... | TID_N_COMPLETIONS |
*/
int init_server() {
  init_hash_table();
	int fd = open(shm_file, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (fd < 0)
		perror("open");
  
  struct stat sb;
  fstat(fd, &sb);

  int shm_size = sb.st_size;

	char *mem = mmap(NULL, shm_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
	if (mem == (void *)-1) 
		perror("mmap");

	/* mmap dups the fd, no longer needed */
	close(fd);

	// memset(mem, 0, shm_size);
	ring = (struct ring *)mem;
	shmem_area = mem;

}



static int parse_args(int argc, char **argv) {
	int op;
	while ((op = getopt(argc, argv, "hn:w:vt:s:fce:i:")) != -1) {
		switch(op) {									           
		case 'n':
		num_threads = atoi(optarg);
		break;
	
		case 's':
    table_size = atoi(optarg);
		break;
	
		return 1;
		}
	}
	return 0;
}


void *thread_function(void *arg) {
  while (true) {
    struct buffer_descriptor bd;
    ring_get(ring, &bd);
    if (bd.req_type == PUT) {
      put(bd.k, bd.v);
    } else {
      value_type value = get(bd.k);
      bd.v = value;
    }
    bd.ready = 1;
    memcpy(shmem_area + bd.res_off, &bd, sizeof(struct buffer_descriptor));
  }
}

void start_threads() {
  pthread_t threads[num_threads];
  // printf("THREADS: %d\n", num_threads);
  for (int i = 0; i < num_threads; i++) {
    pthread_create(&threads[i], NULL, thread_function, NULL);
  }
  for (int i = 0; i < num_threads; i++) {
    pthread_join(threads[i], NULL);
  }
}

int main(int argc, char *argv[]) {
  if (parse_args(argc, argv) != 0)
		exit(EXIT_FAILURE);
  init_server();
  start_threads();
}