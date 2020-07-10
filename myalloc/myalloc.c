#include <stdio.h>
#include <stdlib.h>
#include "myalloc.h"

/* change me to 1 for more debugging information
 * change me to 0 for time testing and to clear your mind
 */
#define DEBUG 0

void *__heap = NULL;
node_t *__head = NULL;

header_t *get_header(void *ptr) {
  return (header_t *) (ptr - sizeof(header_t));
}

void print_header(header_t *header) {
  printf("[header_t @ %p | buffer @ %p size: %lu magic: %08lx]\n",
         header,
         ((void *) header + sizeof(header_t)),
         header->size,
         header->magic);
}

void print_node(node_t *node) {
  printf("[node @ %p | free region @ %p size: %lu next: %p]\n",
         node,
         ((void *) node + sizeof(node_t)),
         node->size,
         node->next);
}

void print_freelist_from(node_t *node) {
  printf("\nPrinting freelist from %p\n", node);
  while (node != NULL) {
    print_node(node);
    node = node->next;
  }
}

node_t* sort_linked_list(node_t *past_head){
  if(past_head == NULL || past_head->next == NULL){
    return past_head;
  }
  node_t *current;
  node_t *tiniest;
  node_t *tiniestPrev;
  node_t *prev;

  current = past_head;
  tiniest = past_head;
  prev = past_head;
  tiniestPrev = past_head;

  while(current != NULL){
    if((void*) current < (void*) tiniest){
      tiniestPrev = prev;
      tiniest = current;
    }
    prev = current;
    current = current->next;
  }

  node_t* temp;
  if(tiniest != past_head){
    tiniestPrev->next = past_head;
    temp = past_head->next;
    past_head->next = tiniest->next;
    tiniest->next = temp;
  }
  tiniest->next = sort_linked_list(tiniest->next);
  return tiniest;
}

void coalesce_freelist() {
  /* coalesce all neighboring free regions in the free list */

  if (DEBUG) printf("In coalesce freelist...\n");
  __head = sort_linked_list(__head);
  node_t *target = __head;
  node_t *node = target->next;
  //node_t *prev = target;

  /* traverse the free list, coalescing neighboring regions!
   * some hints:
   * --> it might be easier if you sort the free list first!
   * --> it might require multiple passes over the free list!
   * --> it might be easier if you call some helper functions from here
   * --> see print_free_list_from for basic code for traversing a
   *     linked list!
   */
  int coalesce = 0;

  while(target != NULL && node != NULL){
    if((void*) node == (void*) target + target->size + sizeof(header_t)){
      node_t *new_next = node->next;
      target->size = target->size + node->size + sizeof(header_t);
      target->next = new_next;
      coalesce = 0;
    }
    //prev = target;
    target = target->next;
    node = target->next;
  }

  if(coalesce == 1){
    coalesce_freelist();
  }

}

void destroy_heap() {
  /* after calling this the heap and free list will be wiped
   * and you can make new allocations and frees on a "blank slate"
   */
  free(__heap);
  __heap = NULL;
  __head = NULL;
}

/* In reality, the kernel or memory allocator sets up the initial heap. But in
 * our memory allocator, we have to allocate our heap manually, using malloc().
 * YOU MUST NOT ADD MALLOC CALLS TO YOUR FINAL PROGRAM!
 */
void init_heap() {
  /* FOR OFFICE USE ONLY */

  if ((__heap = malloc(HEAPSIZE)) == NULL) {
    printf("Couldn't initialize heap!\n");
    exit(1);
  }

  __head = (node_t *) __heap;
  __head->size = HEAPSIZE - sizeof(header_t);
  __head->next = NULL;

  if (DEBUG) printf("heap: %p\n", __heap);
  if (DEBUG) print_node(__head);

}

