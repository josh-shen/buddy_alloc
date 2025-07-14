//
// Created by Josh Shen on 7/5/25.
//
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "buddy.h"

#include <stdlib.h>

/* Bit tree
 * Allows for fast and easy tracking of page allocation
 * Bit tree represented in a flat array using a breadth first traversal
 * The bit tree maps the entire memory pool, with every two bits representing a block
 * 00 - free (free for allocation, not split, available for merging)
 * 01 - split (not allocated, but has been split into two buddies and should not be allocated or merged)
 * 10 - allocated
 * 11 - reserved (never merge)
 *
 * If the max block size is smaller than the memory pool, some of the higher nodes in the tree will be unused
 * Since the tree is indexed in level order, the array can be truncated to skip these nodes
 * Remove all nodes from MEM_BLOCK_log2 through MAX_BLOCK_SIZE_log2
 * First N nodes to remove (N) = 2^(MEM_BLOCK_LOG2 - MAX_BLOCK_SIZE_LOG2) - 1
 *
 * Total number of nodes in the tree for a 2^MEM_BLOCK_LOG2 memory pool = 2^(MEM_BLOCK_LOG2 - MIN_BLOCK_LOG2) - 1  - N
 *
 * order = pow - MIN_BLOCK
 *
 * The height of a node in the tree corresponds with its order
 *
 * height = MEM_BLOCK - pow
 *        = MEM_BLOCK - order - MIN_BLOCK
 * offset = (address - base) / 2^(MIN_BLOCK + order)
 * index  = 2^height - 1 + offset
 *
 * MAX_ORDER = MAX_BLOCK - MIN_BLOCK
 */

/* HELPER FUNCTIONS */
uint8_t get_log2(size_t length) {
    uint8_t log2 = 0;

    while (length > 1) {
        length >>= 1;
        log2++;
    }
    return log2;
}

uint8_t get_order(const size_t length) {
    for (int n = MAX_BLOCK_LOG2; n >= MIN_BLOCK_LOG2; n--) {
        if ((unsigned)(1 << n) <= length) return n - MIN_BLOCK_LOG2;
    }
    return 0;
}

uint32_t get_bit_tree_index(buddy_t *alloc, uintptr_t address, uint8_t order) {
    uint8_t height = MEM_BLOCK_LOG2 - order - MIN_BLOCK_LOG2;
    uint32_t offset = (address - alloc->base) / (1 << (MIN_BLOCK_LOG2 + order));
    uint32_t node_index = (1 << height) - 1 + offset - TRUNCATED_TREE_NODES;

    // Blocks are represented by 2 bits
    return node_index * 2;
}

void mark_free(buddy_t *alloc, uintptr_t address, uint8_t order) {
    uint32_t bit_index = get_bit_tree_index(alloc, address, order);

    // Find which word and offset within the word the index falls in
    uint32_t word_index = bit_index / 16;
    uint32_t word_offset = bit_index % 16;

    // Set bits to 00
    alloc->bit_tree[word_index] &= 0 << word_offset;
    alloc->bit_tree[word_index] &= 0 << word_offset + 1;
}

void mark_split(buddy_t *alloc, uintptr_t address, uint8_t order) {
    uint32_t bit_index = get_bit_tree_index(alloc, address, order);

    // Find which word and offset within the word the index falls in
    uint32_t word_index = bit_index / 16;
    uint32_t word_offset = bit_index % 16;

    // Set bits to 01
    alloc->bit_tree[word_index] &= 0 << word_offset + 1;
    alloc->bit_tree[word_index] |= 1 << word_offset;
}

void mark_allocated(buddy_t *alloc, uintptr_t address, uint8_t order) {
    uint32_t bit_index = get_bit_tree_index(alloc, address, order);

    // Find which word and offset within the word the index falls in
    uint32_t word_index = bit_index / 16;
    uint32_t word_offset = bit_index % 16;

    // Set bits to 10
    alloc->bit_tree[word_index] &= 0 << word_offset;
    alloc->bit_tree[word_index] |= 1 << word_offset + 1;
}

void mark_reserved(buddy_t *alloc, uintptr_t address, uint8_t order) {
    uint32_t bit_index = get_bit_tree_index(alloc, address, order);

    // Find which word and offset within the word the index falls in
    uint32_t word_index = bit_index / 16;
    uint32_t word_offset = bit_index % 16;

    // Set bits to 11
    alloc->bit_tree[word_index] |= 3 << word_offset;
}

