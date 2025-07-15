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

    // Blocks are represented by 2 bits
    return node_index * 2;
}

static uint8_t get_state(buddy_t *alloc, uintptr_t buddy_address, uint8_t order) {
    uint32_t buddy_index = get_bit_tree_index(alloc, buddy_address, order);
    uint32_t buddy_word_index = buddy_index / 16;
    uint32_t buddy_word_offset = buddy_index % 16;

    uint8_t buddy_state = alloc->bit_tree[buddy_word_index] >> buddy_word_offset;

    // Apply mask 11 to get the desired bits
    return buddy_state & 3;
}

static void mark_free(buddy_t *alloc, uintptr_t address, uint8_t order) {
    uint32_t bit_index = get_bit_tree_index(alloc, address, order);

    // Find which word and offset within the word the index falls in
    uint32_t word_index = bit_index / 16;
    uint32_t word_offset = bit_index % 16;

    // Set bits to 00
    alloc->bit_tree[word_index] &= 0 << word_offset;
    alloc->bit_tree[word_index] &= 0 << word_offset + 1;
}

static void mark_split(buddy_t *alloc, uintptr_t address, uint8_t order) {
    uint32_t bit_index = get_bit_tree_index(alloc, address, order);

    // Find which word and offset within the word the index falls in
    uint32_t word_index = bit_index / 16;
    uint32_t word_offset = bit_index % 16;

    // Set bits to 01
    alloc->bit_tree[word_index] &= 0 << word_offset + 1;
    alloc->bit_tree[word_index] |= 1 << word_offset;
}

static void mark_allocated(buddy_t *alloc, uintptr_t address, uint8_t order) {
    uint32_t bit_index = get_bit_tree_index(alloc, address, order);

    // Find which word and offset within the word the index falls in
    uint32_t word_index = bit_index / 16;
    uint32_t word_offset = bit_index % 16;

    // Set bits to 10
    alloc->bit_tree[word_index] &= 0 << word_offset;
    alloc->bit_tree[word_index] |= 1 << word_offset + 1;
}

static void mark_reserved(buddy_t *alloc, uintptr_t address, uint8_t order) {
    uint32_t bit_index = get_bit_tree_index(alloc, address, order);

    // Find which word and offset within the word the index falls in
    uint32_t word_index = bit_index / 16;
    uint32_t word_offset = bit_index % 16;

    // Set bits to 11
    alloc->bit_tree[word_index] |= 3 << word_offset;
}

static void append(buddy_t *alloc, uintptr_t address, uint8_t order) {
    buddy_page_t *p = (buddy_page_t *)address;

    if (alloc->free_lists[order] == NULL) {
        p->prev = NULL;
        p->next = NULL;
        alloc->free_lists[order] = p;
    } else {
        p->prev = NULL;
        p->next = alloc->free_lists[order];

        alloc->free_lists[order]->prev = p;
        alloc->free_lists[order] = p;
    }
}

static void free_list_remove(buddy_t *alloc, uintptr_t address, uint8_t order) {
    buddy_page_t *p = (buddy_page_t *)address;
    buddy_page_t *buddy_p_prev = p->prev;
    buddy_page_t *buddy_p_next = p->next;

    if (buddy_p_prev != NULL) buddy_p_prev->next = buddy_p_next;
    if (buddy_p_next != NULL) buddy_p_next->prev = buddy_p_prev;

    // If this buddy was the head of the list
    if (alloc->free_lists[order] == p) alloc->free_lists[order] = buddy_p_next;

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
        mark_split(alloc, address, order);

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
    {
        size_t pad = -(uintptr_t) base & _Alignof(buddy_t) - 1;
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

    // Marking memory - add free memory to free lists and mark split blocks
    while (length >= 1 << MIN_BLOCK_LOG2) {
        uint8_t order = get_order(length);
        size_t partition_size = 1 << (order + MIN_BLOCK_LOG2);

        // TODO: mark split nodes in bit tree

        // Add to free list
        append(alloc, address, order);

        address += partition_size;
        length -= partition_size;
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
        mark_split(alloc, address, order);

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
            mark_allocated(alloc, address, next_order);

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
        mark_free(alloc, address, order);
        return;
    }

    // Buddy is free. Merge
    while (order < MAX_ORDER) {
        // Mark first buddy as free in the bit tree
        mark_free(alloc, address, order);

        // Remove buddy from its free list
        free_list_remove(alloc, buddy_address, order);

        order++;

        // Mark the parent block as free now
        mark_free(alloc, address, order);

        // Get state of buddy of the next order
        buddy_state = get_state(alloc, buddy_address, order);

        if (buddy_state != 0) break;
    }

    // Add the final, merged block back to free lists
    append(alloc, address, order);
}