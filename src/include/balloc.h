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
#define BALLOC_ALIGN_SIZE(size)                                                \
  (((size) + (BALLOC_ALLOCATOR_ALIGNMENT - 1)) &                               \
   ~(BALLOC_ALLOCATOR_ALIGNMENT - 1))

struct balloc_chunk {
  uint8_t *content;
  size_t capacity;
  size_t used;
  void *next;
};

struct balloc_arena {
  struct balloc_chunk *head;
  struct balloc_chunk *tail;
  size_t chunk_size;
};

struct balloc_arena *balloc_new(size_t chunk_size);
void balloc_destroy(struct balloc_arena *arena);
void *balloc(struct balloc_arena *arena, size_t size);
void *brealloc(struct balloc_arena *arena, void *ptr, size_t size);
void *bmemdup(struct balloc_arena *arena, void *src, size_t len);
/**
 * Duplicate a string
 *
 * \return A pointer to a string, if a null pointer is passed, it returns an
 * empty string
 */
char *bstrndup(struct balloc_arena *arena, const char *str, size_t len);
#define bstrdup(arena, str)                                                    \
  bstrndup((arena), (str), (str) != NULL ? strlen(str) : 0)
#define boldsize(tmp)                                                          \
  ((tmp != NULL)                                                               \
       ? *(size_t *)((uint8_t *)(tmp) - BALLOC_ALIGN_SIZE(sizeof(size_t)))     \
       : 0)

#endif /* BALLOC_H__ */
