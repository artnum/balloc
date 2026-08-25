#include "include/balloc.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>

#ifndef getpagesize
#define getpagesize() 4096
#endif

struct balloc_header_ptr {
    size_t size;
};

#ifndef BALLOC_MMAP_TRIGGER_SIZE
#define BALLOC_MMAP_TRIGGER_SIZE (2 * 1024 * 1024)
#endif

#define CHUNK_HEADER_SIZE BALLOC_ALIGN_SIZE(sizeof(struct balloc_chunk))
#define BALLOC_HEADER_OFFSET BALLOC_ALIGN_SIZE(                               \
                             sizeof(struct balloc_header_ptr))
#define GET_HEADER(ptr) (struct balloc_header_ptr *)(((uint8_t *)ptr) -       \
                                                     BALLOC_HEADER_OFFSET)

static inline struct balloc_chunk *_new_chunk(struct balloc_arena *arena,
                                              struct balloc_chunk **target,
                                              size_t chunk_size) {
  if (arena->free) {
    struct balloc_chunk *c = NULL, *p = NULL;
    for(c = arena->free; c; c = c->next) {
      if (c->capacity >= chunk_size - CHUNK_HEADER_SIZE) {
        break;
      }
      p = c;
    }
    if (c) {
        if (p) { p->next = c->next; }
        else   { arena->free = c->next; }
        c->used = 0;
        c->next = NULL;
        if (*target) { (*target)->next = c; }
        *target = c;
        arena->free_length--;
        arena->used_length++;
        return c;
    }
  }
  
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
    if (*target) { (*target)->next = chunk; }
    *target = chunk;
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
    if (chunk_size >= BALLOC_MMAP_TRIGGER_SIZE) {
      arena->mmap = true;
    }
  }
  return arena;
}

void balloc_destroy(struct balloc_arena *arena) {
  assert(arena != NULL);
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

  c = arena->free;
  while (c) {
    n = c->next;
    if (arena->mmap) {
      munmap(c, c->capacity + CHUNK_HEADER_SIZE);
    } else {
      free(c);
    }
    c = n;
  }

  free(arena);
}

void balloc_reset(struct balloc_arena *arena) {
  assert(arena != NULL);
  struct balloc_chunk * c = NULL;

  if (arena->head) {
    c = arena->head;
    while(c) {
      c->used = 0;
      c = c->next;
    }
    arena->tail->next = arena->free;
    arena->free = arena->head;
  }
  
  arena->tail = NULL;
  arena->head = NULL;

  arena->free_length += arena->used_length;
  arena->used_length = 0;
}

void balloc_compact(struct balloc_arena *arena) {
    assert(arena != NULL);
    size_t to_remove = arena->free_length / 2;
    struct balloc_chunk *c = arena->free;
    while(to_remove && c) {
        struct balloc_chunk *n = c->next;
        if (arena->mmap) {
          munmap(c, c->capacity + CHUNK_HEADER_SIZE);
        } else {
          free(c);
        }
        arena->free_length--;
        to_remove--;
        if (to_remove == 0) {
            arena->free = n;
            break;
        }
        c = n;
    }

}

static inline void *_balloc(struct balloc_arena *arena,
                            struct balloc_chunk **head,
                            struct balloc_chunk **tail, 
                            struct balloc_header_ptr **ptr,
                            size_t size)
{
  size_t aligned_data_size = BALLOC_ALIGN_SIZE(size);
  size_t asize = aligned_data_size + BALLOC_HEADER_OFFSET;
  if (asize < size || asize + CHUNK_HEADER_SIZE < size) {
    return NULL;
  }
  struct balloc_chunk *c = NULL;
  if (*tail == NULL || ((*tail)->used + asize > (*tail)->capacity)) {
    c =  _new_chunk(arena,
                    tail,
                    asize + CHUNK_HEADER_SIZE > arena->chunk_size ?
                        asize + CHUNK_HEADER_SIZE :
                        arena->chunk_size);
  } else {
    c = *tail;
  }
  if (!c) {
    return NULL;
  }

  if (!*head) {
    *head = c;
  }
  struct balloc_header_ptr *h = (struct balloc_header_ptr *)
      ((uint8_t *)c->content + c->used);
  uint8_t *m = c->content + c->used + BALLOC_HEADER_OFFSET;
  h->size = size;
  *ptr = h;
  c->used += asize;

  return m;
}

void *balloc(struct balloc_arena *arena, size_t size) {
  assert(arena != NULL);
  if (size == 0) {
    return NULL;
  }
  struct balloc_header_ptr *ptr = NULL;
  void * m = _balloc(arena, &arena->head, &arena->tail, &ptr, size);

  return m;
}

void *brealloc(struct balloc_arena *arena, void *ptr, size_t size) {
  assert(arena != NULL);
  if (size == 0) {
    return NULL;
  }
  if (ptr == NULL) {
    return balloc(arena, size);
  }

  struct balloc_header_ptr *h = GET_HEADER(ptr);
  void *tmp = balloc(arena, size);
  if (tmp) {
    if (size < h->size) {
      memcpy(tmp, ptr, size);
    } else {
      memcpy(tmp, ptr, h->size);
    }
  }

  return tmp;
}

static inline char *_bstrndup(struct balloc_arena *arena, const char *str,
                       size_t len) {
  char *tmp = NULL;
  tmp = balloc(arena, len + 1);
  if (tmp) {
    memcpy(tmp, str, len);
    *(tmp + len) = '\0';
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
  return _bstrndup(arena, str, len);
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
