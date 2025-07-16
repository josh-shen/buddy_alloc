#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "buddy.h"

#include <stdlib.h>

/* HELPER FUNCTIONS */
static uint8_t get_order(const size_t length) {
    for (int n = MAX_BLOCK_LOG2; n >= MIN_BLOCK_LOG2; n--) {
        if ((unsigned)(1 << n) <= length) return n - MIN_BLOCK_LOG2;
    }
    return 0;
}

static uint32_t get_bit_tree_index(buddy_t *alloc, uintptr_t address, uint8_t order) {
    uint8_t height = MEM_BLOCK_LOG2 - order - MIN_BLOCK_LOG2;
    uint32_t offset = (address - alloc->base) / (1 << (MIN_BLOCK_LOG2 + order));
    uint32_t node_index = (1 << height) - 1 + offset - TRUNCATED_TREE_NODES;

    return node_index;
}

static uint8_t get_state(buddy_t *alloc, uintptr_t address, uint8_t order) {
    uint32_t index = get_bit_tree_index(alloc, address, order);
    uint32_t word_index = index / 16;
    uint32_t word_offset = index * 2 % 32;

    uint8_t state = alloc->bit_tree[word_index] >> word_offset;

    // Apply mask 11 to get the desired bits
    return state & 3;
}

static void set_state(buddy_t *alloc, uintptr_t address, uint8_t order, uint8_t state) {
    uint32_t index = get_bit_tree_index(alloc, address, order);
    uint32_t word_index = index / 16;
    uint32_t word_offset = index * 2 % 32;

    // Create a mask to only set two bits at the target position
    uint32_t mask = ~(3 << word_offset);

    alloc->bit_tree[word_index] = alloc->bit_tree[word_index] & mask | state << word_offset;
}

static void append(buddy_t *alloc, uintptr_t address, uint8_t order) {
    buddy_page_t *p = (buddy_page_t *)address;

    if (alloc->free_lists[order]) alloc->free_lists[order]->prev = p;

    p->prev = NULL;
    p->next = alloc->free_lists[order];

    alloc->free_lists[order] = p;
}

static void free_list_remove(buddy_t *alloc, uintptr_t address, uint8_t order) {
    buddy_page_t *p = (buddy_page_t *)address;

    if (p->prev != NULL) p->prev->next = p->next;

    if (alloc->free_lists[order] == p) alloc->free_lists[order] = p->next;

    if (p->next != NULL) p->next->prev = p->prev;

    p->prev = NULL;
    p->next = NULL;
}

static uint8_t split_partition(buddy_t *alloc, int order, int target) {
    while (order > target) {
        buddy_page_t *partition = alloc->free_lists[order];

        uintptr_t address = (uintptr_t)partition;
        uintptr_t buddy_address = (address - alloc->base ^ 1 << (order - 1 + MIN_BLOCK_LOG2)) + alloc->base;

        // Remove the partition from the free list
        free_list_remove(alloc, address, order);

        // Update parent block in bit tree as split
        set_state(alloc, address, order, 1);

        // Decrement index to the next order
        order--;

        // Add both buddies to their new free list
        // No need to mark free
        append(alloc, address, order);
        append(alloc, buddy_address, order);

        // Continue splitting if necessary
        if (order == target) return order;
    }
    return MAX_ORDER + 1;
}

buddy_t *buddy_init(char *base, size_t length) {
    buddy_t *alloc;
    size_t pad;
    {
        pad = -(uintptr_t) base & _Alignof(buddy_t) - 1;
        if (length < pad) return NULL;
        base += pad;
        length -= pad;

        if (length < sizeof(buddy_t)) return NULL;
        alloc = (buddy_t *) base;
        base += sizeof(buddy_t);
        length -= sizeof(buddy_t);
    }

    alloc->base = (uintptr_t)base;
    alloc->size = length;

    // Initialize bit tree - all bits initially set to 0 (free, not split)
    for (int i = 0; i < TREE_WORDS; i++) {
        alloc->bit_tree[i] = 0;
    }

    // Initialize free lists
    for (int i = 0; i <= MAX_ORDER; i++) {
        alloc->free_lists[i] = NULL;
    }

    uintptr_t address = alloc->base;

    // Add free memory to free lists
    while (length >= 1 << MIN_BLOCK_LOG2) {
        uint8_t order = get_order(length);
        size_t partition_size = 1 << (order + MIN_BLOCK_LOG2);

        // Add to free list
        append(alloc, address, order);

        address += partition_size;
        length -= partition_size;
    }

    // Mark memory reserved by buddy struct
    size_t used = pad + sizeof(buddy_t);
    while (used >= 1 << MIN_BLOCK_LOG2) {
        uint8_t order = get_order(used);
        size_t partition_size = 1 << (order + MIN_BLOCK_LOG2);

        // Mark reserved in bit tree
        set_state(alloc, address, order, 3);

        address += partition_size;
        used -= partition_size;
    }
    return alloc;
}

void *buddy_malloc(buddy_t *alloc, size_t length) {
    if (length > 1 << MAX_BLOCK_LOG2) {
        printf("Error: Requested size is too large\n");
        return NULL;
    }

    uint8_t order = get_order(length);

    // Block of the requested order is available - no need to split
    if (alloc->free_lists[order] != NULL) {
        buddy_page_t *partition = alloc->free_lists[order];

        uintptr_t address = (uintptr_t)partition;

        // Remove the partition from the free list
        free_list_remove(alloc, address, order);

        // Mark partition used in bit tree
        set_state(alloc, address, order, 2);

        return (char *)address;
    }

    // A best fit block was not available - search for a larger block to split
    for (int i = order + 1; i <= MAX_ORDER; i++) {
        if (alloc->free_lists[i] != NULL) {
            // A larger partition found, split
            uint8_t next_order = split_partition(alloc, i, order);

            if (next_order == MAX_ORDER + 1) {
                printf("Error: Failed to split partition\n");
                return NULL;
            }

            buddy_page_t *partition = alloc->free_lists[next_order];

            uintptr_t address = (uintptr_t)partition;

            // Remove the partition from the free list
            free_list_remove(alloc, address, next_order);

            // Mark partition used in bit tree
            set_state(alloc, address, next_order, 2);

            return (char *)address;
        }
    }
    printf("Error: No memory available\n");
    return NULL;
}

void buddy_free(buddy_t *alloc, void *addr, size_t length) {
    uint8_t order = get_order(length);

    uintptr_t address = (uintptr_t)addr;
    uintptr_t buddy_address = (address - alloc->base ^ 1 << (order + MIN_BLOCK_LOG2)) + alloc->base;

    uint8_t buddy_state = get_state(alloc, buddy_address, order);

    // Buddy is either split, allocated, or reserved. Immediately return the block to free lists
    if (buddy_state == 1 || buddy_state == 2 || buddy_state == 3) {
        // Add block back to free lists
        append(alloc, address, order);

        // Update the bit tree
        set_state(alloc, address, order, 0);
        return;
    }

    // Buddy is free. Merge
    while (order < MAX_ORDER) {
        // Mark first buddy as free in the bit tree
        set_state(alloc, address, order, 0);

        // Remove buddy from its free list
        free_list_remove(alloc, buddy_address, order);

        order++;

        // Mark the parent block as free now
        set_state(alloc, address, order, 0);

        // Get state of buddy of the next order
        buddy_address = (address - alloc->base ^ 1 << (order + MIN_BLOCK_LOG2)) + alloc->base;
        buddy_state = get_state(alloc, buddy_address, order);

        if (buddy_state != 0) break;
    }

    // Add the final, merged block back to free lists
    append(alloc, address, order);
}