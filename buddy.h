#ifndef BUDDY_H
#define BUDDY_H

#include <stdint.h>
#include <stddef.h>

//#define LOGGING
#define ERR_LOGGING

/* ================================= BUDDIES ==================================
 * The buddy allocator assumes that the memory pool it manages is a power of 2.
 * This allows memory of 2^n to be split into two "buddy" blocks of 2^(n-1). An
 * allocation request will search for a best-fit power of 2 block that will
 * satisfy the request, splitting larger blocks if necessary. When deallocating
 * memory, blocks are merged with its buddy as long as the buddy itself is free.
 * To find the correct buddy to merge, the buddy block's address can be
 * calculated using XOR:
 *
 * buddy address = (address - base) XOR 2^(order + MIN_BLOCK_LOG2)
 *
 * ================================ FREE LISTS ================================
 * Allocated block sizes are always powers of 2. The smallest block size will
 * be of order 0. The largest block size will be of order MAX_ORDER.
 *
 * MAX_ORDER    = MAX_BLOCK_LOG2 - MIN_BLOCK_LOG2
 * order        = BLOCK_LOG2 - MIN_BLOCK_LOG2
 * block size   = 2^(order + MIN_BLOCK_LOG2)
 *
 * A collection of linked lists called free lists is maintained for all blocks
 * of free memory. Memory blocks of the same order are linked in the same list.
 * Linked list nodes are represented by the base address of the block. This is
 * done by casting the base address of the block to a linked list node pointer.
 * Blocks that are split or allocated should not be in the free lists.
 *
 * The minimum block size should be set to greater than the size of a linked
 * list node. This is 8 bytes on 32-bit systems and 16 bytes on 64-bit. The
 * maximum block size should not be greater than the total memory size.
 *
 * ================================= BIT TREE =================================
 * The bit tree maps the entire memory pool, with each bit representing a block
 * of memory. The tree will be a full binary tree, since the memory pool size
 * is a power of 2. This structure allows the height and total number of nodes
 * to be calculated as:
 *
 * height           = MEM_BLOCK_LOG2 - MIN_BLOCK_LOG2
 * TOTAL_TREE_NODES = 2^(height + 1) - 1
 *
 * The bit tree is used for easy and fast tracking of memory block states. The
 * bit tree tracks two states:
 *
 * 0 - free, not split, available for allocation or merging
 * 1 - allocated, split, or reserved, not available for allocation or merging
 *
 * The bit tree is represented in an array of 32-bit words as a level order
 * traversal of the tree. Each bit of a word represents a node in the bit tree.
 * With the base address of the memory pool and given a block's address and
 * order, the block's bit tree index and array index can be calculated with:
 *
 * offset       = (address - base) / 2^(MIN_BLOCK_LOG2 + order)
 * index        = 2^height - 1 + offset
 *
 * array index  = index / 32
 * word offset  = index % 32
 *
 * If the max block size is smaller than the memory pool size, some of the
 * higher nodes in the tree will be unused. Since the tree is indexed in level
 * order, the array can be truncated to skip these nodes. The bit tree index
 * calculation needs to be modified to account for the truncated nodes.
 *
 * TRUNCATED_TREE_NODES = 2^(MEM_BLOCK_LOG2 - MAX_BLOCK_LOG2) - 1
 * index                = 2^height - 1 + offset - TRUNCATED_TREE_NODES
 */

#define MEM_BLOCK_LOG2 8
#define MAX_BLOCK_LOG2 8
#define MIN_BLOCK_LOG2 4

_Static_assert(MIN_BLOCK_LOG2 > 3);
_Static_assert(MIN_BLOCK_LOG2 <= MAX_BLOCK_LOG2);
_Static_assert(MAX_BLOCK_LOG2 <= MEM_BLOCK_LOG2);

#define MAX_ORDER (MAX_BLOCK_LOG2 - MIN_BLOCK_LOG2)

#define TOTAL_TREE_NODES ((1 << (MEM_BLOCK_LOG2 - MIN_BLOCK_LOG2 + 1)) - 1)
#define TRUNCATED_TREE_NODES ((1 << (MEM_BLOCK_LOG2 - MAX_BLOCK_LOG2)) - 1)
#define TREE_NODES (TOTAL_TREE_NODES - TRUNCATED_TREE_NODES)
#define TREE_WORDS ((TREE_NODES + 31) / 32)

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

/* Initializes the allocator. The buddy struct is allocated within the provided
 * memory pool. The bit tree is initialized as all free, only marking memory
 * allocated for the buddy struct as used. Free blocks of memory are added to
 * the free lists. Memory not added to free lists at this point will never be
 * available for allocation. Returns NULL if initialization fails.
 */
buddy_t *buddy_init(char *, size_t);

/* Allocates a best-fit block of memory for the requested size. Larger blocks
 * may be split to obtain the best-fit block size. Returns NULL if allocation
 * fails.
 */
void *buddy_malloc(buddy_t *, size_t);

/* Deallocates a memory block allocated by buddy_malloc. Blocks deallocated are
 * continuously merged with its buddy block if possible.
 */
void buddy_free(buddy_t *, void *, size_t);

#endif