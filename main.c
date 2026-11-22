#include "partition.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    NumberItem *items;
    size_t count;
    size_t capacity;
} Dataset;

typedef enum {
    LINE_OK,
    LINE_END,
    LINE_NOMEM,
    LINE_IO_ERROR,
    LINE_INVALID
} LineStatus;

/* Bufor rośnie geometrycznie. Czas amortyzowany O(długość wiersza). */
static LineStatus read_line(FILE *input, char **buffer, size_t *capacity)
{
    if (*buffer == NULL) {
        *capacity = 256;
        *buffer = malloc(*capacity);
        if (*buffer == NULL) {
            return LINE_NOMEM;
        }
    }
    size_t length = 0;
    int ch;
    while ((ch = fgetc(input)) != EOF && ch != '\n') {
        if (ch == '\0') {
            return LINE_INVALID;
        }
        if (length == *capacity - 1) {
            if (*capacity > SIZE_MAX / 2) {
                return LINE_NOMEM;
            }
            size_t new_capacity = 2 * *capacity;
            char *grown = realloc(*buffer, new_capacity);
            if (grown == NULL) {
                return LINE_NOMEM;
            }
            *buffer = grown;
            *capacity = new_capacity;
        }
        (*buffer)[length++] = (char)ch;
    }
    if (ferror(input)) {
        return LINE_IO_ERROR;
    }
    if (ch == EOF && length == 0) {
        return LINE_END;
    }
    (*buffer)[length] = '\0';
    return LINE_OK;
}

static void report_line_error(LineStatus status)
{
    switch (status) {
    case LINE_END:
        fputs("Nieoczekiwany koniec pliku.\n", stderr);
        break;
    case LINE_NOMEM:
        fputs("Brak pamieci na wiersz wejsciowy.\n", stderr);
        break;
    case LINE_IO_ERROR:
        fputs("Blad odczytu pliku.\n", stderr);
        break;
    case LINE_INVALID:
        fputs("Wejscie zawiera niedozwolony bajt NUL.\n", stderr);
        break;
    case LINE_OK:
        break;
    }
}

/* Liczniki i k mieszczą się w size_t. Wartości zbioru korzystają z BigUInt. */
static bool parse_size(const char *text, size_t *out)
{
    while (isspace((unsigned char)*text)) {
        ++text;
    }
    if (*text < '0' || *text > '9') {
        return false;
    }
    size_t value = 0;
    do {
        size_t digit = (size_t)(*text++ - '0');
        if (value > (SIZE_MAX - digit) / 10) {
            return false;
        }
        value = 10 * value + digit;
    } while (*text >= '0' && *text <= '9');
    while (isspace((unsigned char)*text)) {
        ++text;
    }
    if (*text != '\0') {
        return false;
    }
    *out = value;
    return true;
}

static void dataset_free(Dataset *dataset)
{
    for (size_t i = 0; i < dataset->count; ++i) {
        big_uint_free(&dataset->items[i].value);
    }
    free(dataset->items);
    *dataset = (Dataset){NULL, 0, 0};
}

static bool dataset_reserve(Dataset *dataset)
{
    if (dataset->count < dataset->capacity) {
        return true;
    }
    const size_t limit = SIZE_MAX / sizeof(*dataset->items);
    if (dataset->capacity == limit) {
        return false;
    }
    size_t capacity = 1;
    if (dataset->capacity > 0) {
        capacity = dataset->capacity > limit / 2 ? limit : 2 * dataset->capacity;
    }
    NumberItem *items = realloc(dataset->items, capacity * sizeof(*items));
    if (items == NULL) {
        return false;
    }
    dataset->items = items;
    dataset->capacity = capacity;
    return true;
}

/* Dzieli pojedynczy wiersz na liczby. Indeks odpowiada pozycji we wierszu.
 * Białe znaki, w tym CR z zakończenia CRLF, są separatorami.
 * Po błędzie dataset zawiera poprawnie zainicjalizowane obiekty do zwolnienia. */
