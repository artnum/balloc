#ifndef BALLOC_H__
#define BALLOC_H__ 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * Compile time option, default size of chunks. Default to 4KiB
 */
#ifndef BALLOC_DEFAULT_CHUNK_SIZE
    #define BALLOC_DEFAULT_CHUNK_SIZE 4096
#endif /* BALLOC_DEFAULT_CHUNK_SIZE */

/**
 * Compile time option, trigger the use of mmap if an arena is created with
 * chunk size equal or bigger than this value, default to 2MiB
 */
#ifndef BALLOC_MMAP_TRIGGER_SIZE
    #define BALLOC_MMAP_TRIGGER_SIZE (2 * 1024 * 1024)
#endif /* BALLOC_MMAP_TRIGGER_SIZE */

/**
 * Compile time option, to set alignment
 */
#ifndef BALLOC_ALLOCATOR_ALIGNMENT
    #define BALLOC_ALLOCATOR_ALIGNMENT _Alignof(max_align_t)
#endif /* BALLOC_ALLOCATOR_ALIGNMENT */


struct balloc_chunk {
  uint8_t *content;
  size_t capacity;
  size_t used;
  void *next;
};

struct balloc_arena {
  struct balloc_chunk *head;
  struct balloc_chunk *current;
  size_t chunk_size;
  bool mmap;
};

/**
 * Create a new arena.
 * 
 * Arena is create on the heap and allocation are done either with mmap or 
 * malloc, the compile time option BALLOC_MMAP_TRIGGER_SIZE (default to 2MiB),
 * let you choose for your project which arena is with malloc and which is 
 * with mmap.
 * If an allocation is bigger that the mmap trigger size on a arena initialized
 * with malloc allocation, the allocation will be done with malloc, the trigger
 * is for the whole arena on startup, not each allocation.
 *
 * @param chunk_size Size of the chunks, if pass zero default to
 *                   BALLOC_DEFAULT_CHUNK_SIZE, a compile time option, defaults
 *                   to 4096.
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
 * Compact arena
 *
 * The arena is a built with a linked list of chunks. Each allocation that
 * would overflow the chunk_size set with balloc_new, it create a new chunk,
 * add it to the list set "current chunk" as that one. When you call 
 * balloc_reset, this list is rewinded and next allocations use those previous
 * chunks.
 * Compact is a way to free all chunks passed the current one. If you call this
 * right after balloc_reset, you free everything but the first chunk. If you 
 * call before balloc_reset you basically tailor the memory usage to the
 * smallest version of your run (you have work loop, the work loop has a
 * variable memory usage per work, at the end of each iteration, you compact
 * and reset, so the chunks staying in the list is always the least amount
 * posssible).
 * 
 * @param arena Arena to compact
 */
void balloc_compact(struct balloc_arena *arena); 
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
 * Reallocate some memory into the arena.
 * 
 * @param arena The arena to allocate to.
 * @param ptr   The pointer to reallocate.
 * @param len   The new size.
 * 
 * @return Pointer to the reallocated memory or NULL in case of failure.
 * 
 * @note This function do a simple balloc then memcpy.
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
 * Duplicate a string.
 *
 * @param arena The arena to duplicate to.
 * @param str   The string to duplicate.
 *
 * @return Pointer to the duplicated string or NULL in case of failure.
 *
 * @note If a null string is passed, it returns a valid empty string.
 */
static inline char *bstrdup(struct balloc_arena *arena, char *str) {
    return bstrndup(arena, str, str ? strlen(str) : 0);
}

#endif /* BALLOC_H__ */
