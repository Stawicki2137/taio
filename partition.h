#ifndef PARTITION_H
#define PARTITION_H

#include "number_sort.h"

typedef struct {
    BigUInt sum;
    size_t *indices; /* Oryginalne indeksy w kolejności przydziału. */
    size_t count;
    size_t capacity;
} PartitionPart;

typedef struct {
    PartitionPart *parts;
    size_t count;
    BigUInt spread; /* Największa suma minus najmniejsza suma. */
} PartitionResult;

#define PARTITION_RESULT_INIT { NULL, 0, BIG_UINT_INIT }

typedef enum {
    PARTITION_OK,
    PARTITION_IMPOSSIBLE, /* Brak podziału spełniającego warunki trybu. */
    PARTITION_NOMEM,
    PARTITION_INVALID    /* k < 2 */
} PartitionStatus;


void partition_result_free(PartitionResult *result);

PartitionStatus partition_descending(const NumberItem *items, size_t n, size_t k, PartitionResult *out);

/* Pierwszy podział idealny w kolejności licznika k-arnego.
 * Wejście pozostaje w oryginalnej kolejności, wynik ma elementy nierosnące.
 * Dla L >= 1 fragmentów sumy całkowitej koszt przeglądu wynosi
 * O(k^n * (n+k) * L), a pamięć dodatkowa O(n + k*L) słów. */
PartitionStatus partition_exact(const NumberItem *items, size_t n, size_t k, PartitionResult *out);

#endif
