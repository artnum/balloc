# balloc

A growable **bump / arena allocator** in C. You allocate for the lifetime of a
request (or any other well-bounded scope), then reset. Individual `free` is
not supported.

Typical use: a web request — read, process, answer — then
`balloc_compact()` + `balloc_reset()` and reuse the same arena. Nested
work inside a run can be undone with **mark / rewind** tokens.

License: **MIT**. See [LICENSE](LICENSE). Copyright 2026 Etienne Bagnoud.

## AI disclosure

README.md is from Grok. Code review skills have been used to fix code. Code
is human-written. Commits from Grok are always under that name.

## How it works

The arena is a linked list of **chunks**. `balloc_new()` creates the first
chunk immediately and keeps it until `balloc_destroy()`. Further chunks are
appended when an allocation does not fit in the current one. If the request
is larger than the arena’s default chunk size, a chunk big enough for that
request is created.

Allocations bump a cursor (`current`) inside a chunk. Each object is prefixed
with a small header (the requested size) so `brealloc` / `balloc_get_size`
can work without the caller passing the old length. Pointers are aligned to
`BALLOC_ALLOCATOR_ALIGNMENT` (by default `_Alignof(max_align_t)`).

There is no per-allocation free. Memory is reclaimed in bulk:

| Call | Effect |
|---|---|
| `balloc_reset()` | Rewinds `current` to the first chunk and sets every chunk’s `used` to 0. Chunks stay mapped and are reused on the next run. Existing pointers **and mark tokens** from this arena are invalid. |
| `balloc_compact()` | Frees every chunk **after** `current`. The first chunk is never released. After a reset, `current` is the first chunk, so compact leaves a single chunk. Before a reset, `current` is the high-water mark of this run: leftover chunks from a larger previous run are dropped. Compact that actually frees a tail also invalidates all mark tokens. A no-op compact (no tail) does not. |
| `balloc_destroy()` | Unmaps/frees every chunk and the arena. |
| `balloc_mark()` / `balloc_rewind()` | See [Mark and rewind](#mark-and-rewind). |

End of a work loop / request:

```c
balloc_compact(arena);  /* drop unused tail, keep this run’s chunks */
balloc_reset(arena);    /* rewind for the next run; kills all marks */
```

## Mark and rewind

`balloc_mark()` snapshots the bump cursor and returns a **token**
(`struct balloc_mark`). Treat the token as opaque. `balloc_rewind(arena, mark)`
restores that cursor if the token is still valid.

Tokens are **LIFO**:

- Only the most recently issued still-valid token can be rewound.
- Rewinding it makes the previous token live again.
- Rewinding an older token while a newer one exists fails (`false`) and leaves
  the newer token valid.

`balloc_reset()` always invalidates every token. `balloc_compact()` invalidates
every token when it frees a tail (`epoch` is incremented). A compact that
does nothing (no chunk after `current`) does not invalidate tokens.

```c
struct balloc_mark outer = balloc_mark(arena);
/* … allocations … */
struct balloc_mark inner = balloc_mark(arena);
/* … more allocations … */
balloc_rewind(arena, inner);   /* ok */
balloc_rewind(arena, outer);   /* ok */

/* this would have failed if inner had not been rewound first: */
/* balloc_rewind(arena, outer); */
```

A failed mark (NULL arena) returns a zeroed token; rewind of that token
returns `false`.

## When to use it

Use it when lifetimes are simple: many allocations, one reset. A request
handler, a parser, a frame, a job. Most runs stay inside the first few
chunks; a spike grows the list, compact brings it back. Mark/rewind is for
a nested attempt inside that run (parse a value, fail, drop those allocs).

Do **not** use it if you need to free objects independently, or share an
arena across threads without your own lock.

The library is not thread-safe. One arena, one thread, or external
synchronization.

`mmap` is optional. On Unix and macOS it is on by default; on Windows it is
off and every chunk uses `malloc`. You can force either side at compile time
(see [Compile-time options](#compile-time-options)).

## Build

```
$ make          # debug: -O0 -g, static lib in ./build/balloc.a
$ make DEBUG=0  # -O2
$ make test     # three ASan runs of test.c (needs clang-21 by default)
$ make clean
```

`make test` compiles and runs the suite three ways:

1. Default auto-detect (`BALLOC_HAVE_MMAP` as on this platform)
2. Force mmap for every arena (`-DBALLOC_MMAP_TRIGGER_SIZE=0`)
3. Disable mmap (`-DBALLOC_HAVE_MMAP=0`, malloc only)

You can also compile the sources into your own project:

```
cc -I src/include src/balloc.c your_app.c -o your_app
```

Header: `src/include/balloc.h`. With mmap enabled you need a POSIX
`mmap`/`munmap`. Without it, `malloc`/`free` are enough.

## Usage

```c
#include "balloc.h"

struct balloc_arena *arena = balloc_new(0);  /* 0 → BALLOC_DEFAULT_CHUNK_SIZE (4 KiB) */
if (!arena) {
    /* OOM or chunk_size too small */
    return 1;
}

char *name = bstrdup(arena, "Etienne");
void *buf = balloc(arena, 128);
char *snip = bstrndup(arena, "hello world", 5);  /* "hello" */

struct balloc_mark m = balloc_mark(arena);
void *tmp = balloc(arena, 64);
if (!/* ok */) {
    balloc_rewind(arena, m);  /* tmp is invalid; name/buf/snip still live */
}

balloc_compact(arena);
balloc_reset(arena);
/* name, buf, snip, and m are invalid */

balloc_destroy(arena);
```

`balloc`, `balloc_new`, and the helpers return `NULL` on failure. `balloc`
also returns `NULL` if `arena` is `NULL` or `size` is 0. `balloc_rewind`
returns `false` if the token is invalid.

## API

| Function | Role |
|---|---|
| `balloc_new(chunk_size)` | Create an arena and its first chunk. `chunk_size == 0` uses `BALLOC_DEFAULT_CHUNK_SIZE`. Returns `NULL` if `chunk_size` is too small to hold a chunk header, or on allocation failure. |
| `balloc_destroy(arena)` | Free all chunks and the arena. `NULL` is a no-op. |
| `balloc_reset(arena)` | Rewind for reuse. Chunks are kept. Invalidates all mark tokens. |
| `balloc_compact(arena)` | Free chunks after `current`. The first chunk always remains. Invalidates all mark tokens if a tail was freed. |
| `balloc(arena, size)` | Bump-allocate `size` bytes. Grows the list if needed. |
| `brealloc(arena, ptr, size)` | Allocate a new block and `memcpy` `min(old, new)` bytes. Does not free the old block (arena lifetime). `ptr == NULL` is `balloc`. `size == 0` returns `NULL`. Passing a pointer that did not come from this allocator is undefined (typically a crash). |
| `balloc_get_size(ptr)` | Size originally requested for `ptr`. Foreign pointers are undefined (typically a crash). `NULL` → `0`. |
| `bmemdup(arena, src, len)` | Copy `len` bytes. `src == NULL` or `len == 0` → `NULL`. |
| `bstrndup(arena, str, len)` | POSIX `strndup`: at most `len` bytes, stop at the first NUL, always terminate. `str == NULL` or `len == 0` → empty string. |
| `bstrdup(arena, str)` | `bstrndup` of the whole string (`strlen`). `str == NULL` → empty string. |
| `balloc_mark(arena)` | Snapshot the cursor. Returns a token; a zeroed token if `arena` is NULL. |
| `balloc_rewind(arena, mark)` | Restore that snapshot if `mark` is the current LIFO token. `true` on success. |

## Compile-time options

Define these before including the header / compiling `balloc.c`:

| Macro | Default | Meaning |
|---|---|---|
| `BALLOC_HAVE_MMAP` | `1` on Unix and macOS (including Cygwin); `0` on native Windows and elsewhere | Master switch. `0` compiles out `mmap`/`munmap`: every chunk uses `malloc`/`free`, and `struct balloc_arena` has no `mmap` field. Set `-DBALLOC_HAVE_MMAP=0` to force a malloc-only build, or `=1` if your toolchain is Unix-like but not detected. |
| `BALLOC_DEFAULT_CHUNK_SIZE` | `4096` | Used when `balloc_new(0)` is called. |
| `BALLOC_MMAP_TRIGGER_SIZE` | `2 * 1024 * 1024` | Only defined when `BALLOC_HAVE_MMAP` is 1. If the arena’s **default** `chunk_size` is ≥ this, every chunk of that arena is obtained with `mmap`. Otherwise every chunk uses `malloc`. The choice is made once at `balloc_new`, not per allocation. An oversized alloc in a malloc arena still uses `malloc`. `-DBALLOC_MMAP_TRIGGER_SIZE=0` makes every arena use mmap (used by `make test`). |
| `BALLOC_ALLOCATOR_ALIGNMENT` | `_Alignof(max_align_t)` | Alignment of returned pointers and of internal headers. Must be a power of two. |

## Layout

```
src/balloc.c           implementation
src/include/balloc.h   public API
test.c                 small self-contained tests
LICENSE                MIT
```

`struct balloc_arena` and `struct balloc_chunk` are defined in the header
(the tests walk the list). Treat them as read-only unless you are debugging.
Treat `struct balloc_mark` as opaque.

## License

MIT License. Copyright 2026 Etienne Bagnoud. Full text in [LICENSE](LICENSE).