uint8_t split_partition(buddy_t *alloc, int index, int target) {
    while (index > target) {
        buddy_page_t *partition = alloc->free_lists[index];

        uintptr_t address = (uintptr_t)partition;
        uintptr_t buddy_address = (address - alloc->base ^ 1 << (index - 1 + MIN_BLOCK_LOG2)) + alloc->base;

        // Remove the partition from the free list
        alloc->free_lists[index] = partition->next;
        if (alloc->free_lists[index] != NULL) {
            alloc->free_lists[index]->prev = NULL;
        }

        // Update parent block in bit tree as split
        mark_split(alloc, address, index);

        // Create buddy partition
        buddy_page_t *buddy_partition = (buddy_page_t *)buddy_address;

        // Decrement index to the next order
        index--;

        // Mark newly created buddies as free in bit tree
        mark_free(alloc, address, index);
        mark_free(alloc, buddy_address, index);

        // Add both buddies to their new free list
        // Free list of the partition's order should always be null since splitting was required
        partition->prev = NULL;
        partition->next = buddy_partition;

        buddy_partition->prev = partition;
        buddy_partition->next = NULL;

        alloc->free_lists[index] = partition;

        // Continue splitting if necessary
        if (index == target) return index;
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

        if (length < sizeof(buddy_t))
            return NULL;
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

    /* Marking memory
     * Bitmap is initialized as all 0 (free, not split) initially
     * Bitmap does not need to be updated when marking free blocks, only add to free list
     */
    while (length >= 1 << MIN_BLOCK_LOG2) {
        uint8_t order = get_order(length);
        size_t partition_size = 1 << (order + MIN_BLOCK_LOG2);

        buddy_page_t *p = (buddy_page_t *)address;

        // TODO: mark split nodes in bit tree

        // Free list
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

    if (alloc->free_lists[order] != NULL) {
        buddy_page_t *partition = alloc->free_lists[order];

        uintptr_t address = (uintptr_t)partition;

        // Remove the partition from the free list
        alloc->free_lists[order] = partition->next;
        if (alloc->free_lists[order] != NULL) {
            alloc->free_lists[order]->prev = NULL;
        }

        // Mark partition used in bit tree
        mark_split(alloc, address, order);

        return (char *)address;
    }
    // A best fit block was not available - search for a larger partition to split
    for (int i = order + 1; i <= MAX_ORDER; i++) {
        if (alloc->free_lists[i] != NULL) {
            // A larger partition found, split
            uint8_t index = split_partition(alloc, i, order);

            if (index == MAX_ORDER + 1) {
                printf("Error: Failed to split partition\n");
                return NULL;
            }

            buddy_page_t *partition = alloc->free_lists[index];

            uintptr_t address = (uintptr_t)partition;

            // Remove the partition from the free list
            alloc->free_lists[index] = partition->next;
            if (alloc->free_lists[order] != NULL) {
                alloc->free_lists[order]->prev = NULL;
            }

            // Mark partition used in bit tree
            mark_allocated(alloc, address, order);

            return (char *)address;
        }
    }
    printf("Error: No memory available\n");
    return NULL;
}

void buddy_free(buddy_t *alloc, void *addr, size_t length) {
    uintptr_t address = (uintptr_t)addr;
    uint8_t order = get_order(length);

    uintptr_t buddy_address = (address - alloc->base ^ 1 << (order + MIN_BLOCK_LOG2)) + alloc->base;
    uint32_t buddy_index = get_bit_tree_index(alloc, buddy_address, order);
    uint32_t buddy_word_index = buddy_index / 16;
    uint32_t buddy_word_offset = buddy_index % 16;

    uint8_t buddy_state = alloc->bit_tree[buddy_word_index] >> buddy_word_offset & 3;

    if (buddy_state == 1 || buddy_state == 2 || buddy_state == 3) {
        // Buddy is either split, allocated, or reserved. Immediately return the memory to free lists
        buddy_page_t *p = (buddy_page_t *)address;
        p->prev = NULL;
        p->next = alloc->free_lists[order];

        if (alloc->free_lists[order]) alloc->free_lists[order]->prev = p;
        alloc->free_lists[order] = p;

        // Update the bit tree
        mark_free(alloc, address, order);
        return;
    }
    if (order < MAX_ORDER) {
        // Buddy is free. Merge
        while (order < MAX_ORDER) {
            // Mark base buddy as free in the bit tree
            mark_free(alloc, address, order);

            // Remove buddy from its free list
            buddy_page_t *buddy_p = (buddy_page_t *)buddy_address;
            buddy_page_t *buddy_p_prev = buddy_p->prev;
            buddy_page_t *buddy_p_next = buddy_p->next;

            if (buddy_p_prev != NULL) buddy_p_prev->next = buddy_p_next;
            if (buddy_p_next != NULL) buddy_p_next->prev = buddy_p_prev;

            // If this buddy was the head of the list
            if (alloc->free_lists[order] == buddy_p) alloc->free_lists[order] = buddy_p_next;

            buddy_p->prev = NULL;
            buddy_p->next = NULL;

            // Get state of buddy of the next order
            order++;
            buddy_address = (address - alloc->base ^ 1 << (order + MIN_BLOCK_LOG2)) + alloc->base;
            buddy_index = get_bit_tree_index(alloc, buddy_address, order);
            buddy_word_index = buddy_index / 16;
            buddy_word_offset = buddy_index % 16;

            buddy_state = alloc->bit_tree[buddy_word_index] >> buddy_word_offset & 3;

            if (buddy_state != 0) break;
        }

        // Mark bit tree as free (after merging, the nodes state should have been 10 (split))
        mark_free(alloc, address, order);

        // Return to free lists
        buddy_page_t *p = (buddy_page_t *)address;

        p->prev = NULL;
        p->next = alloc->free_lists[order];

        if (alloc->free_lists[order]) alloc->free_lists[order]->prev = p;
        alloc->free_lists[order] = p;
    }
}