#include "partition.h"

#include <stdlib.h>

static bool next_assignment(size_t *assignment, size_t n, size_t k)
{
    for (size_t i = n; i > 0; --i) {
        if (++assignment[i - 1] < k) {
            return true;
        }
        assignment[i - 1] = 0;
    }
    return false;
}

static bool collect_indices(const NumberItem *items, size_t n,
                            const size_t *assignment, PartitionResult *result)
{
    for (size_t i = 0; i < result->count; ++i) {
        PartitionPart *part = &result->parts[i];
        part->indices = malloc(part->count * sizeof(*part->indices));
        if (part->indices == NULL) {
            return false;
        }
        part->capacity = part->count;
        part->count = 0;
    }
    const NumberItem **sorted = NULL;
    if (!number_sort_descending(items, n, &sorted)) {
        return false;
    }
    for (size_t i = 0; i < n; ++i) {
        size_t position = (size_t)(sorted[i] - items);
        PartitionPart *part = &result->parts[assignment[position]];
        part->indices[part->count++] = sorted[i]->original_index;
    }
    free(sorted);
    return true;
}

PartitionStatus partition_exact(const NumberItem *items, size_t n, size_t k,
                                PartitionResult *out)
{
    if (k < 2) {
        return PARTITION_INVALID;
    }
    if (n < k) {
        return PARTITION_IMPOSSIBLE;
    }
    PartitionStatus status = PARTITION_NOMEM;
    BigUInt target = BIG_UINT_INIT;
    PartitionResult candidate = PARTITION_RESULT_INIT;
    size_t *assignment = NULL;
    for (size_t i = 0; i < n; ++i) {
        if (big_uint_add_assign(&target, &items[i].value) != BIG_UINT_OK) {
            goto cleanup;
        }
    }
    if (big_uint_div_size(&target, k) != 0) {
        status = PARTITION_IMPOSSIBLE;
        goto cleanup;
    }
    if (k > SIZE_MAX / sizeof(*candidate.parts) || n > SIZE_MAX / sizeof(*assignment)) {
        goto cleanup;
    }
    assignment = calloc(n, sizeof(*assignment));
    candidate.parts = malloc(k * sizeof(*candidate.parts));
    if (assignment == NULL || candidate.parts == NULL) {
        goto cleanup;
    }
    for (size_t j = 0; j < k; ++j) {
        candidate.parts[j] = (PartitionPart){BIG_UINT_INIT, NULL, 0, 0};
    }
    candidate.count = k;
    do {
        for (size_t i = 0; i < n; ++i) {
            PartitionPart *part = &candidate.parts[assignment[i]];
            if (big_uint_add_assign(&part->sum, &items[i].value) != BIG_UINT_OK) {
                goto cleanup;
            }
            ++part->count;
        }
        size_t j = 0;
        while (j < k && candidate.parts[j].count > 0 &&
               big_uint_cmp(&candidate.parts[j].sum, &target) == 0) {
            ++j;
        }
        if (j == k) {
            if (!collect_indices(items, n, assignment, &candidate)) {
                goto cleanup;
            }
            partition_result_free(out);
            *out = candidate; 
            candidate = (PartitionResult)PARTITION_RESULT_INIT;
            status = PARTITION_OK;
            goto cleanup;
        }
        for (j = 0; j < k; ++j) {
            big_uint_free(&candidate.parts[j].sum);
            candidate.parts[j].count = 0;
        }
    } while (next_assignment(assignment, n, k));
    status = PARTITION_IMPOSSIBLE;
cleanup:
    free(assignment);
    partition_result_free(&candidate);
    big_uint_free(&target);
    return status;
}
