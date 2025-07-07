//
// Created by Josh Shen on 7/5/25.
//

#ifndef BUDDY_H
#define BUDDY_H

#include <stdint.h>
#include <stddef.h>

#define MIN_ORDER 0
#define MAX_ORDER 8

void buddy_init(char *, size_t);

uintptr_t buddy_malloc(uintptr_t);

void buddy_free(uintptr_t);

#endif //BUDDY_H
