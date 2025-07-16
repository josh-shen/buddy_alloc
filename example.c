#include <stdio.h>
#include <string.h>

#include "buddy.h"

void print_free_lists(buddy_t *alloc) {
    for (int i = 0; i <= MAX_ORDER; i++) {
        printf("%d: %p", i, alloc->free_lists[i]);

        buddy_page_t *list = alloc->free_lists[i];
        int count = 0;
        while (list != NULL) {
            list = list->next;
            count++;
        }
        printf(" %d nodes\n", count);
    }
}
void print_bit_tree(buddy_t *alloc) {
    for (int i = 0; i < TREE_WORDS; i++) {
        printf("%u ", alloc->bit_tree[i]);
    }
    printf("\n");
}
void get_bit_tree_index(buddy_t *alloc, uintptr_t address, uint8_t order) {
    uint8_t height = MEM_BLOCK_LOG2 - order - MIN_BLOCK_LOG2;
    uint32_t offset = (address - alloc->base) / (1 << (MIN_BLOCK_LOG2 + order));
    uint32_t node_index = (1 << height) - 1 + offset - TRUNCATED_TREE_NODES;

    // Blocks are represented by 2 bits
    printf("%u: height: %u offset: %u index: %u\n",order, height, offset, node_index);
}
void get_state(buddy_t *alloc, uintptr_t address, uint8_t order) {
    uint8_t height = MEM_BLOCK_LOG2 - order - MIN_BLOCK_LOG2;
    uint32_t offset = (address - alloc->base) / (1 << (MIN_BLOCK_LOG2 + order));
    uint32_t node_index = (1 << height) - 1 + offset - TRUNCATED_TREE_NODES;

    uint32_t index = node_index * 2;
    uint32_t word_index = index / 16;
    uint32_t word_offset = index % 16;

    uint8_t state = alloc->bit_tree[word_index] >> word_offset;

    // Apply mask 11 to get the desired bits
    printf("%u\n", state & 3);
}

int main() {
    char memory[1 << MEM_BLOCK_LOG2];

    buddy_t *alloc = buddy_init(memory, sizeof(memory));

    if (!alloc) {
        printf("something went wrong\n");
        return -1;
    }

    char *addr1 = buddy_malloc(alloc, 32);
    char *addr2 = buddy_malloc(alloc, 32);

    strncpy(addr1, "Hello", 32);
    strncpy(addr2, "World!", 32);

    printf("%s %s\n", addr1, addr2);

    buddy_free(alloc, addr1, 32);
    buddy_free(alloc, addr2, 32);

    return 0;
}
