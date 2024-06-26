#include "ring_buffer.h"
#include <stdio.h>
#include <sys/types.h>

static inline void cpu_spinwait(void) {
  sched_yield();  
}

/*
 * Initialize the ring
 * @param r A pointer to the ring
 * @return 0 on success, negative otherwise - this negative value will be
 * printed to output by the client program
*/
int init_ring(struct ring *r) {
  r->p_tail = 0;
  r->p_head = 0;
  r->c_tail = 0;
  r->c_head = 0;
  return 0;
}

/*
 * Submit a new item - should be thread-safe
 * This call will block the calling thread if there's not enough space
 * @param r The shared ring
 * @param bd A pointer to a valid buffer_descriptor - This pointer is only
 * guaranteed to be valid during the invocation of the function
*/
void ring_submit(struct ring *r, struct buffer_descriptor *bd) {
  uint32_t c_tail, p_head, p_next, status;
  do {
    status = 0;
    c_tail = r->c_tail;
    p_head = r->p_head;
    p_next = (p_head + 1) % RING_SIZE;
    if (c_tail == p_next) {
      // ring is full, sleep
      cpu_spinwait();
      continue;
    }
    status = __sync_bool_compare_and_swap(&r->p_head, p_head, p_next);
  } while (status == 0);


  r->buffer[p_head] = *bd;

  while (r->p_tail != p_head) {
    // wait for the producer to finish
    cpu_spinwait();
  }
  r->p_tail = p_next;
}

/*
 * Get an item from the ring - should be thread-safe
 * This call will block the calling thread if the ring is empty
 * @param r A pointer to the shared ring 
 * @param bd pointer to a valid buffer_descriptor to copy the data to
 * Note: This function is not used in the clinet program, so you can change
 * the signature.
*/
void ring_get(struct ring *r, struct buffer_descriptor *bd) {
  uint32_t c_head, p_tail, c_next, status;
  do {
    status = 0;
    c_head = r->c_head;
    p_tail = r->p_tail;
    c_next = (c_head + 1) % RING_SIZE;
    if (c_head == p_tail) {
      // ring is empty, sleep
      cpu_spinwait();
      continue;
    }
    status = __sync_bool_compare_and_swap(&r->c_head, c_head, c_next);
  } while (status == 0);
  *bd = r->buffer[c_head];
  while (r->c_tail != c_head) {
    // wait for the consumer to finish
    cpu_spinwait();
  }
  r->c_tail = c_next;
}
