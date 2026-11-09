/**
 * mymalloc.c - A basic first-fit memory allocator built on sbrk.
 *
 * Assumptions:
 *  - All blocks (free and in use) live in one singly linked list, in the
 *    order they were obtained from sbrk. New blocks are appended at the tail.
 *  - No splitting or coalescing: a reused block keeps its original size.
 *  - The heap never shrinks (no negative sbrk calls).
 *  - Alignment is whatever sbrk provides; no extra alignment is done.
 */
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#include <malloc.h>
#include <stdio.h>
#include <unistd.h>  // sbrk
#include <string.h>  // memset
#include <stdint.h>  // SIZE_MAX, INTPTR_MAX
#include <assert.h>

#include <debug.h> // definition of debug_printf

/** Header stored directly in front of every chunk of user memory. */
typedef struct block {
  size_t size;        // bytes available to the user after this header
  struct block *next; // next block in sbrk order (free or in use)
  int free;           // 1 if the block is free, 0 if in use
} block_t;

#define BLOCK_SIZE sizeof(block_t)

static block_t *head = NULL; // first block obtained from sbrk
static block_t *tail = NULL; // last block, so appending is O(1)

/**
 * Searches the block list from the head for the first free block whose
 * size is at least s bytes (first-fit).
 *
 * s: the number of bytes requested by the user.
 * Returns: a pointer to the matching block's header, or NULL if none fits.
 */
static block_t *find_free_block(size_t s) {
  for (block_t *cur = head; cur != NULL; cur = cur->next) {
    if (cur->free && cur->size >= s) {
      return cur;
    }
  }
  return NULL;
}

/**
 * Requests exactly BLOCK_SIZE + s bytes from the OS with one sbrk call,
 * initializes the header as in use, and appends it to the tail of the list.
 *
 * s: the number of user bytes the new block must hold.
 * Returns: a pointer to the new block's header, or NULL if sbrk fails or
 *          the request is too large to pass to sbrk.
 */
static block_t *request_block(size_t s) {
  if (s > SIZE_MAX - BLOCK_SIZE || s + BLOCK_SIZE > (size_t) INTPTR_MAX) {
    return NULL;
  }

  void *mem = sbrk(BLOCK_SIZE + s);
  if (mem == (void *) -1) {
    return NULL;
  }

  block_t *blk = (block_t *) mem;
  blk->size = s;
  blk->next = NULL;
  blk->free = 0;

  if (tail != NULL) {
    tail->next = blk;
  } else {
    head = blk;
  }
  tail = blk;

  return blk;
}

/**
 * Allocates s bytes, reusing the first free block that fits and calling
 * sbrk only when no free block is large enough.
 *
 * s: the number of bytes to allocate.
 * Returns: a pointer to the user's memory (just past the header), or NULL
 *          if s is 0 or the OS is out of memory.
 */
void *mymalloc(size_t s) {
  if (s == 0) {
    return NULL;
  }

  block_t *blk = find_free_block(s);
  if (blk != NULL) {
    blk->free = 0;
  } else {
    blk = request_block(s);
    if (blk == NULL) {
      return NULL; // sbrk failed
    }
  }

  assert(!blk->free && blk->size >= s);
  debug_printf("Malloc %zu bytes\n", s);

  return (void *) (blk + 1); // user memory starts right after the header
}

/**
 * Allocates memory for an array of nmemb elements of s bytes each, and
 * sets all of it to zero. Gets its memory from mymalloc.
 *
 * nmemb: the number of elements.
 * s: the size of each element in bytes.
 * Returns: a pointer to the zeroed memory, or NULL if nmemb * s overflows,
 *          is 0, or mymalloc fails.
 */
void *mycalloc(size_t nmemb, size_t s) {
  if (nmemb != 0 && s > SIZE_MAX / nmemb) {
    return NULL; // nmemb * s would overflow
  }

  size_t total = nmemb * s;
  void *p = mymalloc(total);
  if (p == NULL) {
    return NULL;
  }

  memset(p, 0, total);
  debug_printf("Calloc %zu bytes\n", total);

  return p;
}

/**
 * Marks the block that owns ptr as free so later mallocs can reuse it.
 * Does nothing if ptr is NULL.
 *
 * ptr: a pointer previously returned by mymalloc or mycalloc.
 */
void myfree(void *ptr) {
  if (ptr == NULL) {
    return;
  }

  block_t *blk = (block_t *) ptr - 1; // the header sits just before ptr
  assert(!blk->free); // catches double frees

  blk->free = 1;
  debug_printf("Freed %zu bytes\n", blk->size);
}
