#ifndef NUMBER_SORT_H
#define NUMBER_SORT_H

#include "big_uint.h"

#include <stdbool.h>

typedef struct {
    BigUInt value;
    size_t original_index; 
} NumberItem;

bool number_sort_descending(const NumberItem *items, size_t n, const NumberItem ***out);

#endif
