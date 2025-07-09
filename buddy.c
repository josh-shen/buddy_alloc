//
// Created by Josh Shen on 7/5/25.
//
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "buddy.h"

/* Bit tree
 * Allows for fast and easy tracking of page allocation
 * Bit tree represented in a flat array using a breadth first traversal
 * The bit tree maps the entire memory pool, with every two bits representing a block
 * 00 - free, 01 - allocated, 10 - reserved
 *
 * If the max block size is smaller than the memory pool, some of the higher nodes in the tree will be unused
 * Since the tree is indexed in level order, the array can be truncated to skip these nodes
 * Remove all nodes from MEM_BLOCK_log2 through MAX_BLOCK_SIZE_log2
 * First N nodes to remove (N) = 2^(MEM_BLOCK_LOG2 - MAX_BLOCK_SIZE_LOG2) - 1
 *
 * Total number of nodes in the tree for a 2^MEM_BLOCK_LOG2 memory pool = 2^MEM_BLOCK - 1  - N
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
#define TOTAL_TREE_NODES ((1 << MEM_BLOCK_LOG2) - 1)
#define TRUNCATED_TREE_NODES ((1 << (MEM_BLOCK_LOG2 - MAX_BLOCK_LOG2)) - 1)
#define TREE_NODES (TOTAL_TREE_NODES - TRUNCATED_TREE_NODES)
#define TREE_WORDS (TREE_NODES / 16)

struct buddy_page {
    struct buddy_page *prev;
    struct buddy_page *next;
};
typedef struct buddy_page buddy_page_t;

struct buddy {
    uintptr_t base;
    size_t size;
    uint32_t bit_tree[TREE_WORDS];
    buddy_page_t *free_lists[MAX_ORDER + 1];
};
typedef struct buddy buddy_t;

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

void mark_bit_tree(buddy_t alloc, uintptr_t address, uint8_t order, uint8_t state) {
    uint8_t height = MEM_BLOCK_LOG2 - order - MIN_BLOCK_LOG2;
    uint32_t offset = (address - alloc.base) / (1 << (MIN_BLOCK_LOG2 + order));
    uint32_t bit_tree_index = (1 << height) - 1 + offset - TRUNCATED_TREE_NODES;
    alloc.bit_tree[bit_tree_index] = state;
}

int split_partition(buddy_t alloc, int index, int target) {
    while (index > target) {
        buddy_page_t *partition = alloc.free_lists[index];

        uintptr_t address = (uintptr_t)partition;
        uintptr_t buddy_address = address + (1 << (index + MIN_BLOCK_LOG2)) / 2;

        // Remove the partition from the free list
        alloc.free_lists[index] = partition->next;
        if (alloc.free_lists[index] != NULL) {
            alloc.free_lists[index]->prev = NULL;
        }
        // Update block in bit tree as split
        mark_bit_tree(alloc, address, index, 3);

        // Create buddy partition
        buddy_page_t *buddy_partition = (buddy_page_t *)(uintptr_t)(buddy_address);

        // Mark newly created buddy as free in bit tree
        mark_bit_tree(alloc, address, index, 0);

        // Add both buddies to their new free list
        // Free list of the partition's order should always be null since splitting is required
        partition->prev = NULL;
        partition->next = buddy_partition;

        buddy_partition->prev = partition;
        buddy_partition->next = NULL;

        alloc.free_lists[index--] = partition;

        if (index == target) return index;
    }

    return -1;
}

void buddy_init(const char *base, size_t length) {
    buddy_t alloc;
    alloc.base = (uintptr_t)base;
    alloc.size = length;

    // Initialize free lists
    for (int i = 0; i <= MAX_ORDER; i++) {
        alloc.free_lists[i] = NULL;
    }

    /* TODO: mark reserved regions
     * if current block falls within a reserved region, mark it as reserved in the bitmap
     * do not add to free lists
     */
    uintptr_t address = alloc.base;

    // Mark memory as free
    while (length > 0) {
        uint8_t order = get_order(length);
        int partition_size = 1 << (order + MIN_BLOCK_LOG2);

        buddy_page_t *p = (buddy_page_t *)address;

        // Mark free in bit tree
        mark_bit_tree(alloc, address, order, 0);

        // Free list
        if (alloc.free_lists[order] == NULL) {
            p->prev = NULL;
            p->next = NULL;
            alloc.free_lists[order] = p;
        } else {
            p->prev = NULL;
            p->next = alloc.free_lists[order];

            alloc.free_lists[order]->prev = p;
            alloc.free_lists[order] = p;
        }

        address += partition_size;
        length -= partition_size;
    }
}

uintptr_t buddy_malloc(buddy_t alloc, uintptr_t size) {
    if (size > 1 << MAX_ORDER) {
        printf("Error: Requested size is too large: %llu bytes\n", size);
        return -1;
    }

    uint8_t order = get_order(size);

    if (alloc.free_lists[order] != NULL) {
        buddy_page_t *partition = alloc.free_lists[order];

        uintptr_t address = (uintptr_t)partition;

        // Remove the partition from the free list
        alloc.free_lists[order] = partition->next;
        if (alloc.free_lists[order] != NULL) {
            alloc.free_lists[order]->prev = NULL;
        }

        // Mark partition used in bit tree
        mark_bit_tree(alloc, address, order, 1);

        return address;
    } else {
        // Search for a larger partition to split
        for (int i = order + 1; i <= MAX_ORDER; i++) {
            if (alloc.free_lists[i] != NULL) {
                // A larger partition found, split
                int index = split_partition(alloc, i, order);

                if (index == -1) {
                    printf("Error: Failed to split partition\n");
                    return -1;
                }

                buddy_page_t *partition = alloc.free_lists[index];

                uintptr_t address = (uintptr_t)partition;

                // Remove the partition from the free list
                alloc.free_lists[index] = partition->next;
                if (alloc.free_lists[order] != NULL) {
                    alloc.free_lists[order]->prev = NULL;
                }

                // Mark partition used in bit tree
                mark_bit_tree(alloc, address, order, 1);

                return address;
            }
        }

        return -1;
    }
}

//void buddy_free(uintptr_t address) {}