#ifndef MIN_HEAP_H
#define MIN_HEAP_H

#include "big_uint.h"

typedef struct {
    const BigUInt *sum;
    size_t part_index; /* Numer wewnętrzny od 0. Mniejszy wygrywa remis. */
} MinHeapEntry;

typedef struct {
    MinHeapEntry *entries;
    size_t count;
    size_t capacity;
} MinHeap;

/* Kopiec korzysta z bufora wywołującego i pożycza wskaźniki do sum.
 * Wywołujący odpowiada za czas życia bufora i sum.
 * Sumę można zmienić po wyjęciu jej wpisu, przed ponownym wstawieniem.
 * build przestawia n gotowych wpisów w storage. Czas O(n * (1 + S)),
 * pamięć dodatkowa O(1), gdzie S to maksymalna liczba fragmentów sumy.
 * Dla n == 0 dozwolone jest storage == NULL. */
void min_heap_build(MinHeap *heap, MinHeapEntry *storage, size_t n);

/* pop wymaga count > 0, push wymaga count < capacity.
 * Czas O((1 + log capacity) * (1 + S)), pamięć dodatkowa O(1).
 * Funkcje nie alokują pamięci. */
MinHeapEntry min_heap_pop(MinHeap *heap);
void min_heap_push(MinHeap *heap, MinHeapEntry entry);

#endif
