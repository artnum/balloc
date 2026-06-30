#include "../src/include/balloc.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    /* by default, this will be mmap.
     * You can set, at compile time, BALLOC_MMAP_TRIGGER_SIZE to change this
     * behavior
     */
    struct balloc_arena *arena = balloc_new(2 * 1024 * 1024);
    if(!arena) {
        /* fail */
        return 1;
    }

    char * name = bstrdup(arena, "Etienne");
    if (!name) {
        /* fail */
    }
    char * password = bstrdup_sec(arena, "1234abcd");
    if (!password) {
        /* fail */
    }
    printf("%s %s\n", name, password);
    balloc_reset(arena);

    struct balloc_chunk *freelist = arena->free;
    while (freelist) {
        uint8_t *content = freelist->content;
        for(int i = 0; i < 128; i++) {
            if (content[i] < ' ' || content[i] > 126) {
                printf(". ");
            } else {
                printf("%c ", content[i]);
            }
        }
        printf("\n");
        freelist = freelist->next;
    }

    balloc_destroy(arena);


    return 0;
}
