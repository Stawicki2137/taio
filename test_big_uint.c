#include "big_uint.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do {                                                  \
    if (!(expr)) {                                                        \
        fprintf(stderr, "Blad w linii %d: %s\n", __LINE__, #expr);        \
        goto cleanup;                                                     \
    }                                                                     \
} while (0)

static int equals_decimal(const BigUInt *x, const char *expected)
{
    char *text = NULL;
    BigUIntStatus status = big_uint_to_decimal(x, &text);
    int equal = status == BIG_UINT_OK && strcmp(text, expected) == 0;
    free(text);
    return equal;
}

int main(void)
{
    const char nines[] =
        "9999999999" "9999999999" "9999999999" "9999999999" "9999999999"
        "9999999999" "9999999999" "9999999999" "9999999999" "9999999999";
    const char ten_to_100[] =
        "1"
        "0000000000" "0000000000" "0000000000" "0000000000" "0000000000"
        "0000000000" "0000000000" "0000000000" "0000000000" "0000000000";
    BigUInt a = BIG_UINT_INIT, b = BIG_UINT_INIT, result = BIG_UINT_INIT;
    int exit_code = EXIT_FAILURE;

    CHECK(big_uint_from_decimal(&a, "00000") == BIG_UINT_OK);
    CHECK(a.limbs == NULL && a.len == 0 && a.cap == 0);
    CHECK(big_uint_cmp(&a, &b) == 0 && equals_decimal(&a, "0"));
    CHECK(big_uint_add_assign(&a, &b) == BIG_UINT_OK && equals_decimal(&a, "0"));

    CHECK(big_uint_from_decimal(&a, "004294967295") == BIG_UINT_OK);
    CHECK(big_uint_from_decimal(&b, "1") == BIG_UINT_OK);
    CHECK(big_uint_add(&result, &a, &b) == BIG_UINT_OK);
    CHECK(result.len == 2 && result.limbs[0] == 0 && result.limbs[1] == 1);
    CHECK(equals_decimal(&result, "4294967296"));
    CHECK(big_uint_cmp(&a, &b) > 0 && big_uint_cmp(&a, &result) < 0);

    CHECK(big_uint_from_decimal(&a, "18446744073709551615") == BIG_UINT_OK);
    CHECK(big_uint_add_assign(&a, &b) == BIG_UINT_OK);
    CHECK(equals_decimal(&a, "18446744073709551616"));
    CHECK(big_uint_sub(&b, &a, &b) == BIG_UINT_OK);
    CHECK(equals_decimal(&b, "18446744073709551615"));
    CHECK(big_uint_sub(&b, &b, &a) == BIG_UINT_UNDERFLOW);
    CHECK(equals_decimal(&b, "18446744073709551615"));

    CHECK(big_uint_from_decimal(&a, nines) == BIG_UINT_OK);
    CHECK(big_uint_copy(&result, &a) == BIG_UINT_OK);
    CHECK(big_uint_cmp(&a, &result) == 0);
    CHECK(big_uint_from_decimal(&b, "1") == BIG_UINT_OK);
    CHECK(big_uint_add_assign(&a, &b) == BIG_UINT_OK);
    CHECK(equals_decimal(&a, ten_to_100) && equals_decimal(&result, nines));
    CHECK(big_uint_sub(&a, &a, &b) == BIG_UINT_OK);
    CHECK(big_uint_sub(&result, &result, &a) == BIG_UINT_OK);
    CHECK(result.limbs == NULL && result.len == 0 && result.cap == 0);

    CHECK(big_uint_add_assign(&b, &b) == BIG_UINT_OK && equals_decimal(&b, "2"));
    CHECK(big_uint_from_decimal(&b, "") == BIG_UINT_INVALID);
    CHECK(big_uint_from_decimal(&b, "12x") == BIG_UINT_INVALID);
    CHECK(equals_decimal(&b, "2"));
    exit_code = EXIT_SUCCESS;
    puts("OK - testy BigUInt");

cleanup:
    big_uint_free(&result);
    big_uint_free(&b);
    big_uint_free(&a);
    return exit_code;
}
