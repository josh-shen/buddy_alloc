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
 *        = MEM_BLOCK - (order + MIN_BLOCK)
 * offset = (address - base) / 2^(MIN_BLOCK + order)
 * index  = 2^height - 1 + offset
 *
 * MAX_BLOCK = 22 (2^22, 4 MB)
 * MIN_BLOCK = 12 (2^10, 4 KB)
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
uint8_t get_order(uintptr_t length) {
    for (int n = MAX_ORDER; n >= 0; n--) {
        if ((unsigned)(2 << (n + 11)) <= length) return n;
    }
    return 0;
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

    // Mark memory as free
    while (length > 0) {
        uint8_t order = get_order(length);
        int partition_size = 2 << (order + 11);

        buddy_page_t *p = (buddy_page_t *)base;

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

//uintptr_t buddy_malloc(uintptr_t length) {}

//void buddy_free(uintptr_t address) {}