static BigUIntStatus parse_dataset(char *line, Dataset *dataset)
{
    char *cursor = line;
    while (*cursor != '\0') {
        while (isspace((unsigned char)*cursor)) {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        char *start = cursor;
        while (*cursor != '\0' && !isspace((unsigned char)*cursor)) {
            ++cursor;
        }
        char separator = *cursor;
        *cursor = '\0';
        if (!dataset_reserve(dataset)) {
            return BIG_UINT_NOMEM;
        }
        BigUInt value = BIG_UINT_INIT;
        BigUIntStatus status = big_uint_from_decimal(&value, start);
        if (status != BIG_UINT_OK) {
            big_uint_free(&value);
            return status;
        }
        size_t index = dataset->count + 1;
        dataset->items[dataset->count++] = (NumberItem){value, index};
        if (separator == '\0') {
            break;
        }
        ++cursor;
    }
    return BIG_UINT_OK;
}

static bool print_number(const BigUInt *value)
{
    char *text = NULL;
    if (big_uint_to_decimal(value, &text) != BIG_UINT_OK) {
        fputs("Brak pamieci przy konwersji wyniku na zapis dziesietny.\n", stderr);
        return false;
    }
    fputs(text, stdout);
    free(text);
    return true;
}

static bool print_result(const PartitionResult *result)
{
    for (size_t i = 0; i < result->count; ++i) {
        const PartitionPart *part = &result->parts[i];
        printf("Czesc %zu | indeksy", i + 1);
        for (size_t j = 0; j < part->count; ++j) {
            printf(" %zu", part->indices[j]);
        }
        fputs(" | suma ", stdout);
        if (!print_number(&part->sum)) {
            return false;
        }
        putchar('\n');
    }
    fputs("Roznica ", stdout);
    if (!print_number(&result->spread)) {
        return false;
    }
    putchar('\n');
    return true;
}

static bool process_dataset(char *line, size_t k, size_t dataset_index)
{
    Dataset dataset = {NULL, 0, 0};
    PartitionResult result = PARTITION_RESULT_INIT;
    bool success = false;
    BigUIntStatus parse_status = parse_dataset(line, &dataset);
    if (parse_status != BIG_UINT_OK) {
        fprintf(stderr, "Zbior %zu: %s\n", dataset_index,
                parse_status == BIG_UINT_INVALID
                    ? "oczekiwano nieujemnych liczb dziesietnych."
                    : "brak pamieci na dane wejsciowe.");
        goto cleanup;
    }
    PartitionStatus status =
        partition_descending(dataset.items, dataset.count, k, &result);
    if (status == PARTITION_IMPOSSIBLE) {
        printf("Zbior %zu\nBRAK\n\n", dataset_index);
        success = true;
        goto cleanup;
    }
    if (status != PARTITION_OK) {
        fprintf(stderr, "Zbior %zu: brak pamieci na obliczenie podzialu.\n",
                dataset_index);
        goto cleanup;
    }
    printf("Zbior %zu\n", dataset_index);
    success = print_result(&result);
    putchar('\n');

cleanup:
    partition_result_free(&result);
    dataset_free(&dataset);
    return success;
}

static int process_input(FILE *input, size_t k)
{
    char *line = NULL;
    size_t capacity = 0;
    size_t dataset_count;
    int exit_code = EXIT_FAILURE;
    LineStatus status = read_line(input, &line, &capacity);
    if (status != LINE_OK) {
        report_line_error(status);
        goto cleanup;
    }
    if (!parse_size(line, &dataset_count)) {
        fputs("Pierwszy wiersz musi zawierac liczbe zbiorow mieszczaca sie w size_t.\n",
              stderr);
        goto cleanup;
    }
    for (size_t i = 0; i < dataset_count; ++i) {
        status = read_line(input, &line, &capacity);
        if (status != LINE_OK) {
            fprintf(stderr, "Nie mozna odczytac zbioru %zu.\n", i + 1);
            report_line_error(status);
            goto cleanup;
        }
        if (!process_dataset(line, k, i + 1)) {
            goto cleanup;
        }
        if (ferror(stdout)) {
            goto cleanup;
        }
    }
    /* Po zadeklarowanych zbiorach dozwolone są końcowe białe znaki. */
    int ch;
    while ((ch = fgetc(input)) != EOF) {
        if (!isspace((unsigned char)ch)) {
            fputs("Plik zawiera dane po zadeklarowanej liczbie zbiorow.\n", stderr);
            goto cleanup;
        }
    }
    if (ferror(input)) {
        report_line_error(LINE_IO_ERROR);
        goto cleanup;
    }
    exit_code = EXIT_SUCCESS;

cleanup:
    free(line);
    return exit_code;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Uzycie: %s plik_wejsciowy k\n", argv[0]);
        return EXIT_FAILURE;
    }
    size_t k;
    if (!parse_size(argv[2], &k) || k < 2) {
        fputs("k musi byc liczba calkowita >= 2 mieszczaca sie w size_t.\n", stderr);
        return EXIT_FAILURE;
    }
    FILE *input = fopen(argv[1], "rb");
    if (input == NULL) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }
    int exit_code = process_input(input, k);
    if (fclose(input) != 0) {
        fputs("Blad zamykania pliku wejsciowego.\n", stderr);
        exit_code = EXIT_FAILURE;
    }
    if (fflush(stdout) == EOF || ferror(stdout)) {
        fputs("Blad zapisu wyniku.\n", stderr);
        exit_code = EXIT_FAILURE;
    }
    return exit_code;
}
