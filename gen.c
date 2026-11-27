/*
 * Generator danych do pomiarow czasu (sprawozdanie, sekcja Testy).
 *
 * Uzycie: gen rodzina n k R bity porzadek ziarno plik_wyjsciowy
 *
 * Zapisuje R zbiorow po n liczb we wspolnym formacie zadania. Dla tych
 * samych argumentow plik jest zawsze identyczny (wlasny generator
 * splitmix64, niezalezny od rand() danej biblioteki C).
 *
 * rodzina:
 *   pes     - a_1..a_{n-1} = k, a_n = k(n+1); podzial idealny nie istnieje,
 *             a S jest podzielna przez k (pesymistyczna rodzina z raportu);
 *             argument bity jest ignorowany
 *   los     - liczby losowe z [1, 2^bity - 1]; dla bity <= 64 ostatnia liczba
 *             jest korygowana o mniej niz k tak, aby S byla podzielna przez k
 *   idealny - k grup o rownej sumie (podzial idealny istnieje z konstrukcji),
 *             elementy grup wymieszane; bity <= 40
 * porzadek: los (bez zmian), nier (nierosnaco), niem (niemalejaco);
 *           ta sama rodzina i ziarno daja ten sam multizbior w kazdym porzadku
 */
#include "big_uint.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { FAMILY_PES, FAMILY_LOS, FAMILY_IDEALNY } Family;
typedef enum { ORDER_RANDOM, ORDER_DESC, ORDER_ASC } Order;

static uint64_t rng_state;

