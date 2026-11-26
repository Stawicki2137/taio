#ifndef BIG_UINT_H
#define BIG_UINT_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t *limbs;
    size_t len;
    size_t cap;
} BigUInt;

#define BIG_UINT_INIT { NULL, 0, 0 }

typedef enum {
    BIG_UINT_OK,
    BIG_UINT_INVALID,    /* Błędny zapis dziesiętny. */
    BIG_UINT_NOMEM,      /* Brak pamięci lub rozmiar przekracza limit size_t. */
    BIG_UINT_UNDERFLOW   /* Różnica byłaby ujemna. */
} BigUIntStatus;

void big_uint_init(BigUInt *x);
void big_uint_free(BigUInt *x);

BigUIntStatus big_uint_copy(BigUInt *dst, const BigUInt *src);

BigUIntStatus big_uint_from_decimal(BigUInt *dst, const char *text);

BigUIntStatus big_uint_to_decimal(const BigUInt *x, char **out);

int big_uint_cmp(const BigUInt *a, const BigUInt *b);

BigUIntStatus big_uint_add(BigUInt *dst, const BigUInt *a, const BigUInt *b);
BigUIntStatus big_uint_add_assign(BigUInt *a, const BigUInt *b);

BigUIntStatus big_uint_sub(BigUInt *dst, const BigUInt *a, const BigUInt *b);

/* Dzieli x w miejscu przez divisor > 0 i zwraca resztę. O(1 + len). */
size_t big_uint_div_size(BigUInt *x, size_t divisor);

#endif
