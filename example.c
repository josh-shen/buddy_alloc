#include <stdio.h>
#include <string.h>

#include "buddy.h"

int main() {
    char memory[(1 << MEM_BLOCK_LOG2 ) + 152 + 8];

    buddy_t *alloc = buddy_init(memory, sizeof(memory));

    char *addr1 = buddy_malloc(alloc, 1048576/2);
    char *addr2 = buddy_malloc(alloc, 1048576/2);

    strncpy(addr1, "Hello", 32);
    strncpy(addr2, "World!", 32);

    printf("%s %s\n", addr1, addr2);

    buddy_free(alloc, addr1, 524288);
    buddy_free(alloc, addr2, 524288);

    return 0;
}
