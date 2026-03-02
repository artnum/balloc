#include "include/balloc.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#ifndef getpagesize
#define getpagesize() 4096
#endif

static inline struct balloc_chunk *_new_chunk(size_t chunk_size) {
  size_t ssize = BALLOC_ALIGN_SIZE(sizeof(struct balloc_chunk));
  struct balloc_chunk *chunk = malloc(ssize + chunk_size);
  if (chunk) {
    chunk->content = (uint8_t *)chunk + ssize;
    chunk->capacity = chunk_size;
    chunk->used = 0;
    chunk->next = NULL;
  }
  return chunk;
}

struct balloc_arena *balloc_new(size_t chunk_size) {
  if (chunk_size == 0) {
    chunk_size = getpagesize();
  }
  struct balloc_arena *arena =
      malloc(BALLOC_ALIGN_SIZE(sizeof(struct balloc_arena)));
  if (arena) {
    arena->head = _new_chunk(chunk_size);
    if (!arena->head) {
      free(arena);
      return NULL;
    }
    arena->tail = arena->head;
    arena->chunk_size = chunk_size;
  }
  return arena;
}

void balloc_destroy(struct balloc_arena *arena) {
  assert(arena != NULL);
  struct balloc_chunk *c = NULL, *n = NULL;

  c = arena->head;
  while (c) {
    n = c->next;
    free(c);
    c = n;
  }
  free(arena);
}

void *balloc(struct balloc_arena *arena, size_t size) {
  assert(arena != NULL);
  if (size == 0) {
    return NULL;
  }

  size_t asize = BALLOC_ALIGN_SIZE(size) + BALLOC_ALIGN_SIZE(sizeof(size_t));
  if (asize < size) {
    return NULL;
  }
  if (arena->tail->used + asize >= arena->tail->capacity) {
    arena->tail->next =
        _new_chunk(asize > arena->chunk_size ? asize : arena->chunk_size);
    if (arena->tail->next) {
      arena->tail = arena->tail->next;
    }
  }
  uint8_t *tmp = arena->tail->content + arena->tail->used +
                 BALLOC_ALIGN_SIZE(sizeof(size_t));
  *(size_t *)(tmp - BALLOC_ALIGN_SIZE(sizeof(size_t))) = asize;
  arena->tail->used += asize;
  return tmp;
}

void *brealloc(struct balloc_arena *arena, void *ptr, size_t size) {
  assert(arena != NULL);
  if (size == 0) {
    return NULL;
  }
  if (ptr == NULL) {
    return balloc(arena, size);
  }

  size_t psize =
      *(size_t *)((uint8_t *)ptr - BALLOC_ALIGN_SIZE(sizeof(size_t)));
  void *tmp = balloc(arena, size);
  if (tmp) {
    memcpy(tmp, ptr, psize);
  }

  return tmp;
}

char *bstrndup(struct balloc_arena *arena, const char *str, size_t len) {
  assert(arena != NULL);
  if (len == 0 || str == NULL) {
    char *tmp = balloc(arena, 1);
    if (tmp) {
      *tmp = '\0';
    }
    return tmp;
  }
  char *tmp = balloc(arena, len + 1);
  if (tmp) {
    memcpy(tmp, str, len);
    *(tmp + len) = '\0';
  }
  return tmp;
}

void *bmemdup(struct balloc_arena *arena, void *src, size_t len) {
  assert(arena);
  if (src == NULL || len == 0) {
    return NULL;
  }
  void *tmp = balloc(arena, len);
  if (tmp) {
    memcpy(tmp, src, len);
  }
  return tmp;
}
