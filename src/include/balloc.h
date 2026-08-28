#ifndef BALLOC_H__
#define BALLOC_H__ 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * If BALLOC_HAVE_MAP is not set, we try to detect if we have mmap. If we don't
 * mmap is removed and the arena structure is a bit smaller
 */
#ifndef BALLOC_HAVE_MMAP
#if defined(_WIN32) && !defined(__CYGWIN__)
    #define BALLOC_HAVE_MMAP 0
#elif defined(__unix__) || defined(__unix) \
   || (defined(__APPLE__) && defined(__MACH__))
    #define BALLOC_HAVE_MMAP 1
#else
    #define BALLOC_HAVE_MMAP 0
#endif
#endif /* BALLOC_HAVE_MMAP */

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
#if BALLOC_HAVE_MMAP
#ifndef BALLOC_MMAP_TRIGGER_SIZE
    #define BALLOC_MMAP_TRIGGER_SIZE (2 * 1024 * 1024)
#endif /* BALLOC_MMAP_TRIGGER_SIZE */
#endif /* BALLOC_HAVE_MMAP */

/**
 * Compile time option, to set alignment
 */
#ifndef BALLOC_ALLOCATOR_ALIGNMENT
    #define BALLOC_ALLOCATOR_ALIGNMENT _Alignof(max_align_t)
#endif /* BALLOC_ALLOCATOR_ALIGNMENT */

struct balloc_stats {
  size_t chunk_count;
  size_t requested_size;
  size_t allocated_size;
};

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
  size_t epoch;

#if BALLOC_HAVE_MMAP
  bool mmap;
#endif /* BALLOC_HAVE_MMAP */
};

struct balloc_mark {
    struct balloc_chunk *where;
    size_t used;
    size_t at;
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
void *bmemdup(struct balloc_arena *arena, const void *src, size_t len);

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
char *bstrdup(struct balloc_arena *arena, const char *str); 
/**
 * Get current size of memory
 *
 * @param ptr Pointer to memory.
 *
 * @return Current size.
 *
 * @note If pass a pointer that has not been allocated by this allocator, you
 * will likely get a crash.
 *
 */
size_t balloc_get_size(const void *ptr);
/**
 * Create a mark token on allocation 
 *
 * A token allow to rewind back at the specific moment in the allocation chain.
 * If you create several tokens, they act as LIFO, last issued token is the 
 * first to be rewinded if you rewind any token before any other tokens, all
 * tokens coming after are invalided.
 * balloc_reset/balloc_compact invalidate all tokens.
 *
 * @param arena The arena to put a mark on.
 *
 * @return A mark token.
 */
struct balloc_mark balloc_mark(struct balloc_arena *arena);
/**
 * Rewind to the mark
 *
 * @param arena The arena to rewind
 * @param mark  The mark you want to rewind to.
 *
 * @return True if success, false otherwise
 */
bool balloc_rewind(struct balloc_arena *arena, struct balloc_mark mark); 
#endif /* BALLOC_H__ */
