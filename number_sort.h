#ifndef NUMBER_SORT_H
#define NUMBER_SORT_H

#include "big_uint.h"

#include <stdbool.h>

typedef struct {
    BigUInt value;
    size_t original_index; /* Indeksy wejścia zaczynają się od 1. */
} NumberItem;

/* Sortowanie przez scalanie. Tworzy tablicę odwołań do items, malejąco
 * według value, przy remisie rosnąco według original_index.
 * items musi pozostać przy życiu podczas używania wyniku.
 * Wywołujący zwalnia wyłącznie tablicę odwołań przez free().
 * Po błędzie *out pozostaje bez zmian. Dla n == 0 zwraca NULL.
 * Czas O(n log n * (1 + L)), pamięć O(n) słów i O(log n) stosu,
 * gdzie L to największa liczba fragmentów wartości wejściowej. */
bool number_sort_descending(const NumberItem *items, size_t n,
                            const NumberItem ***out);

#endif
