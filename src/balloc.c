#include "include/balloc.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#ifndef getpagesize
#define getpagesize() 4096
#endif

static inline struct balloc_chunk *_new_chunk(struct balloc_arena *arena,
                                              size_t chunk_size) {
  if (arena->free) {
    struct balloc_chunk *c = NULL, *p = NULL;
    for(c = arena->free; c; c = c->next) {
      if (c->capacity >= chunk_size) {
        break;
      }
      p = c;
    }
    if (c) {
        if (p) { p->next = c->next; }
        else   { arena->free = c->next; }
        c->next = NULL;
        arena->tail->next = c;
        arena->tail = c;
        arena->free_length--;
        arena->used_length++;
        return c;
    }
  }

  size_t ssize = BALLOC_ALIGN_SIZE(sizeof(struct balloc_chunk));
  struct balloc_chunk *chunk = malloc(ssize + chunk_size);
  if (chunk) {
    chunk->content = (uint8_t *)chunk + ssize;
    chunk->capacity = chunk_size;
    chunk->used = 0;
    chunk->next = NULL;
    if (arena->tail)  { arena->tail->next = chunk; }
    arena->tail = chunk;
    arena->used_length++;
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
    memset(arena, 0, BALLOC_ALIGN_SIZE(sizeof(struct balloc_arena)));
    arena->chunk_size = chunk_size;
    if (!_new_chunk(arena, chunk_size)) {
      free(arena);
      return NULL;
    }
    arena->head = arena->tail;
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
  n = NULL;
  c = arena->free;
  while (c) {
    n = c->next;
    free(c);
    c = n;
  }

  free(arena);
}

void balloc_reset(struct balloc_arena *arena) {
  assert(arena != NULL);
  struct balloc_chunk * c = arena->head;
  while(c) {
    c->used = 0;
    c = c->next;
  }
  arena->tail->next = arena->free;
  arena->free = arena->head->next;
  c = arena->head;
  c->next = NULL;
  arena->tail = c;
  arena->head = c;
  arena->free_length += arena->used_length;
  arena->used_length = 0;
}

void balloc_compact(struct balloc_arena *arena) {
    assert(arena != NULL);
    size_t to_remove = arena->free_length / 2;
    struct balloc_chunk *c = arena->free;
    while(c) {
        struct balloc_chunk *n = c->next;
        free(c);
        arena->free_length--;
        to_remove--;
        if (to_remove == 0) {
            arena->free = n;
            break;
        }
        c = n;
    }

}

void *balloc(struct balloc_arena *arena, size_t size) {
  assert(arena != NULL);
  if (size == 0) {
    return NULL;
  }

  size_t aligned_data_size = BALLOC_ALIGN_SIZE(size);
  size_t asize = aligned_data_size + BALLOC_ALIGN_SIZE(sizeof(size_t));
  if (asize < size) {
    return NULL;
  }
  struct balloc_chunk *c = NULL;
  if (arena->tail->used + asize > arena->tail->capacity) {
    c =  _new_chunk(arena, 
                    size > arena->chunk_size ? asize : arena->chunk_size);
  } else {
    c = arena->tail;
  }
  if (!c) {
    return NULL;
  }
  uint8_t *tmp = c->content + c->used + BALLOC_ALIGN_SIZE(sizeof(size_t));
  
  *(size_t *)(tmp - BALLOC_ALIGN_SIZE(sizeof(size_t))) = aligned_data_size;

  c->used += asize;
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

#include <stdio.h>
void balloc_dump_stat(struct balloc_arena *arena) {
    assert(arena);
    struct balloc_chunk * c = arena->head;
    size_t tused = 0;
    size_t tcap = 0;
    size_t ucount = 0;
    while(c) {
        tused += c->used;
        tcap += c->capacity;
        c = c->next;
        ucount++;
    }
    size_t fcount = 0;
    c = arena->free;
    while(c) {
        fcount++;
        c = c->next;
    }
    float tusage = (float)tused / (float)tcap;
    printf("TOTAL used %ld, capacity %ld, USAGE %.2f\n"
           "Used length %ld (%ld), Free length %ld (%ld)\n", 
           tused, tcap, tusage, arena->used_length, ucount,
           arena->free_length, fcount);
}
