#include "number_sort.h"

#include <stdlib.h>
#include <string.h>

static bool before_or_equal(const NumberItem *a, const NumberItem *b)
{
    int order = big_uint_cmp(&a->value, &b->value);
    if (order != 0) {
        return order > 0;
    }
    return a->original_index <= b->original_index;
}

static void merge_sort(const NumberItem **items, const NumberItem **scratch,
                       size_t begin, size_t end)
{
    if (end - begin < 2) {
        return;
    }
    size_t middle = begin + (end - begin) / 2;
    merge_sort(items, scratch, begin, middle);
    merge_sort(items, scratch, middle, end);

    size_t left = begin;
    size_t right = middle;
    size_t pos = begin;
    while (left < middle && right < end) {
        if (before_or_equal(items[left], items[right])) {
            scratch[pos++] = items[left++];
        } else {
            scratch[pos++] = items[right++];
        }
    }
    while (left < middle) {
        scratch[pos++] = items[left++];
    }
    while (right < end) {
        scratch[pos++] = items[right++];
    }
    memcpy(items + begin, scratch + begin, (end - begin) * sizeof(*items));
}

bool number_sort_descending(const NumberItem *items, size_t n,
                            const NumberItem ***out)
{
    if (n == 0) {
        *out = NULL;
        return true;
    }
    if (n > SIZE_MAX / sizeof(const NumberItem *)) {
        return false;
    }
    const NumberItem **sorted = malloc(n * sizeof(*sorted));
    if (sorted == NULL) {
        return false;
    }
    const NumberItem **scratch = malloc(n * sizeof(*scratch));
    if (scratch == NULL) {
        free(sorted);
        return false;
    }
    for (size_t i = 0; i < n; ++i) {
        sorted[i] = &items[i];
    }
    merge_sort(sorted, scratch, 0, n);
    free(scratch);
    *out = sorted;
    return true;
}
