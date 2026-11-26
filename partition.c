#include "partition.h"
#include "min_heap.h"

#include <stdlib.h>

void partition_result_free(PartitionResult *result)
{
    for (size_t i = 0; i < result->count; ++i) {
        big_uint_free(&result->parts[i].sum);
        free(result->parts[i].indices);
    }
    free(result->parts);
    big_uint_free(&result->spread);
    *result = (PartitionResult)PARTITION_RESULT_INIT;
}

static bool append_index(PartitionPart *part, size_t index)
{
    if (part->count == part->capacity) {
        const size_t limit = SIZE_MAX / sizeof(*part->indices);
        if (part->capacity == limit) {
            return false;
        }
        size_t capacity = 1;
        if (part->capacity > 0) {
            capacity = part->capacity > limit / 2 ? limit : 2 * part->capacity;
        }
        size_t *indices = realloc(part->indices, capacity * sizeof(*indices));
        if (indices == NULL) {
            return false;
        }
        part->indices = indices;
        part->capacity = capacity;
    }
    part->indices[part->count++] = index;
    return true;
}

PartitionStatus partition_descending(const NumberItem *items, size_t n,
                                     size_t k, PartitionResult *out)
{
    if (k < 2) {
        return PARTITION_INVALID;
    }
    if (n < k) {
        return PARTITION_IMPOSSIBLE;
    }
    if (k > SIZE_MAX / sizeof(PartitionPart) ||
        k > SIZE_MAX / sizeof(MinHeapEntry)) {
        return PARTITION_NOMEM;
    }

    PartitionStatus status = PARTITION_NOMEM;
    PartitionResult tmp = PARTITION_RESULT_INIT;
    const NumberItem **sorted = NULL;
    MinHeapEntry *entries = NULL;
    if (!number_sort_descending(items, n, &sorted)) {
        goto cleanup;
    }
    tmp.parts = malloc(k * sizeof(*tmp.parts));
    if (tmp.parts == NULL) {
        goto cleanup;
    }
    for (size_t i = 0; i < k; ++i) {
        tmp.parts[i] = (PartitionPart){BIG_UINT_INIT, NULL, 0, 0};
    }
    tmp.count = k;
    entries = malloc(k * sizeof(*entries));
    if (entries == NULL) {
        goto cleanup;
    }
    for (size_t i = 0; i < k; ++i) {
        if (!append_index(&tmp.parts[i], sorted[i]->original_index) ||
            big_uint_copy(&tmp.parts[i].sum, &sorted[i]->value) != BIG_UINT_OK) {
            goto cleanup;
        }
        entries[i] = (MinHeapEntry){&tmp.parts[i].sum, i};
    }

    MinHeap heap;
    min_heap_build(&heap, entries, k);
    for (size_t i = k; i < n; ++i) {
        MinHeapEntry entry = min_heap_pop(&heap);
        PartitionPart *part = &tmp.parts[entry.part_index];
        if (!append_index(part, sorted[i]->original_index) ||
            big_uint_add_assign(&part->sum, &sorted[i]->value) != BIG_UINT_OK) {
            goto cleanup;
        }
        min_heap_push(&heap, entry);
    }

    const BigUInt *minimum = heap.entries[0].sum;
    const BigUInt *maximum = &tmp.parts[0].sum;
    for (size_t i = 1; i < k; ++i) {
        if (big_uint_cmp(&tmp.parts[i].sum, maximum) > 0) {
            maximum = &tmp.parts[i].sum;
        }
    }
    if (big_uint_sub(&tmp.spread, maximum, minimum) != BIG_UINT_OK) {
        goto cleanup;
    }
    partition_result_free(out);
    *out = tmp;
    tmp = (PartitionResult)PARTITION_RESULT_INIT;
    status = PARTITION_OK;

cleanup:
    free(entries);
    free(sorted);
    partition_result_free(&tmp);
    return status;
}
