//
// Created by Josh Shen on 7/5/25.
//

#ifndef BUDDY_H
#define BUDDY_H

#include <stdint.h>
#include <stddef.h>

/*
 * MAX_BLOCK = 22 (2^22, 4 MB)
 * MIN_BLOCK = 12 (2^10, 4 KB)
 *
 * MAX_ORDER = MAX_BLOCK - MIN_BLOCK = 10
 * order = pow - MIN_BLOCK
 */
#define MEM_BLOCK_LOG2 20
#define MAX_BLOCK_LOG2 20
#define MIN_BLOCK_LOG2 12
#define MAX_ORDER (MAX_BLOCK_LOG2 - MIN_BLOCK_LOG2)

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

buddy_t *buddy_init(const char *, size_t);

void *buddy_malloc(buddy_t *, size_t);

void buddy_free(buddy_t *, uintptr_t);

#endif //BUDDY_H