/* splitmix64 */
static uint64_t rng_next(void)
{
    uint64_t z = (rng_state += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

/* Liczba jednostajnie z [0, bound), bound > 0, bez obciazenia modulo. */
static uint64_t rng_below(uint64_t bound)
{
    uint64_t skip = (UINT64_MAX % bound + 1) % bound; /* 2^64 mod bound */
    uint64_t x;
    do {
        x = rng_next();
    } while (x > UINT64_MAX - skip);
    return x % bound;
}

/* Liczba jednostajnie z [1, 2^bits - 1], 1 <= bits <= 64. */
static uint64_t rng_bits_positive(unsigned bits)
{
    uint64_t max = bits == 64 ? UINT64_MAX : (UINT64_C(1) << bits) - 1;
    return 1 + rng_below(max);
}

static bool parse_u64(const char *text, uint64_t *out)
{
    if (*text < '0' || *text > '9') {
        return false;
    }
    uint64_t value = 0;
    for (; *text != '\0'; ++text) {
        if (*text < '0' || *text > '9') {
            return false;
        }
        uint64_t digit = (uint64_t)(*text - '0');
        if (value > (UINT64_MAX - digit) / 10) {
            return false;
        }
        value = 10 * value + digit;
    }
    *out = value;
    return true;
}

static bool write_u64(FILE *out, uint64_t value)
{
    char buffer[21];
    size_t pos = sizeof(buffer);
    buffer[--pos] = '\0';
    do {
        buffer[--pos] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    return fputs(buffer + pos, out) != EOF;
}

static int compare_desc(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;
    return (x < y) - (x > y);
}

static int compare_asc(const void *a, const void *b)
{
    return compare_desc(b, a);
}

static bool generate_pes(uint64_t *a, size_t n, size_t k)
{
    if ((uint64_t)n + 1 > UINT64_MAX / k) {
        return false;
    }
    for (size_t i = 0; i + 1 < n; ++i) {
        a[i] = k;
    }
    a[n - 1] = (uint64_t)k * ((uint64_t)n + 1);
    return true;
}

static void generate_los(uint64_t *a, size_t n, size_t k, unsigned bits)
{
    uint64_t remainder = 0;
    for (size_t i = 0; i < n; ++i) {
        a[i] = rng_bits_positive(bits);
        remainder = (remainder + a[i] % k) % k;
    }
    if (remainder != 0) {
        if (a[n - 1] > remainder) {
            a[n - 1] -= remainder;
        } else {
            a[n - 1] += k - remainder; /* a[n-1] <= remainder < k */
        }
    }
}

static bool generate_idealny(uint64_t *a, size_t n, size_t k, unsigned bits)
{
    const uint64_t weight_limit = UINT64_C(1) << 16;
    size_t base = n / k;
    size_t extra = n % k;
    /* Grupa 0 jest najwieksza: jej suma wyznacza wspolna sume grup. */
    size_t size0 = base + (extra > 0);
    uint64_t target = 0;
    for (size_t i = 0; i < size0; ++i) {
        a[i] = rng_bits_positive(bits);
        target += a[i];
    }
    if (target > UINT64_MAX / weight_limit) {
        return false;
    }
    size_t pos = size0;
    for (size_t j = 1; j < k; ++j) {
        size_t size = base + (j < extra);
        uint64_t total_weight = 0;
        for (size_t i = 0; i < size; ++i) {
            a[pos + i] = 1 + rng_below(weight_limit);
            total_weight += a[pos + i];
        }
        uint64_t sum = 0;
        for (size_t i = 0; i < size; ++i) {
            a[pos + i] = target * a[pos + i] / total_weight;
            sum += a[pos + i];
        }
        a[pos + rng_below(size)] += target - sum;
        pos += size;
    }
    for (size_t i = n - 1; i > 0; --i) {
        size_t j = (size_t)rng_below((uint64_t)i + 1);
        uint64_t tmp = a[i];
        a[i] = a[j];
        a[j] = tmp;
    }
    return true;
}

/* Liczba losowa z [0, 2^bits) dla bits > 64, zapisana dziesietnie. */
static bool write_big_random(FILE *out, uint32_t *limbs, unsigned bits)
{
    size_t count = (bits + 31) / 32;
    for (size_t i = 0; i < count; ++i) {
        limbs[i] = (uint32_t)(rng_next() >> 32);
    }
    unsigned top_bits = bits - 32 * (unsigned)(count - 1);
    if (top_bits < 32) {
        limbs[count - 1] &= (UINT32_C(1) << top_bits) - 1;
    }
    size_t len = count;
    while (len > 0 && limbs[len - 1] == 0) {
        --len;
    }
    BigUInt value = {limbs, len, count};
    char *text = NULL;
    if (big_uint_to_decimal(&value, &text) != BIG_UINT_OK) {
        return false;
    }
    bool ok = fputs(text, out) != EOF;
    free(text);
    return ok;
}

static void usage(const char *program)
{
    fprintf(stderr,
            "Uzycie: %s rodzina n k R bity porzadek ziarno plik_wyjsciowy\n"
            "  rodzina:  pes | los | idealny\n"
            "  porzadek: los | nier | niem\n",
            program);
}

int main(int argc, char **argv)
{
    if (argc != 9) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    Family family;
    if (strcmp(argv[1], "pes") == 0) {
        family = FAMILY_PES;
    } else if (strcmp(argv[1], "los") == 0) {
        family = FAMILY_LOS;
    } else if (strcmp(argv[1], "idealny") == 0) {
        family = FAMILY_IDEALNY;
    } else {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    Order order;
    if (strcmp(argv[6], "los") == 0) {
        order = ORDER_RANDOM;
    } else if (strcmp(argv[6], "nier") == 0) {
        order = ORDER_DESC;
    } else if (strcmp(argv[6], "niem") == 0) {
        order = ORDER_ASC;
    } else {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    uint64_t n64, k64, sets, bits64, seed;
    if (!parse_u64(argv[2], &n64) || !parse_u64(argv[3], &k64) ||
        !parse_u64(argv[4], &sets) || !parse_u64(argv[5], &bits64) ||
        !parse_u64(argv[7], &seed)) {
        fputs("n, k, R, bity i ziarno musza byc nieujemnymi liczbami calkowitymi.\n",
              stderr);
        return EXIT_FAILURE;
    }
    if (k64 < 2 || n64 < k64 || n64 > SIZE_MAX / sizeof(uint64_t) || sets < 1) {
        fputs("Wymagane: k >= 2, n >= k, R >= 1.\n", stderr);
        return EXIT_FAILURE;
    }
    if (bits64 < 1 || bits64 > 4096) {
        fputs("Wymagane: 1 <= bity <= 4096.\n", stderr);
        return EXIT_FAILURE;
    }
    if (family == FAMILY_IDEALNY && bits64 > 40) {
        fputs("Rodzina idealny wymaga bity <= 40.\n", stderr);
        return EXIT_FAILURE;
    }
    bool big = family == FAMILY_LOS && bits64 > 64;
    if (big && order != ORDER_RANDOM) {
        fputs("Dla bity > 64 dostepny jest tylko porzadek los.\n", stderr);
        return EXIT_FAILURE;
    }
    size_t n = (size_t)n64;
    size_t k = (size_t)k64;
    unsigned bits = (unsigned)bits64;
    rng_state = seed;

    uint64_t *values = NULL;
    uint32_t *limbs = NULL;
    if (big) {
        limbs = malloc(((bits + 31) / 32) * sizeof(*limbs));
    } else {
        values = malloc(n * sizeof(*values));
    }
    if (values == NULL && limbs == NULL) {
        fputs("Brak pamieci.\n", stderr);
        return EXIT_FAILURE;
    }
    FILE *out = fopen(argv[8], "wb");
    if (out == NULL) {
        perror(argv[8]);
        free(values);
        free(limbs);
        return EXIT_FAILURE;
    }
    static char buffer[1 << 20];
    setvbuf(out, buffer, _IOFBF, sizeof(buffer));

    bool ok = write_u64(out, sets) && fputc('\n', out) != EOF;
    for (uint64_t r = 0; ok && r < sets; ++r) {
        if (big) {
            for (size_t i = 0; ok && i < n; ++i) {
                ok = (i == 0 || fputc(' ', out) != EOF) &&
                     write_big_random(out, limbs, bits);
            }
        } else {
            if (family == FAMILY_PES) {
                ok = generate_pes(values, n, k);
            } else if (family == FAMILY_LOS) {
                generate_los(values, n, k, bits);
            } else {
                ok = generate_idealny(values, n, k, bits);
            }
            if (!ok) {
                fputs("Wartosci przekraczaja zakres 64 bitow.\n", stderr);
                break;
            }
            if (order == ORDER_DESC) {
                qsort(values, n, sizeof(*values), compare_desc);
            } else if (order == ORDER_ASC) {
                qsort(values, n, sizeof(*values), compare_asc);
            }
            for (size_t i = 0; ok && i < n; ++i) {
                ok = (i == 0 || fputc(' ', out) != EOF) && write_u64(out, values[i]);
            }
        }
        ok = ok && fputc('\n', out) != EOF;
    }
    if (fclose(out) != 0) {
        ok = false;
    }
    if (!ok) {
        fputs("Blad zapisu pliku wyjsciowego.\n", stderr);
    }
    free(values);
    free(limbs);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
