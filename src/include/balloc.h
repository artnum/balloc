#ifndef BALLOC_H__
#define BALLOC_H__ 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef BALLOC_ALLOCATOR_ALIGNMENT
#define BALLOC_ALLOCATOR_ALIGNMENT sizeof(max_align_t)
#endif
#define BALLOC_ALIGN_SIZE(size)                                               \
  (((size) + (BALLOC_ALLOCATOR_ALIGNMENT - 1)) &                              \
   ~(BALLOC_ALLOCATOR_ALIGNMENT - 1))

struct balloc_chunk {
  uint8_t *content;
  size_t capacity;
  size_t used;
  void *next;
  bool locked;
};

struct balloc_arena {
  struct balloc_chunk *head;
  struct balloc_chunk *tail;
  struct balloc_chunk *sec_head;
  struct balloc_chunk *sec_tail;
  struct balloc_chunk *free;
  size_t free_length;
  size_t used_length;
  size_t chunk_size;
  bool mmap;
};

/**
 * Create a new arena.
 *
 * @param chunk_size Size of the chunks, if not set it uses getpagesize, if 
 *                   not available it uses 4096bytes
 * 
 * @return An arena or NULL in case of failure
 */
struct balloc_arena *balloc_new(size_t chunk_size);
/**
 * Destroy an arena.
 * 
 * @param arena Destroy the arena, free every chunk
 */
void balloc_destroy(struct balloc_arena *arena);
/**
 * Reset an arena.
 * 
 * @param arena Set all chunks to free so they can be reused
 */
void balloc_reset(struct balloc_arena *arena); 
/**
 * Compact free list.
 * 
 * @param arena Reduce the current free list by half. The current free list is
 *              not the same size before and after a reset, so if you want to 
 *              reduce the full chunk size, call it after balloc_reset.
 */
void balloc_compact(struct balloc_arena *arena); 
/**
 * Print some stats about current arena.
 * 
 * @param arena The arena you want to see stats
 */
void balloc_dump_stat(struct balloc_arena *arena); 
/**
 * Allocate some memory into the arena.
 * 
 * @param arena The arena to allocate to
 * @param size  Quantity of bytes you want. A new chunk is created if the
 *              current is too small, if the size is bigger than chunk_size a
 *              whole chunk of that size will be created.
 * 
 * @return Pointer to the allocated memory or NULL in case of failure.
 */
void *balloc(struct balloc_arena *arena, size_t size);
/**
 * Allocate some memory into the secret arena.
 * It works as balloc but the memory allocated with *_sec variant will be 
 * shred on reset or destroy. It is designed to store passphrase and other
 * sensitive material in it.
 * It will try to mlock the memory but won't fail if it cannot do it, so chunk
 * might be swapped to disk.
 */
void *balloc_sec(struct balloc_arena *arena, size_t size);
/**
 * Reallocate some memory into the arena.
 * 
 * @param arena The arena to allocate to.
 * @param ptr   The pointer to reallocate.
 * @param len   The new size.
 * 
 * @return Pointer to the reallocated memory or NULL in case of failure.
 * 
 * @note This function do a simple balloc then memcpy.
 * @note This function doesn't have a _sec equivalent as it the original
 *       pointer is in a sec arena, the new one will, if it is not, the new
 *       one will not be.
 * @warn If you pass a pointer that is not from a balloc arena, segfault is
 *       guarantee, there is not attempt to secure against that.
 */
void *brealloc(struct balloc_arena *arena, void *ptr, size_t size);
/**
 * Duplicate some memory into the arena.
 * 
 * @param arena The arena to duplicate to.
 * @param src   The source to duplicate.
 * @param len   The amount you want to duplicate in bytes.
 *
 * @return Pointer to the duplicated memory or NULL in case of failure.
 */
void *bmemdup(struct balloc_arena *arena, void *src, size_t len);
/**
 * Secure version of bmemdup
 */
void *bmemdup_sec(struct balloc_arena *arena, void *src, size_t len);

/**
 * Duplicate a string.
 *
 * @param arena The arena to duplicate to.
 * @param str   The string to duplicate.
 * @param len   The amount you want to duplicate in bytes.
 *
 * @return Pointer to the duplicated string or NULL in case of failure.
 * 
 * @note If a null string is passed, it returns a valid empty string.
 */
char *bstrndup(struct balloc_arena *arena, const char *str, size_t len);
/**
 * Secure version of bstrndup
 */
char *bstrndup_sec(struct balloc_arena *arena, const char *str, size_t len); 
/**
 * Duplicate a string.
 *
 * @param arena The arena to duplicate to.
 * @param str   The string to duplicate.
 *
 * @return Pointer to the duplicated string or NULL in case of failure.
 *
 * @note If a null string is passed, it returns a valid empty string.
 */
#define bstrdup(arena, str)                                                   \
  bstrndup((arena), (str), (str) != NULL ? strlen(str) : 0)
/**
 * Secure version of bstrdup_sec
 */
#define bstrdup_sec(arena, str)                                               \
  bstrndup_sec((arena), (str), (str) != NULL ? strlen(str) : 0)

#define boldsize(tmp)                                                         \
  ((tmp != NULL)                                                              \
       ? *(size_t *)((uint8_t *)(tmp) - BALLOC_ALIGN_SIZE(sizeof(size_t)))    \
       : 0)

#endif /* BALLOC_H__ */
