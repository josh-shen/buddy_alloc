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
#define MAX_BLOCK_LOG2 20
#define MIN_BLOCK_LOG2 12
#define MAX_ORDER (MAX_BLOCK_LOG2 - MIN_BLOCK_LOG2)

void buddy_init(char *, size_t);

uintptr_t buddy_malloc(uintptr_t);

void buddy_free(uintptr_t);

#endif //BUDDY_H
