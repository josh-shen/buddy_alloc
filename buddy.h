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
#define TOTAL_TREE_NODES ((1 << (MEM_BLOCK_LOG2 - MIN_BLOCK_LOG2)) - 1)
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

buddy_t *buddy_init(char *, size_t);

void *buddy_malloc(buddy_t *, size_t);

void buddy_free(buddy_t *, void *, size_t);

#endif //BUDDY_H
