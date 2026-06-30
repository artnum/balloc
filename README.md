# ShredArena

It is a growable arena allocator with features for working with sensitive data.
It uses mmap for region (chunk) bigger than 2MiB and malloc for smaller one.

  * Fixed size region (chunk) set at creation `balloc_new()`.
  * Allocation for "secret" with `balloc_sec()`.
  * Reset the whole arena with `balloc_reset()`, chunks are put in a free-list.
  * When allocation larger than original fixed size is requested, a chunk large
    enough is created.
  * Secret allocation are mlock'd and zeroed on reset.
  * Allocation are aligned.
  * A function to compact the free list.


## Where to use it

I use it in a **Web backend**. I needed to handle _sensitive data_ and normal 
data as much as I needed to allocate during the whole request lifetime without
thinking too much of freeing memory.

So this might be useful in **WebAssembly** (I don't know, never done any
WebAssembly).                                            

So when your _lifetimes_ are quite straigtforward and your data usage is easy
to estimate, it works very well.

## Usage

You may want to build into a static library or directly add the code into your
own compilation.

To compile for non-debug static library :
```
$ make DEBUG=0
```

The static libary is into ./build/

## Secret arena

If you need to allocate memory for sensitive data, you have the `*_sec`
functions that will create a chunk for those data. The memory is mlock'd if
the process can do it but will not fail if it can't.

On reset, the _used_ (not the whole chunk) memory is zeroed and put in the
freelist. This chunk can be picked up for standard allocation too.

## Compaction

If the freelist has grown too much, you can halve it with `balloc_compact()`.
