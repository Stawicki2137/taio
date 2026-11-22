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
    PARTITION_IMPOSSIBLE, /* n < k */
    PARTITION_NOMEM,
    PARTITION_INVALID    /* k < 2 */
} PartitionStatus;

/* Zwalnia wynik i przywraca stan PARTITION_RESULT_INIT.
 * Czas O(k) wywołań free, gdzie k oznacza liczbę części. */
void partition_result_free(PartitionResult *result);

/* Wynik out musi być zainicjalizowany. Funkcja pożycza items tylko na czas
 * wywołania, a wynik jest jego niezależną własnością.
 * Każde wystąpienie trafia dokładnie raz do niepustej części.
 * Po błędzie out pozostaje bez zmian.
 * Dla L fragmentów największej wartości i S fragmentów największej sumy
 * czas wynosi O(n log n * (1+L) + k*(1+S)
 *                + (n-k)*(1+S)*(1+log k)).
 * Pamięć dodatkowa O(n + k*(1+S)) słów, poza wejściem. */
PartitionStatus partition_descending(const NumberItem *items, size_t n,
                                     size_t k, PartitionResult *out);

#endif
