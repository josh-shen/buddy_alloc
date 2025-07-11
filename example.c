//
// Created by Josh Shen on 7/5/25.
//
#include <stdio.h>
#include <string.h>

#include "buddy.h"

int main() {
    char memory[1 << MEM_BLOCK_LOG2];

    buddy_t *alloc = buddy_init(memory, sizeof(memory));

    char *addr = buddy_malloc(alloc, 262144);

    printf("%p\n", addr);

    for (int i = 0; i <= MAX_ORDER; i++) {
        printf("%d: %p\n", i, alloc->free_lists[i]);
    }

    strncpy(addr, "Hello", 32);

    printf("%s\n", addr);

    // buddy_free()

    return 0;
}
