#include "big_uint.h"

#include <stdlib.h>
#include <string.h>

#define DECIMAL_BASE UINT32_C(1000000000)
enum { DECIMAL_DIGITS = 9 };

void big_uint_init(BigUInt *x)
{
    *x = (BigUInt)BIG_UINT_INIT;
}

void big_uint_free(BigUInt *x)
{
    free(x->limbs);
    big_uint_init(x);
}

static BigUIntStatus reserve(BigUInt *x, size_t need)
{
    if (need <= x->cap) {
        return BIG_UINT_OK;
    }

    const size_t max_limbs = SIZE_MAX / sizeof(*x->limbs);
    if (need > max_limbs) {
        return BIG_UINT_NOMEM;
    }
    size_t capacity = need;
    if (x->cap <= max_limbs / 2 && 2 * x->cap > need) {
        capacity = 2 * x->cap;
    }
    uint32_t *limbs = realloc(x->limbs, capacity * sizeof(*limbs));
    if (limbs == NULL) {
        return BIG_UINT_NOMEM;
    }
    x->limbs = limbs;
    x->cap = capacity;
    return BIG_UINT_OK;
}

static void normalize(BigUInt *x)
{
    while (x->len > 0 && x->limbs[x->len - 1] == 0) {
        --x->len;
    }
    if (x->len == 0) {
        big_uint_free(x);
    }
}

size_t big_uint_div_size(BigUInt *x, size_t divisor)
{
    size_t remainder = 0;
    for (size_t i = x->len; i > 0; --i) {
        uint32_t limb = x->limbs[i - 1];
        uint32_t quotient = 0;
        for (unsigned pos = 32; pos > 0; --pos) {
            uint32_t mask = UINT32_C(1) << (pos - 1);
            size_t bit = (limb & mask) != 0;
            /* Test 2*remainder + bit >= divisor bez przepełnienia size_t. */
            size_t gap = divisor - remainder - bit;
            if (remainder >= gap) {
                remainder -= gap;
                quotient |= mask;
            } else {
                remainder = 2 * remainder + bit;
            }
        }
        x->limbs[i - 1] = quotient;
    }
    normalize(x);
    return remainder;
}

BigUIntStatus big_uint_copy(BigUInt *dst, const BigUInt *src)
{
    if (dst == src) {
        return BIG_UINT_OK;
    }
    if (src->len == 0) {
        big_uint_free(dst);
        return BIG_UINT_OK;
    }
    BigUIntStatus status = reserve(dst, src->len);
    if (status != BIG_UINT_OK) {
        return status;
    }
    memcpy(dst->limbs, src->limbs, src->len * sizeof(*src->limbs));
    dst->len = src->len;
    return BIG_UINT_OK;
}

static BigUIntStatus mul_add_chunk(BigUInt *x, uint32_t factor, uint32_t chunk)
{
    uint64_t carry = chunk;
    for (size_t i = 0; i < x->len; ++i) {
        uint64_t product = (uint64_t)x->limbs[i] * factor + carry;
        x->limbs[i] = (uint32_t)product;
        carry = product >> 32;
    }
    if (carry != 0) {
        if (x->len == SIZE_MAX) {
            return BIG_UINT_NOMEM;
        }
        BigUIntStatus status = reserve(x, x->len + 1);
        if (status != BIG_UINT_OK) {
            return status;
        }
        x->limbs[x->len++] = (uint32_t)carry;
    }
    return BIG_UINT_OK;
}

BigUIntStatus big_uint_from_decimal(BigUInt *dst, const char *text)
{
    if (text == NULL || *text == '\0') {
        return BIG_UINT_INVALID;
    }
    while (*text == '0') {
        ++text;
    }
    for (const char *p = text; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') {
            return BIG_UINT_INVALID;
        }
    }

    BigUInt tmp = BIG_UINT_INIT;
    while (*text != '\0') {
        uint32_t chunk = 0;
        uint32_t factor = 1;
        for (unsigned i = 0; i < DECIMAL_DIGITS && *text != '\0'; ++i) {
            chunk = chunk * 10 + (uint32_t)(*text++ - '0');
            factor *= 10;
        }
        BigUIntStatus status = mul_add_chunk(&tmp, factor, chunk);
        if (status != BIG_UINT_OK) {
            big_uint_free(&tmp);
            return status;
        }
    }
    big_uint_free(dst);
    *dst = tmp;  
    return BIG_UINT_OK;
}

