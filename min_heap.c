#include "min_heap.h"

#include <assert.h>
#include <stdbool.h>

static bool less(MinHeapEntry a, MinHeapEntry b)
{
    int order = big_uint_cmp(a.sum, b.sum);
    if (order != 0) {
        return order < 0;
    }
    return a.part_index < b.part_index;
}

static void sift_down(MinHeap *heap, size_t root)
{
    MinHeapEntry entry = heap->entries[root];
    while (root < heap->count / 2) {
        size_t child = 2 * root + 1;
        if (child + 1 < heap->count &&
            less(heap->entries[child + 1], heap->entries[child])) {
            ++child;
        }
        if (!less(heap->entries[child], entry)) {
            break;
        }
        heap->entries[root] = heap->entries[child];
        root = child;
    }
    heap->entries[root] = entry;
}

void min_heap_build(MinHeap *heap, MinHeapEntry *storage, size_t n)
{
    *heap = (MinHeap){storage, n, n};
    for (size_t i = n / 2; i > 0; --i) {
        sift_down(heap, i - 1);
    }
}

MinHeapEntry min_heap_pop(MinHeap *heap)
{
    assert(heap->count > 0);
    MinHeapEntry result = heap->entries[0];
    --heap->count;
    if (heap->count > 0) {
        heap->entries[0] = heap->entries[heap->count];
        sift_down(heap, 0);
    }
    return result;
}

void min_heap_push(MinHeap *heap, MinHeapEntry entry)
{
    assert(heap->count < heap->capacity);
    size_t pos = heap->count++;
    while (pos > 0) {
        size_t parent = (pos - 1) / 2;
        if (!less(entry, heap->entries[parent])) {
            break;
        }
        heap->entries[pos] = heap->entries[parent];
        pos = parent;
    }
    heap->entries[pos] = entry;
}
