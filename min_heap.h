#ifndef MIN_HEAP_H
#define MIN_HEAP_H

#include "big_uint.h"

typedef struct {
    const BigUInt *sum;
    size_t part_index; 
} MinHeapEntry;

typedef struct {
    MinHeapEntry *entries;
    size_t count;
    size_t capacity;
} MinHeap;

void min_heap_build(MinHeap *heap, MinHeapEntry *storage, size_t n);

MinHeapEntry min_heap_pop(MinHeap *heap);
void min_heap_push(MinHeap *heap, MinHeapEntry entry);

#endif
