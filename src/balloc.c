#include "include/balloc.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#define BALLOC_ALIGN_SIZE(size)                                               \
(((size) + (BALLOC_ALLOCATOR_ALIGNMENT - 1)) &                              \
~(BALLOC_ALLOCATOR_ALIGNMENT - 1))

#define CHUNK_HEADER_SIZE BALLOC_ALIGN_SIZE(sizeof(struct balloc_chunk))
#define BALLOC_HEADER_OFFSET BALLOC_ALIGN_SIZE(                               \
                             sizeof(struct balloc_header_ptr))
#define GET_HEADER(ptr) (struct balloc_header_ptr *)(((uint8_t *)ptr) -       \
                                                     BALLOC_HEADER_OFFSET)

/* needed for realloc to work without requesting the old size from the user */
struct balloc_header_ptr {
    size_t size;
};

static inline struct balloc_chunk *_new_chunk(struct balloc_arena *arena,
                                              size_t chunk_size) {
    
  struct balloc_chunk * chunk = NULL;

  uint8_t *tmp = NULL;
  if (arena->mmap) {
    tmp = mmap(NULL, chunk_size, PROT_READ | PROT_WRITE,
               MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    /* mmap does not return NULL */
    if (tmp == MAP_FAILED) {
        tmp = NULL;
    }
  } else {
    tmp = malloc(chunk_size);
  }

  if (tmp) {
    chunk = (struct balloc_chunk *)tmp;
    chunk->content = tmp + CHUNK_HEADER_SIZE;
    chunk->capacity = chunk_size - CHUNK_HEADER_SIZE;
    chunk->used = 0;
    chunk->next = NULL;
  }
  return chunk;
}

struct balloc_arena *balloc_new(size_t chunk_size) {
  if (chunk_size == 0) {
    chunk_size = BALLOC_DEFAULT_CHUNK_SIZE;
  }
  if (chunk_size <= CHUNK_HEADER_SIZE) {
    return NULL;
  }

  struct balloc_arena *arena =
      malloc(BALLOC_ALIGN_SIZE(sizeof(struct balloc_arena)));
  if (arena) {
    memset(arena, 0, BALLOC_ALIGN_SIZE(sizeof(struct balloc_arena)));
    arena->chunk_size = chunk_size;
    if (chunk_size >= BALLOC_MMAP_TRIGGER_SIZE) {
      arena->mmap = true;
    }
    /* we always want one chunk present */
    struct balloc_chunk *c = _new_chunk(arena, chunk_size);
    if (!c) {
        free(arena);
        return NULL;
    }
    arena->head = c;
    arena->current = c;
  }
  return arena;
}

void balloc_destroy(struct balloc_arena *arena) {
  if(!arena) {
    return;
  }
  struct balloc_chunk *c = NULL, *n = NULL;

  c = arena->head;
  while (c) {
    n = c->next;
    if (arena->mmap) {
      munmap(c, c->capacity + CHUNK_HEADER_SIZE);
    } else {
      free(c);
    }
    c = n;
  }
  n = NULL;

  free(arena);
}

void balloc_reset(struct balloc_arena *arena) {
    if (!arena) { return; }
    arena->current = arena->head;
    for(struct balloc_chunk *c = arena->head; c; c = c->next) { c->used = 0; }
}

void balloc_compact(struct balloc_arena *arena) {
    if (!arena || !arena->current || !arena->current->next) { return; }
    for(struct balloc_chunk *c = arena->current->next; c;) {
        struct balloc_chunk *n = c->next;
        if (arena->mmap) {
          munmap(c, c->capacity + CHUNK_HEADER_SIZE);
        } else {
          free(c);
        }
        c = n;
    }
    arena->current->next = NULL; 
}

static inline struct balloc_chunk *_find_chunk(struct balloc_arena *arena,
                                               size_t size) {
    struct balloc_chunk *c = NULL;
    for(c = arena->current; c; c = c->next) {
      if (c->used <= c->capacity &&  (c->capacity - c->used) >= size) {
        return c;
      }
    }
    return NULL;
}

void *balloc(struct balloc_arena *arena, size_t size)
{
  if (arena == NULL || size == 0) {
    return NULL;
  }

  size_t aligned_data_size = BALLOC_ALIGN_SIZE(size);
  size_t asize = aligned_data_size + BALLOC_HEADER_OFFSET;
  if (asize < size || asize + CHUNK_HEADER_SIZE < size) {
    return NULL;
  }

  struct balloc_chunk *c = _find_chunk(arena, asize);
  if (c == NULL) {
    c = _new_chunk(arena,
                   asize + CHUNK_HEADER_SIZE > arena->chunk_size ?
                   asize + CHUNK_HEADER_SIZE : arena->chunk_size);
    if (!c) { return NULL; }
    c->next = arena->current->next;
    arena->current->next = c;
  }
  arena->current = c;
  struct balloc_header_ptr *h = (struct balloc_header_ptr *)
      ((uint8_t *)c->content + c->used);
  uint8_t *m = c->content + c->used + BALLOC_HEADER_OFFSET;
  h->size = size;
  c->used += asize;

  return m;
}


void *brealloc(struct balloc_arena *arena, void *ptr, size_t size) {
  if(!arena || size == 0) {
    return NULL;
  }
  
  if (ptr == NULL) {
    return balloc(arena, size);
  }

  struct balloc_header_ptr *h = GET_HEADER(ptr);
  void *tmp = balloc(arena, size);
  if (tmp) {
    memcpy(tmp, ptr, size < h->size ? size : h->size);
  }

  return tmp;
}

static inline char *_bstrndup(struct balloc_arena *arena, const char *str,
                       size_t len) {
  char *tmp = NULL;
  size_t real_len = strnlen(str, len);
  tmp = balloc(arena, len + 1);
  if (tmp) {
    memcpy(tmp, str, real_len);
    *(tmp + real_len) = '\0';
  }
  return tmp;
}

char *bstrndup(struct balloc_arena *arena, const char *str, size_t len) {
  if (!arena) { return NULL; }
  if (!str || len == 0) {
    char *tmp = balloc(arena, 1);
    if (tmp) {
      *tmp = '\0';
    }
    return tmp;
  }
  return _bstrndup(arena, str, len);
}

void *bmemdup(struct balloc_arena *arena, void *src, size_t len) {
  if (!arena || !src || len == 0) {
    return NULL;
  }
  void *tmp = balloc(arena, len);
  if (tmp) {
    memcpy(tmp, src, len);
  }
  return tmp;
}
