# Bump Allocator

It is a growable arena allocator. 

  * Fixed size region (chunk) set at creation `balloc_new()`.
  * Reset the whole arena with `balloc_reset()`, previous allocated chunks are
    kept and reused next run.
  * When allocation larger than original fixed size is requested, a chunk large
    enough is created.
  * Allocation are aligned.
  * Compactation available, keeps size as low as possible.


## Where to use it

I use it in a **Web backend** , web request has a specific lifetime : read 
incoming data - process accordingly - answer. Most requests have low memory
usage but some requests have a suddent spike of memory usage.
This has been written with that in mind, at the end of the request, you just
call balloc_compact then balloc_reset, keeping memory very low while still
gaining some performance of a bump allocator.

## Usage

You may want to build into a static library or directly add the code into your
own compilation.

To compile for non-debug static library :
```
$ make DEBUG=0
```

The static libary is into ./build/