int big_uint_cmp(const BigUInt *a, const BigUInt *b)
{
    if (a->len != b->len) {
        return a->len < b->len ? -1 : 1;
    }
    for (size_t i = a->len; i > 0; --i) {
        if (a->limbs[i - 1] != b->limbs[i - 1]) {
            return a->limbs[i - 1] < b->limbs[i - 1] ? -1 : 1;
        }
    }
    return 0;
}

BigUIntStatus big_uint_add(BigUInt *dst, const BigUInt *a, const BigUInt *b)
{
    if (a->len == 0) {
        return big_uint_copy(dst, b);
    }
    if (b->len == 0) {
        return big_uint_copy(dst, a);
    }
    size_t n = a->len > b->len ? a->len : b->len;
    if (n == SIZE_MAX) {
        return BIG_UINT_NOMEM;
    }
    BigUIntStatus status = reserve(dst, n + 1);
    if (status != BIG_UINT_OK) {
        return status;
    }
    uint64_t carry = 0;
    for (size_t i = 0; i < n; ++i) {
        uint64_t left = i < a->len ? a->limbs[i] : 0;
        uint64_t right = i < b->len ? b->limbs[i] : 0;
        uint64_t sum = left + right + carry;
        dst->limbs[i] = (uint32_t)sum;
        carry = sum >> 32;
    }
    if (carry != 0) {
        dst->limbs[n++] = (uint32_t)carry;
    }
    dst->len = n;
    return BIG_UINT_OK;
}

BigUIntStatus big_uint_add_assign(BigUInt *a, const BigUInt *b)
{
    return big_uint_add(a, a, b);
}

BigUIntStatus big_uint_sub(BigUInt *dst, const BigUInt *a, const BigUInt *b)
{
    int order = big_uint_cmp(a, b);
    if (order < 0) {
        return BIG_UINT_UNDERFLOW;
    }
    if (order == 0) {
        big_uint_free(dst);
        return BIG_UINT_OK;
    }
    if (b->len == 0) {
        return big_uint_copy(dst, a);
    }
    BigUIntStatus status = reserve(dst, a->len);
    if (status != BIG_UINT_OK) {
        return status;
    }
    uint64_t borrow = 0;
    for (size_t i = 0; i < a->len; ++i) {
        uint64_t left = a->limbs[i];
        uint64_t right = (i < b->len ? b->limbs[i] : 0) + borrow;
        dst->limbs[i] = (uint32_t)(left - right);
        borrow = left < right;
    }
    dst->len = a->len;
    normalize(dst);
    return BIG_UINT_OK;
}

static uint32_t div_decimal_chunk(BigUInt *x)
{
    uint64_t remainder = 0;
    for (size_t i = x->len; i > 0; --i) {
        uint64_t value = (remainder << 32) | x->limbs[i - 1];
        x->limbs[i - 1] = (uint32_t)(value / DECIMAL_BASE);
        remainder = value % DECIMAL_BASE;
    }
    normalize(x);
    return (uint32_t)remainder;
}

BigUIntStatus big_uint_to_decimal(const BigUInt *x, char **out)
{
    if (x->len > (SIZE_MAX - 1) / 10) {
        return BIG_UINT_NOMEM;
    }
    size_t capacity = x->len == 0 ? 2 : 10 * x->len + 1;
    char *buffer = malloc(capacity);
    if (buffer == NULL) {
        return BIG_UINT_NOMEM;
    }
    BigUInt tmp = BIG_UINT_INIT;
    BigUIntStatus status = big_uint_copy(&tmp, x);
    if (status != BIG_UINT_OK) {
        free(buffer);
        return status;
    }

    size_t pos = capacity;
    buffer[--pos] = '\0';
    do {
        uint32_t chunk = div_decimal_chunk(&tmp);
        size_t end = pos;
        do {
            buffer[--pos] = (char)('0' + chunk % 10);
            chunk /= 10;
        } while (chunk != 0);
        if (tmp.len != 0) {
            while (end - pos < DECIMAL_DIGITS) {
                buffer[--pos] = '0';
            }
        }
    } while (tmp.len != 0);
    big_uint_free(&tmp);
    memmove(buffer, buffer + pos, capacity - pos);
    *out = buffer;
    return BIG_UINT_OK;
}
