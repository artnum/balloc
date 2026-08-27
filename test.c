#include "src/include/balloc.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

/* this opaque */
#define BALLOC_ALIGN_SIZE(size)                                               \
(((size) + (BALLOC_ALLOCATOR_ALIGNMENT - 1)) &                              \
~(BALLOC_ALLOCATOR_ALIGNMENT - 1))

struct balloc_chunk {
  uint8_t *content;
  size_t capacity;
  size_t used;
  void *next;
};

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
    /* allocation 3000 will fit ONE chunk without having enough space to fit
     * a second allocation
     */
    size_t chunk = 3000;
    (*test)++;
    void * ptr = balloc(a, chunk);
    if (!ptr) { return false; }
    (*passed)++;
    (*test)++;
    if (_count_chunks(a) != 1) {
        fprintf(stderr, "[%03d] Wrong chunk count : %zu\n", __LINE__, 
                _count_chunks(a));
        return false; 
    }
    (*passed)++;
    (*test)++;
    balloc_reset(a);
    balloc(a, chunk);
    if (_count_chunks(a) != 1) {
        fprintf(stderr, "[%03d] Wrong chunk count : %zu\n", __LINE__, 
                _count_chunks(a));
        return false; 
    }
    (*passed)++;

    (*test)++;
    balloc_reset(a);
    if (!balloc(a, chunk) || !balloc(a, chunk)) { return false; }
    balloc_reset(a);
    if (!balloc(a, chunk) || !balloc(a, chunk)) { return false; }
    if (_count_chunks(a) != 2) { 
        fprintf(stderr, "[%03d] Wrong chunk count : %zu\n", __LINE__, 
                _count_chunks(a));
        return false; 
    }
    (*passed)++;

    return true;
}

bool test_bstrdup(struct balloc_arena *a, int *test, int *passed) {
    (*test)++;
    if (!bstrdup(a, NULL)) {
       fprintf(stderr, "[%03d] Failed on NULL string\n", __LINE__);
       return false; 
    }
    (*passed)++;
    (*test)++;
    if (bstrdup(NULL, "string")) {
       fprintf(stderr, "[%03d] Succeed on NULL arena\n", __LINE__);
       return false; 
    }
    (*passed)++;


    (*test)++;
    char * ptr = bstrdup(a, "test");
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
    ptr = bstrdup(a, "test");
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

bool test_bstrndup(struct balloc_arena *a, int *test, int *passed) {
    (*test)++;
    if (!bstrndup(a, NULL, 100)) {
       fprintf(stderr, "[%03d] Failed on NULL string\n", __LINE__);
       return false; 
    }
    (*passed)++;
    (*test)++;
    if (!bstrndup(a, "string", 0)) {
       fprintf(stderr, "[%03d] Failed on 0 size string\n", __LINE__);
       return false; 
    }
    (*passed)++;
    (*test)++;
    if (bstrndup(NULL, "string", 10)) {
       fprintf(stderr, "[%03d] Succeed on NULL arena\n", __LINE__);
       return false; 
    }
    (*passed)++;

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

bool test_null_0_alloc(struct balloc_arena *a, int *test, int *passed) {
    (*test)++;
    if (balloc(NULL, 100)) {
        fprintf(stderr, "[%03d] balloc on a NULL arena suceeded\n", __LINE__);
        return false;
    }
    (*passed)++;
    (*test)++;
    if (balloc(a, 0)) {
        fprintf(stderr, "[%03d] balloc on with 0 size suceeded\n", __LINE__);
        return false;
    }
    (*passed)++;
    (*test)++;
    if (balloc(NULL, 0)) {
        fprintf(stderr, "[%03d] balloc on with 0 size and NULL arena "
                "suceeded\n", __LINE__);
        return false;
    }
    (*passed)++;
    return true;
}

bool test_compact(struct balloc_arena *a, int *test, int *passed) {
    balloc_reset(a);
    (*test)++;
    balloc_compact(a);
    if (_count_chunks(a) != 1) {
        fprintf(stderr, "[%03d] Count chunk is %zu instead of 1\n", __LINE__,
                _count_chunks(a));
        return false;
    }
    (*passed)++;
    (*test)++;
    /* this should skip first block and create a big block */
    if (!balloc(a, 40960)) {
        fprintf(stderr, "[%03d] Big block failed\n", __LINE__);
        return false;
    }
    if (_count_chunks(a) != 2) {
        fprintf(stderr, "[%03d] Count chunk is %zu instead of 2\n", __LINE__,
                _count_chunks(a));
        return false;
    }
    balloc_compact(a);
    if (_count_chunks(a) != 2) {
        fprintf(stderr, "[%03d] Count chunk is %zu instead of 2 after "
                "compacting without reset\n", __LINE__, _count_chunks(a));
        return false;
    }
    (*passed)++;
    
    return true;
}

#define test(y, x) do { bool r = false; \
    printf("RUN %s\n", (y)); \
    r = (x); \
    if (r) { printf(" -> \t\t\t\t\t\t\tOK\n"); } \
    else { retval = false; printf(" -> \t\t\t\t\t\t\tFAILED\n"); } } while(0)

int main(void) {
    bool retval = true;
    int test = 0;
    int passed = 0;
    int allocs = 0;
    struct balloc_arena *a = balloc_new(4096);
    test++;
    if (a) { 

        test("Test NULL/0", test_null_0_alloc(a, &test, &passed));
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
        test("Test bstrdup", test_bstrdup(a, &test, &passed));
       
        test("Test compact", test_compact(a, &test, &passed));

        if (retval) { passed++; }
        printf("Total test %d, passed %d\n", test, passed);
        balloc_destroy(a);
    } else {
        fprintf(stderr, "balloc_new failed\n");
    }

    return retval ? 0 : 1;
}
