#include "partition.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

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

static bool write_number(FILE *output, const BigUInt *value)
{
    char *text = NULL;
    if (big_uint_to_decimal(value, &text) != BIG_UINT_OK) {
        fputs("Brak pamieci przy konwersji wyniku na zapis dziesietny.\n", stderr);
        return false;
    }
    bool success = fputs(text, output) != EOF;
    free(text);
    return success;
}

/* Indeksy każdej części są już w kolejności nierosnących wartości,
 * ponieważ algorytm przydziela elementy w tej kolejności. */
static bool part_precedes(const PartitionPart *a, const PartitionPart *b,
                          const Dataset *dataset)
{
    size_t common = a->count < b->count ? a->count : b->count;
    for (size_t i = 0; i < common; ++i) {
        const BigUInt *left = &dataset->items[a->indices[i] - 1].value;
        const BigUInt *right = &dataset->items[b->indices[i] - 1].value;
        int order = big_uint_cmp(left, right);
        if (order != 0) {
            return order > 0;
        }
    }
    return a->count >= b->count;
}

static void sort_parts(const PartitionPart **parts, const PartitionPart **scratch,
                       size_t begin, size_t end, const Dataset *dataset)
{
    if (end - begin < 2) {
        return;
    }
    size_t middle = begin + (end - begin) / 2;
    sort_parts(parts, scratch, begin, middle, dataset);
    sort_parts(parts, scratch, middle, end, dataset);
    size_t left = begin;
    size_t right = middle;
    size_t pos = begin;
    while (left < middle && right < end) {
        if (part_precedes(parts[left], parts[right], dataset)) {
            scratch[pos++] = parts[left++];
        } else {
            scratch[pos++] = parts[right++];
        }
    }
    while (left < middle) {
        scratch[pos++] = parts[left++];
    }
    while (right < end) {
        scratch[pos++] = parts[right++];
    }
    memcpy(parts + begin, scratch + begin, (end - begin) * sizeof(*parts));
}

static bool write_values(FILE *output, const Dataset *dataset, const BigUInt *sum,
                         const size_t *indices, size_t count)
{
    if (!write_number(output, sum)) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        size_t position = indices == NULL ? i : indices[i] - 1;
        if (fputc(' ', output) == EOF ||
            !write_number(output, &dataset->items[position].value)) {
            return false;
        }
    }
    return fputc('\n', output) != EOF;
}

/* Dla n elementów, k części, L fragmentów wartości oraz S fragmentów sumy
 * całkowitej (L,S >= 1) dodawanie sumy kosztuje O(n*S),
 * sortowanie leksykograficzne O(n*L*log k), a konwersje dziesiętne
 * O(n*L^2 + k*S^2). Pamięć pomocnicza O(k + S + L) słów. */
static bool write_result(FILE *output, const Dataset *dataset,
                         const PartitionResult *result)
{
    BigUInt total = BIG_UINT_INIT;
    const PartitionPart **parts = NULL;
    const PartitionPart **scratch = NULL;
    bool success = false;
    for (size_t i = 0; i < dataset->count; ++i) {
        if (big_uint_add_assign(&total, &dataset->items[i].value) != BIG_UINT_OK) {
            goto memory_error;
        }
    }
    if (!write_values(output, dataset, &total, NULL, dataset->count)) {
        goto cleanup;
    }
    if (result == NULL) {
        success = fputs("BRAK\n", output) != EOF;
        goto cleanup;
    }
    if (result->count > SIZE_MAX / sizeof(*parts)) {
        goto memory_error;
    }
    parts = malloc(result->count * sizeof(*parts));
    scratch = malloc(result->count * sizeof(*scratch));
    if (parts == NULL || scratch == NULL) {
        goto memory_error;
    }
    for (size_t i = 0; i < result->count; ++i) {
        parts[i] = &result->parts[i];
    }
    sort_parts(parts, scratch, 0, result->count, dataset);
    for (size_t i = 0; i < result->count; ++i) {
        const PartitionPart *part = parts[i];
        if (!write_values(output, dataset, &part->sum, part->indices, part->count)) {
            goto cleanup;
        }
    }
    success = write_number(output, &result->spread) && fputc('\n', output) != EOF;
    goto cleanup;

memory_error:
    fputs("Brak pamieci przy przygotowywaniu wyniku.\n", stderr);
cleanup:
    free(scratch);
    free(parts);
    big_uint_free(&total);
    return success;
}

static bool process_dataset(FILE *output, char *line, size_t k, size_t dataset_index,
                            bool exact)
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
    PartitionStatus status = exact
        ? partition_exact(dataset.items, dataset.count, k, &result)
        : partition_descending(dataset.items, dataset.count, k, &result);
    if (status == PARTITION_IMPOSSIBLE) {
        if (exact) {
            success = write_result(output, &dataset, NULL);
            goto cleanup;
        }
        fprintf(stderr, "Zbior %zu: liczba elementow jest mniejsza niz k.\n",
                dataset_index);
        goto cleanup;
    }
    if (status != PARTITION_OK) {
        fprintf(stderr, "Zbior %zu: brak pamieci na obliczenie podzialu.\n",
                dataset_index);
        goto cleanup;
    }
    success = write_result(output, &dataset, &result);

