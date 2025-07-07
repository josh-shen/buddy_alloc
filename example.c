//
// Created by Josh Shen on 7/5/25.
//
#include <stdio.h>

#include "buddy.h"

int main() {
    char memory[2 << 19];

    buddy_init(memory, sizeof(memory));

    // buddy_malloc()

    // buddy_free()

    return 0;
}
