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
 * Each bit in the bit tree represents a block
 * The height of the bit in the tree represents its order
 * 0 - free, 1 - allocated
 *
 * height = MAX_ORDER - order
 * offset = (address - base) / (MIN_BLOCK_SIZE * 2^order)
 * index  = 2^height - 1 + offset
 *
 * 1048576  8 (19)  1   2^0
 * 524288   7 (18)  2   2^1
 * 262144   6 (17)  4   2^2
 * 131072   5 (16)  8   2^3
 * 65536    4 (15)  16  2^4
 * 32768    3 (14)  32  2^5
 * 16384    2 (13)  64  2^6
 * 8192     1 (12)  128 2^7
 * 4096     0 (11)  256 2^8
 */

/* Free lists
 * A linked list is maintained for pages of each order
 */
struct partition {
    struct partition *prev;
    struct partition *next;
};
typedef struct partition partition_t;
partition_t *free_lists[MAX_ORDER + 1];

/* HELPER FUNCTIONS */
uint8_t get_order(uintptr_t length) {
    for (int n = MAX_ORDER; n >= 0; n--) {
        if ((uintptr_t)(2 << (n + 11)) <= length) return n;
    }
    return 0;
}

void buddy_init(char *base, size_t length) {
    // Initialize free lists
    for (int i = 0; i <= MAX_ORDER; i++) {
        free_lists[i] = NULL;
    }

    // Mark memory as free
    while (length > 0) {
        uint8_t order = get_order(length);
        int partition_size = 2 << (order + 11);

        printf("%hhu: marked %d as free\n", order, partition_size);

        partition_t *p = (partition_t *)(base);

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