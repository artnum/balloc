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
        *allocs =  _count_chunks(a);

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

bool test_bstrndup(struct balloc_arena *a, int *test, int *passed) {
    (*test)++;
    char * ptr = bstrndup(a, "test", 20);
    if (!ptr) { 
        fprintf(stderr, "[%03d] Duplication failed\n", __LINE__);
        return false; 
    }
    (*passed)++;
    (*test)++;
    if (strlen(ptr) != 4) { 
        fprintf(stderr, "[%03d] Wrong length : %lu \"%s\"\n", __LINE__,
                strlen(ptr), ptr);
        return false;
    }
    (*passed)++;
    (*test)++;
    if (strcmp(ptr, "test") != 0) {
        fprintf(stderr, "[%03d] strcmp failed \"%s\"\n", __LINE__, ptr);
        return false; 
    }
    (*passed)++;

    (*test)++;
    ptr = bstrndup(a, "test", 4);
    if (!ptr) {
        fprintf(stderr, "[%03d] Duplication failed\n", __LINE__);
        return false;
    }
    (*passed)++;
    (*test)++;
    if (strlen(ptr) != 4) {
        fprintf(stderr, "[%03d] Wrong length : %lu \"%s\"\n", __LINE__,
                strlen(ptr), ptr);
        return false;
    }
    (*passed)++;
    (*test)++;
    if (strcmp(ptr, "test") != 0) { 
        fprintf(stderr, "[%03d] strcmp failed \"%s\"\n", __LINE__, ptr);
        return false; 
    }
    (*passed)++;

    return true;
}


#define test(y, x) do { (x) ? printf("+++ %s : ok\n", (y)) : \
    printf("+++ %s : failed\n", y); } while(0)

int main(void) {
    int test = 0;
    int passed = 0;
    int allocs = 0;
    struct balloc_arena *a = balloc_new(4096);
    if (a) { 
        
        test("Test simple ", test_simple(a, &test, &passed));
        
        /* The side effect of test_simple is that is test reset/compact :
         * value have been calculated manually and if reset/compact doesn't
         * free all chunks, it breaks tests down the road. This could be 
         * improved.
         */
        balloc_reset(a);
        balloc_compact(a);

        test("Test simple allocation", test_allocation(a, &test, &passed, &allocs));
        test("Test reset", test_reset_succeed(a, &test, &passed));
        test("Test allocation after reset",
             test_allocation_after_reset(a, &test, &passed, allocs));
        test("Test allocation with big",
             test_allocation_with_big(a, &test, &passed));

        test("Test bstrndup", test_bstrndup(a, &test, &passed));
        
        printf("Total test %d, passed %d\n", test, passed);
        balloc_destroy(a);
    } else {
        fprintf(stderr, "balloc_new failed\n");
    }

}