cleanup:
    partition_result_free(&result);
    dataset_free(&dataset);
    return success;
}

static int process_input(FILE *input, FILE *output, size_t k, bool exact)
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
    if (fprintf(output, "%zu %zu\n", dataset_count, k) < 0) {
        goto cleanup;
    }
    for (size_t i = 0; i < dataset_count; ++i) {
        status = read_line(input, &line, &capacity);
        if (status != LINE_OK) {
            fprintf(stderr, "Nie mozna odczytac zbioru %zu.\n", i + 1);
            report_line_error(status);
            goto cleanup;
        }
        if (i > 0 && fputc('\n', output) == EOF) {
            goto cleanup;
        }
        if (!process_dataset(output, line, k, i + 1, exact)) {
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

static char *make_output_name(const char *input_name)
{
    const char suffix[] = "_output.txt";
    size_t length = strlen(input_name);
    if (length >= 4 && input_name[length - 4] == '.' &&
        tolower((unsigned char)input_name[length - 3]) == 't' &&
        tolower((unsigned char)input_name[length - 2]) == 'x' &&
        tolower((unsigned char)input_name[length - 1]) == 't') {
        length -= 4;
    }
    if (length > SIZE_MAX - sizeof(suffix)) {
        return NULL;
    }
    char *name = malloc(length + sizeof(suffix));
    if (name != NULL) {
        memcpy(name, input_name, length);
        memcpy(name + length, suffix, sizeof(suffix));
    }
    return name;
}

/* Utworzenie wyłączne chroni pliki innych uruchomień programu.
 * Dopisek ".tmp.<licznik>" jest usuwany po sukcesie lub błędzie. */
static FILE *create_temporary_output(const char *output_name, char **temporary_name)
{
    size_t length = strlen(output_name);
    size_t extra = sizeof(".tmp.") + 3 * sizeof(size_t);
    if (length > SIZE_MAX - extra) {
        fputs("Za dluga nazwa pliku wynikowego.\n", stderr);
        return NULL;
    }
    char *name = malloc(length + extra);
    if (name == NULL) {
        fputs("Brak pamieci na nazwe pliku tymczasowego.\n", stderr);
        return NULL;
    }
    for (size_t attempt = 0; ; ++attempt) {
        snprintf(name, length + extra, "%s.tmp.%zu", output_name, attempt);
        FILE *output = fopen(name, "wbx");
        if (output != NULL) {
            *temporary_name = name;
            return output;
        }
        if (errno != EEXIST || attempt == SIZE_MAX) {
            perror(name);
            free(name);
            return NULL;
        }
    }
}

static bool replace_output(const char *temporary_name, const char *output_name)
{
#ifdef _WIN32
    if (!MoveFileExA(temporary_name, output_name, MOVEFILE_REPLACE_EXISTING)) {
        fprintf(stderr, "Blad zapisywania pliku %s (kod Windows %lu).\n",
                output_name, (unsigned long)GetLastError());
        return false;
    }
#else
    if (rename(temporary_name, output_name) != 0) {
        perror(output_name);
        return false;
    }
#endif
    return true;
}

int main(int argc, char **argv)
{
    if (argc != 4 || (strcmp(argv[1], "A") != 0 && strcmp(argv[1], "D") != 0)) {
        fprintf(stderr, "Uzycie: %s A|D plik_wejsciowy k\n", argv[0]);
        return EXIT_FAILURE;
    }
    size_t k;
    if (!parse_size(argv[3], &k) || k < 2) {
        fputs("k musi byc liczba calkowita >= 2 mieszczaca sie w size_t.\n", stderr);
        return EXIT_FAILURE;
    }
    FILE *input = fopen(argv[2], "rb");
    if (input == NULL) {
        perror(argv[2]);
        return EXIT_FAILURE;
    }
    char *output_name = make_output_name(argv[2]);
    if (output_name == NULL) {
        fputs("Brak pamieci na nazwe pliku wynikowego.\n", stderr);
        fclose(input);
        return EXIT_FAILURE;
    }
    char *temporary_name = NULL;
    FILE *output = create_temporary_output(output_name, &temporary_name);
    if (output == NULL) {
        free(output_name);
        fclose(input);
        return EXIT_FAILURE;
    }
    int exit_code = process_input(input, output, k, strcmp(argv[1], "D") == 0);
    if (fclose(input) != 0) {
        fputs("Blad zamykania pliku wejsciowego.\n", stderr);
        exit_code = EXIT_FAILURE;
    }
    if (ferror(output)) {
        fputs("Blad zapisu pliku wynikowego.\n", stderr);
        exit_code = EXIT_FAILURE;
    }
    if (fclose(output) != 0) {
        perror(temporary_name);
        exit_code = EXIT_FAILURE;
    }
    if (exit_code == EXIT_SUCCESS && !replace_output(temporary_name, output_name)) {
        exit_code = EXIT_FAILURE;
    }
    if (exit_code != EXIT_SUCCESS && remove(temporary_name) != 0) {
        perror(temporary_name);
    }
    free(temporary_name);
    free(output_name);
    return exit_code;
}
