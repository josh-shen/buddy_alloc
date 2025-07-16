#include <stdio.h>
#include <string.h>

#include "buddy.h"

int main() {
    char memory[1 << MEM_BLOCK_LOG2];

    buddy_t *alloc = buddy_init(memory, sizeof(memory));
    if (!alloc) return -1;

    char *addr1 = buddy_malloc(alloc, 32);
    char *addr2 = buddy_malloc(alloc, 32);

    strncpy(addr1, "Hello", 32);
    strncpy(addr2, "World!", 32);

    printf("%s %s\n", addr1, addr2);

    buddy_free(alloc, addr1, 32);
    buddy_free(alloc, addr2, 32);

    buddy_free(alloc, addr1, 32);

    return 0;
}
