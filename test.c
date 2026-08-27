#include "src/include/balloc.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static inline size_t _count_chunks(struct balloc_arena *a) {
    size_t i = 0;
    for(struct balloc_chunk *c = a->head; c; c = c->next) { i++; }
    return i;
}

bool test_allocation(struct balloc_arena *a, int *test, int *passed, 
                     int *allocs) {
    size_t chunk = a->chunk_size - BALLOC_ALIGN_SIZE(sizeof(struct balloc_chunk));
    /* it takes 1408 bytes to store that, there is 4064 bytes per-chunk, so
     * I can store 2 per chunk
     */
    chunk = chunk / 3;

    printf("Chunk size %lu\n", chunk);
    (*test)++;
    void *ptr = balloc(a, chunk);
    if (ptr == NULL) {
        fprintf(stderr, "Allocation failed\n");
        return false;
    }
    (*passed)++;

    for (size_t i = 0; i < 30; i++) {
        (*test)++;
        void *ptr = balloc(a, chunk);
        if (ptr == NULL) {
            fprintf(stderr, "Allocation in loop (%ld) failed\n", i);
            return false;
        }
        (*passed)++;
    }
    if (allocs) {
        for (struct balloc_chunk *c = a->head; c; c = c->next) {
            printf("CHUNK %d, size : %lu/used : %lu\n", (*allocs), c->capacity, c->used);
            (*allocs)++;
        }

        (*test)++;
        /* with those settings, this should be 16 chunks, the 1 alloc then
         * the 30 allocs -> 16 chunks, 15 with 2 allocs, 1 with one alloc */
        if (*allocs != 16) {
            fprintf(stderr, "Expected 16 chunks, got %d\n", *allocs);
            return false;
        }
        (*passed)++;
    }
    return true;
}

bool test_reset_succeed(struct balloc_arena *a, int *test, int *passed) {
    (*test)++;
    balloc_reset(a);
    if (a->head != a->current) {
        fprintf(stderr, "Reset did not put current on head\n");
        return false;
    }
    (*passed)++;
    return true;
}

bool test_allocation_after_reset(struct balloc_arena *a, int *test,
                                 int *passed, int allocs)  {
    if (!test_allocation(a, test, passed, NULL))  {
        return false;
    }
    
    (*test)++;
    int i = 0;
    for (struct balloc_chunk *c = a->head; c; c = c->next) {
        i++;
    }
    if (i != allocs) {
        fprintf(stderr, "Should have been %d not %d\n", allocs, i);
        return false;
    }
    (*passed)++;

    return true;
}

bool test_allocation_with_big(struct balloc_arena *a, int *test, int *passed) {
    int allocs = 0;
    balloc_reset(a);
    if (!test_allocation(a, test, passed, &allocs)) {
        return false;
    }
    balloc_reset(a);
    (*test)++;
    void *ptr = balloc(a, 40960);
    if (!ptr) {
        fprintf(stderr, "Allocation of a big chunk failed\n");
        return false;
    }   
    (*passed)++;

    (*test)++;
    int i = 0;
    for (struct balloc_chunk *c = a->head; c; c = c->next) {
        i++;
    }
    if (i != allocs + 1) {
        fprintf(stderr, "Allocating an oversize chunk triggered broken chain "
                "after reset, got %i wanted %i\n", i, allocs + 1);
        return false;
    }
    (*passed)++;
    return true;
}

bool test_simple(struct balloc_arena *a, int *test, int *passed) {
    size_t chunk = a->chunk_size - BALLOC_ALIGN_SIZE(sizeof(struct balloc_chunk));
    printf("Chunk size is %lu\n", chunk);
    (*test)++;
    void * ptr = balloc(a, chunk);
    if (!ptr) { return false; }
    (*passed)++;
    (*test)++;
    if (_count_chunks(a) != 1) { return false; }
    (*passed)++;
    (*test)++;
    balloc_reset(a);
    balloc(a, chunk);
    if (_count_chunks(a) != 1) { return false; }
    (*passed)++;

    (*test)++;
    balloc_reset(a);
    if (!balloc(a, chunk) || !balloc(a, chunk)) { return false; }
    balloc_reset(a);
    if (!balloc(a, chunk) || !balloc(a, chunk)) { return false; }
    if (_count_chunks(a) != 2) { return false; }
    (*passed)++;

    return true;
}

int main(void) {
    int test = 0;
    int passed = 0;
    int allocs = 0;
    struct balloc_arena *a = balloc_new(4096);
    assert(a != NULL);
    printf("+++ Test simple allocation +++\n");
    assert(test_simple(a, &test, &passed) == true);
    printf("+++ Passed +++\n");
    balloc_destroy(a);


    a = balloc_new(4096);
    assert(a != NULL);
    printf("+++ Test simple allocation +++\n");
    assert(test_allocation(a, &test, &passed, &allocs) == true);
    printf("+++ Passed +++\n");
    printf("+++ Test reset +++\n");
    assert(test_reset_succeed(a, &test, &passed) == true);
    printf("+++ Passed +++\n");
    printf("+++ Test allocation after reset +++\n");
    assert(test_allocation_after_reset(a, &test, &passed, allocs) == true);
    printf("+++ Passed +++\n");
    printf("+++ Test allocation with big +++\n");
    assert(test_allocation_with_big(a, &test, &passed) == true);
    printf("+++ Passed +++\n");

    printf("Total test %d, passed %d\n", test, passed);
    balloc_destroy(a);
}
