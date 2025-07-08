//
// Created by Josh Shen on 7/5/25.
//
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "buddy.h"

/* Bit tree
 * Allows for fast and easy tracking of page allocation
 * Bit tree implemented as a flat array
 * The bit tree maps the entire memory pool, with every two bits representing a block
 * 00 - free, 01 - allocated, 10 - reserved
 *
 * If the max block size is smaller than the memory pool, some of the higher nodes in the tree will be unused
 * The height of a node in the tree corresponds with its order
 *
 * Total number of nodes in the tree = 2^(MEM_BLOCK + 1) - 1 for 2^MEM_BLOCK
 *
 * order = pow - MIN_BLOCK
 *
 * height = MEM_BLOCK - pow
 *        = MEM_BLOCK - order - MIN_BLOCK
 * offset = (address - base) / 2^(MIN_BLOCK + order)
 * index  = 2^height - 1 + offset
 *
 * MAX_BLOCK = 22 (2^22, 4 MB)
 * MIN_BLOCK = 12 (2^12, 4 KB)
 *
 * MAX_ORDER = MAX_BLOCK - MIN_BLOCK = 10
 */
#define TREE_NODES ((2 << (MAX_ORDER)) - 1)
#define WORDS (TREE_NODES / 16)
uint32_t bit_tree[WORDS];

/* Free lists
 * A linked list is maintained for pages of each order
 */
struct buddy_page {
    struct buddy_page *prev;
    struct buddy_page *next;
};
typedef struct buddy_page buddy_page_t;
buddy_page_t *free_lists[MAX_ORDER + 1];

/* HELPER FUNCTIONS */
uint8_t get_log2(uintptr_t length) {
    uint8_t log2 = 0;

    while (length > 1) {
        length >>= 1;
        log2++;
    }
    return log2;
}

uint8_t get_order(const uintptr_t length) {
    for (int n = MAX_BLOCK_LOG2; n >= MIN_BLOCK_LOG2; n--) {
        if ((unsigned)(1 << n) <= length) return n - MIN_BLOCK_LOG2;
    }
    return 0;
}

void mark_free() {}

void mark_reserved() {}

int split_partition(int index, int target) {
    while (index > target) {
        buddy_page_t *partition = free_lists[index];

        uint32_t address = (uint32_t)((uintptr_t)partition);
        uint32_t buddy_address = address + (2 << (index + 11)) / 2;

        // Remove the partition from the free list
        free_lists[index] = partition->next;
        if (free_lists[index] != NULL) {
            free_lists[index]->prev = NULL;
        }
        // TODO: update bit tree

        // Create buddy partition
        buddy_page_t *buddy_partition = (buddy_page_t *)(uintptr_t)(buddy_address);

        // TODO: Update bit tree for newly created buddy

        // Add both buddies to their new free list
        // Free list of the partition's order should always be null since splitting is required
        partition->prev = NULL;
        partition->next = buddy_partition;

        buddy_partition->prev = partition;
        buddy_partition->next = NULL;

        free_lists[index--] = partition;

        if (index == target) return index;
    }

    return -1;
}

void buddy_init(char *base, size_t length) {
    // Initialize free lists
    for (int i = 0; i <= MAX_ORDER; i++) {
        free_lists[i] = NULL;
    }

    /* mark reserved regions
     * if current block falls within a reserved region, mark it as reserved in the bitmap
     * do not add to free lists
     */
    uint8_t MEM_LOG2 = get_log2(length);
    uintptr_t floor = (uintptr_t)base;

    // Mark memory as free
    while (length > 0) {
        uint8_t order = get_order(length);
        int partition_size = 1 << (order + MIN_BLOCK_LOG2);

        buddy_page_t *p = (buddy_page_t *)base;

        // Bit tree
        uint8_t height = MEM_LOG2 - order - MIN_BLOCK_LOG2;
        uint32_t offset = ((uintptr_t)base - floor) / (1 << (MIN_BLOCK_LOG2 + order));
        uint32_t bit_tree_index = (1 << height) - 1 + offset;
        bit_tree[bit_tree_index] = 0;

        // Free list
        if (free_lists[order] == NULL) {
            p->prev = NULL;
            p->next = NULL;
            free_lists[order] = p;
        } else {
            p->prev = NULL;
            p->next = free_lists[order];

            free_lists[order]->prev = p;
            free_lists[order] = p;
        }

        base += partition_size;
        length -= partition_size;
    }
}

uintptr_t buddy_malloc(uintptr_t size) {
    if (size > 2 << (MAX_ORDER + 11)) {
        printf("Error: Requested size is too large: %lu bytes\n", size);
        return -1;
    }

    uint8_t order = get_order(size);

    if (free_lists[order] != NULL) {
        buddy_page_t *partition = free_lists[order];

        // Remove the partition from the free list
        free_lists[order] = partition->next;
        if (free_lists[order] != NULL) {
            free_lists[order]->prev = NULL;
        }

        // TODO: Mark partition used in bit tree. Base and size of the memory pool needs to be globally accessible

        return (uint32_t)((uintptr_t)partition);
    } else {
        // Search for a larger partition to split
        for (int i = order + 1; i <= MAX_ORDER; i++) {
            if (free_lists[i] != NULL) {
                // A larger partition found, split
                int index = split_partition(i, order);

                if (index == -1) {
                    printf("Error: Failed to split partition\n");
                    return -1;
                }

                buddy_page_t *partition = free_lists[index];

                uint32_t address = (uint32_t)(uintptr_t)partition;

                // Remove the partition from the free list
                free_lists[index] = partition->next;
                if (free_lists[order] != NULL) {
                    free_lists[order]->prev = NULL;
                }

                // TODO: Mark partition used in bit tree

                return address;
            }
        }

        return -1;
    }
}

//void buddy_free(uintptr_t address) {}