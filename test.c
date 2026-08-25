#include "src/include/balloc.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

bool test_allocation(struct balloc_arena *a, int *test, int *passed, 
                     int *allocs) {
    (*test)++;
    void *ptr = balloc(a, 1000);
    if (ptr == NULL) {
        fprintf(stderr, "Allocation failed\n");
        return false;
    }
    (*passed)++;

    for (size_t i = 0; i < 100; i++) {
        (*test)++;
        void *ptr = balloc(a, 1000);
        if (ptr == NULL) {
            fprintf(stderr, "Allocation in loop (%ld) failed\n", i);
            return false;
        }
        (*passed)++;
    }
    if (allocs) {
        for (struct balloc_chunk *c = a->head; c; c = c->next) {
            (*allocs)++;
        }
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

int main(void) {
    int test = 0;
    int passed = 0;
    int allocs = 0;
    struct balloc_arena *a = balloc_new(4096);

    assert(test_allocation(a, &test, &passed, &allocs) == true);
    assert(test_reset_succeed(a, &test, &passed) == true);
    assert(test_allocation_after_reset(a, &test, &passed, allocs) == true);
    assert(test_allocation_with_big(a, &test, &passed) == true);

    printf("Total test %d, passed %d\n", test, passed);
    balloc_destroy(a);
}