void *first_fit(size_t req_size) {
  //void *ptr = NULL; /* pointer to the match that we'll return */
  void *ptr = __head;
  int fnum_s = 0;

  if (DEBUG)
    printf("In first_fit with size: %u and freelist @ %p\n",
           (unsigned) req_size, __head);

  node_t *listitem = __head; /* cursor into our linked list */
  node_t *prev = NULL; /* if listitem is __head, then prev must be null */
  header_t *alloc; /* a pointer to a header you can use for your allocation */

  /* traverse the free list from __head! when you encounter a region that
   * is large enough to hold the buffer and required header, use it!
   * If the region is larger than you need, split the buffer into two
   * regions: first, the region that you allocate and second, a new (smaller)
   * free region that goes on the free list in the same spot as the old free
   * list node_t.
   *
   * If you traverse the whole list and can't find space, return a null
   * pointer! :(
   *
   * Hints:
   * --> see print_freelist_from to see how to traverse a linked list
   * --> remember to keep track of the previous free region (prev) so
   *     that, when you divide a free region, you can splice the linked
   *     list together (you'll either use an entire free region, so you
   *     point prev to what used to be next, or you'll create a new
   *     (smaller) free region, which should have the same prev and the next
   *     of the old region.
   * --> If you divide a region, remember to update prev's next pointer!
   */

  while(listitem != NULL){
    if((req_size + sizeof(header_t)) <= listitem->size && req_size > 0){
      int listitem_is_head = 0;
      node_t *new_freelist_item_header = NULL;

      int listitem_size = listitem->size;
      node_t *listitem_next = listitem->next;

      alloc = (void*) listitem;
      alloc->magic = HEAPMAGIC;
      alloc->size = req_size;

      ptr = (void*) alloc + sizeof(header_t);

      if(__head == listitem){
        listitem_is_head = 1;
      }

      if((listitem_size - req_size) >= (sizeof(header_t))){
        new_freelist_item_header = (void*) ptr + req_size;
        new_freelist_item_header->size = listitem_size - req_size - sizeof(header_t);
        new_freelist_item_header->next = listitem_next;
      }

      if(listitem_is_head == 1){
        if(new_freelist_item_header !=NULL){
          __head = new_freelist_item_header;
        }
        else{
          __head = listitem_next;
        }
      }
      else{
        if(new_freelist_item_header != NULL){
          prev->next = new_freelist_item_header;
          alloc->size = req_size + (listitem_size - req_size);
          printf("Size of new alloc: %ld\n", alloc->size);
        }
        else{
          prev->next = listitem_next;
        }
      }
      fnum_s = 1;

      break;
    }
    prev = listitem;
    listitem = listitem->next;
  }
  
  if(fnum_s == 0){
    ptr = NULL;
  }

  if (DEBUG) printf("Returning pointer: %p\n", ptr);
  return ptr;

}

/* myalloc returns a void pointer to size bytes or NULL if it can't.
 * myalloc will check the free regions in the free list, which is pointed to by
 * the pointer __head.
 */

void *myalloc(size_t size) {
  if (DEBUG) printf("\nIn myalloc:\n");
  void *ptr = NULL;

  /* initialize the heap if it hasn't been */
  if (__heap == NULL) {
    if (DEBUG) printf("*** Heap is NULL: Initializing ***\n");
    init_heap();
  }

  /* perform allocation */
  /* search __head for first fit */
  if (DEBUG) printf("Going to do allocation.\n");

  ptr = first_fit(size); /* all the work really happens in first_fit */

  if (DEBUG) printf("__head is now @ %p\n", __head);

  return ptr;

}

/* myfree takes in a pointer _that was allocated by myfree_ and deallocates it,
 * returning it to the free list (__head) like free(), myfree() returns
 * nothing.  If a user tries to myfree() a buffer that was already freed, was
 * allocated by malloc(), or basically any other use, the behavior is
 * undefined.
 */
void myfree(void *ptr) {

  if (DEBUG) printf("\nIn myfree with pointer %p\n", ptr);

  header_t *header = get_header(ptr); /* get the start of a header from a pointer */

  if (DEBUG) { print_header(header); }

  if (header->magic != HEAPMAGIC) {
    printf("Header is missing its magic number!!\n");
    printf("It should be '%08lx'\n", HEAPMAGIC);
    printf("But it is '%08lx'\n", header->magic);
    printf("The heap is corrupt!\n");
    return;
  }

  /* free the buffer pointed to by ptr!
   * To do this, save the location of the old head (hint, it's __head).
   * Then, change the allocation header_t to a node_t. Point __head
   * at the new node_t and update the new head's next to point to the
   * old head. Voila! You've just turned an allocated buffer into a
   * free region!
   */

  /* save the current __head of the freelist */
  node_t *old_head = __head;

  /* now set the __head to point to the header_t for the buffer being freed */
  __head = (node_t*) header;

  /* set the new head's next to point to the old head that you saved */
  __head->next = old_head;

  /* PROFIT!!! */

}
