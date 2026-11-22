#ifndef BIG_UINT_H
#define BIG_UINT_H

#include <stddef.h>
#include <stdint.h>

/* Wartość to suma limbs[i] * 2^(32*i). Zero ma postać {NULL, 0, 0}.
 * Dla liczby dodatniej 0 < len <= cap oraz limbs[len - 1] != 0.
 * Pola służą implementacji. Każdy bufor ma jednego właściciela.
 * Do powielenia wartości używaj big_uint_copy. */
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

/* Inicjalizuj nowy obiekt przez BIG_UINT_INIT albo big_uint_init.
 * big_uint_free przywraca zero. Można go wywołać ponownie. O(1). */
void big_uint_init(BigUInt *x);
void big_uint_free(BigUInt *x);

/* Wszystkie wskaźniki na obiekty muszą być poprawne, a obiekty
 * zainicjalizowane, również dst. dst może być tym samym obiektem co
 * dowolny operand. Po błędzie poprzednia wartość wyniku jest zachowana.
 * Złożoności dotyczą liczby operacji na fragmentach o stałej szerokości. */

/* Głęboka kopia. O(n) dla n fragmentów src, O(1) dla kopii na siebie. */
BigUIntStatus big_uint_copy(BigUInt *dst, const BigUInt *src);

/* Akceptuje niepusty ciąg cyfr '0'..'9' zakończony NUL, także z zerami
 * wiodącymi. NULL zwraca BIG_UINT_INVALID. O(d + n^2) dla d cyfr
 * i n fragmentów wyniku. */
BigUIntStatus big_uint_from_decimal(BigUInt *dst, const char *text);

/* Zapis bez zer wiodących (zero jako "0"). Po sukcesie *out dostaje
 * nowy napis do zwolnienia przez free(). Poprzedni wskaźnik w *out
 * pozostaje własnością wywołującego. Po błędzie *out pozostaje bez zmian.
 * out musi wskazywać na zmienną typu char *. O(1 + n^2). */
BigUIntStatus big_uint_to_decimal(const BigUInt *x, char **out);

/* -1, 0, +1 dla a < b, a == b, a > b. O(1) dla różnych długości,
 * O(1 + n) w najgorszym przypadku dla równych długości n. */
int big_uint_cmp(const BigUInt *a, const BigUInt *b);

/* Czas obu funkcji O(1 + max(n, m)) dla n i m fragmentów argumentów. */
BigUIntStatus big_uint_add(BigUInt *dst, const BigUInt *a, const BigUInt *b);
BigUIntStatus big_uint_add_assign(BigUInt *a, const BigUInt *b);

/* dst = a - b. Dla a < b zwraca BIG_UINT_UNDERFLOW. O(1 + max(n, m)). */
BigUIntStatus big_uint_sub(BigUInt *dst, const BigUInt *a, const BigUInt *b);

#endif
