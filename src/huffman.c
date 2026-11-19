/*
 * huffman.c
 *
 * Huffman coding compressor / decompressor for my COMP 211 checkoff.
 *
 *   ./huffman stats <file>             show frequencies, codes and sizes
 *   ./huffman compress <in> <out>      compress a file
 *   ./huffman decompress <in> <out>    decompress a file
 *
 * Compressed file format (integers are little-endian):
 *   4 bytes    "HUF1"
 *   8 bytes    size of the original file in bytes
 *   2 bytes    number of distinct symbols (n)
 *   n * 5      symbol (1 byte) + its count (4 bytes)
 *   the rest   encoded bits, 8 per byte, last byte padded with 0s
 *
 * The decompressor rebuilds the same tree from the counts, so the tree
 * itself is never written to the file. Because the original size is in the
 * header, the decompressor knows when to stop and never decodes the padding.
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_SYMBOLS 256   /* a byte can be 0..255 */
#define MAX_CODE 256      /* longest possible code is 255 bits, +1 for '\0' */

typedef struct Node {
    int symbol;          /* 0..255 for a leaf, -1 for an internal node */
    uint32_t freq;
    int order;           /* tie breaker so compress and decompress build the same tree */
    struct Node *left;   /* 0 branch */
    struct Node *right;  /* 1 branch */
} Node;

/* ---------------------------------------------------------------------- */
/* tree nodes                                                             */
/* ---------------------------------------------------------------------- */

static Node *new_node(int symbol, uint32_t freq, int order, Node *left, Node *right)
{
    Node *n = malloc(sizeof(Node));
    if (n == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    n->symbol = symbol;
    n->freq = freq;
    n->order = order;
    n->left = left;
    n->right = right;
    return n;
}

/* children first, then the parent (post-order) */
static void free_tree(Node *n)
{
    if (n == NULL)
        return;
    free_tree(n->left);
    free_tree(n->right);
    free(n);
}

/* ---------------------------------------------------------------------- */
/* min-heap of Node pointers (smallest freq on top)                       */
/* ---------------------------------------------------------------------- */

typedef struct {
    Node *items[NUM_SYMBOLS];
    int size;
} MinHeap;

static int less(const Node *a, const Node *b)
{
    if (a->freq != b->freq)
        return a->freq < b->freq;
    return a->order < b->order;
}

static void swap(Node **a, Node **b)
{
    Node *tmp = *a;
    *a = *b;
    *b = tmp;
}

static void heap_push(MinHeap *h, Node *n)
{
    int i = h->size++;
    h->items[i] = n;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (!less(h->items[i], h->items[parent]))
            break;
        swap(&h->items[i], &h->items[parent]);
        i = parent;
    }
}

static Node *heap_pop(MinHeap *h)
{
    Node *top = h->items[0];
    h->items[0] = h->items[--h->size];

    int i = 0;
    while (1) {
        int l = 2 * i + 1;
        int r = 2 * i + 2;
        int smallest = i;
        if (l < h->size && less(h->items[l], h->items[smallest]))
            smallest = l;
        if (r < h->size && less(h->items[r], h->items[smallest]))
            smallest = r;
        if (smallest == i)
            break;
        swap(&h->items[i], &h->items[smallest]);
        i = smallest;
    }
    return top;
}

/* ---------------------------------------------------------------------- */
/* building the tree and the code table                                   */
/* ---------------------------------------------------------------------- */

static Node *build_tree(const uint32_t freq[NUM_SYMBOLS])
{
    MinHeap heap = { .size = 0 };

    for (int s = 0; s < NUM_SYMBOLS; s++) {
        if (freq[s] > 0)
            heap_push(&heap, new_node(s, freq[s], s, NULL, NULL));
    }

    if (heap.size == 0)          /* empty file */
        return NULL;

    if (heap.size == 1) {
        /* Only one kind of symbol (like "AAAA"). A lone leaf would have an
         * empty code, so hang it off a parent and give it the code "0". */
        Node *only = heap_pop(&heap);
        return new_node(-1, only->freq, NUM_SYMBOLS, only, NULL);
    }

    /* the actual Huffman algorithm: merge the two smallest until one is left */
    int next_order = NUM_SYMBOLS;
    while (heap.size > 1) {
        Node *a = heap_pop(&heap);
        Node *b = heap_pop(&heap);
        heap_push(&heap, new_node(-1, a->freq + b->freq, next_order++, a, b));
    }
    return heap_pop(&heap);
}

/* walk the tree; going left adds '0', going right adds '1' */
static void build_codes(const Node *n, char *path, int depth, char codes[][MAX_CODE])
{
    if (n == NULL)
        return;
    if (n->symbol >= 0) {
        path[depth] = '\0';
        strcpy(codes[n->symbol], path);
        return;
    }
    path[depth] = '0';
    build_codes(n->left, path, depth + 1, codes);
    path[depth] = '1';
    build_codes(n->right, path, depth + 1, codes);
}

/* ---------------------------------------------------------------------- */
/* file helpers                                                           */
/* ---------------------------------------------------------------------- */

static unsigned char *read_file(const char *path, size_t *size_out)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        perror(path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char *data = malloc(size > 0 ? size : 1);
    if (data == NULL || fread(data, 1, size, f) != (size_t)size) {
        fprintf(stderr, "could not read %s\n", path);
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *size_out = (size_t)size;
    return data;
}

static void count_freqs(const unsigned char *data, size_t size, uint32_t freq[NUM_SYMBOLS])
{
    memset(freq, 0, NUM_SYMBOLS * sizeof(uint32_t));
    for (size_t i = 0; i < size; i++)
        freq[data[i]]++;
}

/* write the low nbytes of value, lowest byte first */
static void write_le(FILE *f, uint64_t value, int nbytes)
{
    for (int i = 0; i < nbytes; i++)
        fputc((value >> (8 * i)) & 0xFF, f);
}

static int read_le(FILE *f, int nbytes, uint64_t *out)
{
    uint64_t value = 0;
    for (int i = 0; i < nbytes; i++) {
        int c = fgetc(f);
        if (c == EOF)
            return 0;
        value |= (uint64_t)c << (8 * i);
    }
    *out = value;
    return 1;
}

/* ---------------------------------------------------------------------- */
/* writing and reading single bits                                        */
/* ---------------------------------------------------------------------- */

typedef struct {
    FILE *f;
    unsigned char byte;  /* bits collected so far */
    int count;           /* how many bits are in byte */
} BitWriter;

static void put_bit(BitWriter *w, int bit)
{
    w->byte = (w->byte << 1) | (bit & 1);
    w->count++;
    if (w->count == 8) {
        fputc(w->byte, w->f);
        w->byte = 0;
        w->count = 0;
    }
}

/* pad the last partial byte with zeros on the right */
static void flush_bits(BitWriter *w)
{
    if (w->count > 0) {
        w->byte <<= (8 - w->count);
        fputc(w->byte, w->f);
        w->byte = 0;
        w->count = 0;
    }
}

typedef struct {
    FILE *f;
    int byte;
    int left;  /* bits not yet used in byte */
} BitReader;

/* returns 0 or 1, or -1 if the file ran out */
static int get_bit(BitReader *r)
{
    if (r->left == 0) {
        r->byte = fgetc(r->f);
        if (r->byte == EOF)
            return -1;
        r->left = 8;
    }
    r->left--;
    return (r->byte >> r->left) & 1;
}

/* ---------------------------------------------------------------------- */
/* commands                                                               */
/* ---------------------------------------------------------------------- */

static int compress_file(const char *in_path, const char *out_path)
{
    size_t size;
    unsigned char *data = read_file(in_path, &size);
    if (data == NULL)
        return 1;

    uint32_t freq[NUM_SYMBOLS];
    count_freqs(data, size, freq);

    Node *root = build_tree(freq);
    static char codes[NUM_SYMBOLS][MAX_CODE];
    char path[MAX_CODE];
    memset(codes, 0, sizeof(codes));
    build_codes(root, path, 0, codes);

    FILE *out = fopen(out_path, "wb");
    if (out == NULL) {
        perror(out_path);
        free_tree(root);
        free(data);
        return 1;
    }

    /* header */
    fwrite("HUF1", 1, 4, out);
    write_le(out, size, 8);
    int distinct = 0;
    for (int s = 0; s < NUM_SYMBOLS; s++)
        if (freq[s] > 0)
            distinct++;
    write_le(out, distinct, 2);
    for (int s = 0; s < NUM_SYMBOLS; s++) {
        if (freq[s] > 0) {
            fputc(s, out);
            write_le(out, freq[s], 4);
        }
    }

    /* body: swap every byte for its code */
    BitWriter w = { out, 0, 0 };
    for (size_t i = 0; i < size; i++) {
        for (const char *c = codes[data[i]]; *c != '\0'; c++)
            put_bit(&w, *c == '1');
    }
    flush_bits(&w);

    long out_size = ftell(out);
    fclose(out);

    printf("%s: %zu bytes -> %ld bytes", in_path, size, out_size);
    if (size > 0)
        printf(" (%.1f%% of original)", 100.0 * out_size / size);
    printf("\n");

    free_tree(root);
    free(data);
    return 0;
}

static int decompress_file(const char *in_path, const char *out_path)
{
    FILE *in = fopen(in_path, "rb");
    if (in == NULL) {
        perror(in_path);
        return 1;
    }

    char magic[4];
    uint64_t size, distinct;
    if (fread(magic, 1, 4, in) != 4 || memcmp(magic, "HUF1", 4) != 0 ||
        !read_le(in, 8, &size) || !read_le(in, 2, &distinct) || distinct > NUM_SYMBOLS) {
        fprintf(stderr, "%s is not a file made by ./huffman compress\n", in_path);
        fclose(in);
        return 1;
    }

    uint32_t freq[NUM_SYMBOLS] = { 0 };
    for (uint64_t i = 0; i < distinct; i++) {
        int s = fgetc(in);
        uint64_t count;
        if (s == EOF || !read_le(in, 4, &count)) {
            fprintf(stderr, "header is cut off\n");
            fclose(in);
            return 1;
        }
        freq[s] = (uint32_t)count;
    }

    Node *root = build_tree(freq);   /* same counts -> same tree */

    FILE *out = fopen(out_path, "wb");
    if (out == NULL) {
        perror(out_path);
        free_tree(root);
        fclose(in);
        return 1;
    }

    /* start at the root, follow bits until a leaf, print it, go back to the root */
    BitReader r = { in, 0, 0 };
    int status = 0;
    for (uint64_t i = 0; i < size && status == 0; i++) {
        const Node *n = root;
        while (n->symbol < 0) {
            int bit = get_bit(&r);
            if (bit < 0) {
                fprintf(stderr, "ran out of bits, file is truncated\n");
                status = 1;
                break;
            }
            n = bit ? n->right : n->left;
            if (n == NULL) {
                fprintf(stderr, "bad bit pattern, file is corrupted\n");
                status = 1;
                break;
            }
        }
        if (status == 0)
            fputc(n->symbol, out);
    }

    fclose(out);
    fclose(in);
    free_tree(root);
    return status;
}

/* for sorting the stats table by count, biggest first */
static const uint32_t *sort_freq;

static int by_count(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    if (sort_freq[x] != sort_freq[y])
        return sort_freq[x] < sort_freq[y] ? 1 : -1;
    return x - y;
}

static void print_symbol(int s)
{
    if (s == '\n')
        printf("'\\n'  ");
    else if (s == '\t')
        printf("'\\t'  ");
    else if (s == '\r')
        printf("'\\r'  ");
    else if (s >= 32 && s < 127)
        printf("'%c'   ", s);
    else
        printf("0x%02X  ", s);
}

static int stats_file(const char *path)
{
    size_t size;
    unsigned char *data = read_file(path, &size);
    if (data == NULL)
        return 1;
    if (size == 0) {
        printf("%s is empty\n", path);
        free(data);
        return 0;
    }

    uint32_t freq[NUM_SYMBOLS];
    count_freqs(data, size, freq);

    Node *root = build_tree(freq);
    static char codes[NUM_SYMBOLS][MAX_CODE];
    char path_buf[MAX_CODE];
    memset(codes, 0, sizeof(codes));
    build_codes(root, path_buf, 0, codes);

    int symbols[NUM_SYMBOLS];
    int distinct = 0;
    for (int s = 0; s < NUM_SYMBOLS; s++)
        if (freq[s] > 0)
            symbols[distinct++] = s;
    sort_freq = freq;
    qsort(symbols, distinct, sizeof(int), by_count);

    uint64_t huff_bits = 0;
    double entropy = 0.0;
    int show = distinct <= 40 ? distinct : 20;

    printf("symbol  count     code\n");
    for (int i = 0; i < distinct; i++) {
        int s = symbols[i];
        huff_bits += (uint64_t)freq[s] * strlen(codes[s]);
        double p = (double)freq[s] / size;
        entropy -= p * log2(p);
        if (i < show) {
            print_symbol(s);
            printf("%-9u %s\n", freq[s], codes[s]);
        }
    }
    if (show < distinct)
        printf("... (%d more symbols)\n", distinct - show);

    int fixed = 0;   /* smallest fixed-length code that can tell them apart */
    while ((1 << fixed) < distinct)
        fixed++;
    if (fixed == 0)
        fixed = 1;

    uint64_t header = 14 + 5 * (uint64_t)distinct;
    uint64_t body = (huff_bits + 7) / 8;

    printf("\n");
    printf("bytes in file:            %zu (%d distinct)\n", size, distinct);
    printf("as 8-bit bytes:           %llu bits\n", (unsigned long long)size * 8);
    printf("best fixed-length code:   %llu bits (%d bits each)\n",
           (unsigned long long)size * fixed, fixed);
    printf("huffman encoded:          %llu bits (%.3f bits each on average)\n",
           (unsigned long long)huff_bits, (double)huff_bits / size);
    printf("entropy (lower limit):    %.1f bits (%.3f bits each)\n", entropy * size, entropy);
    printf("compressed file would be: %llu bytes header + %llu bytes data = %llu bytes\n",
           (unsigned long long)header, (unsigned long long)body,
           (unsigned long long)(header + body));

    if (size <= 64) {
        printf("\nencoded bits: ");
        for (size_t i = 0; i < size; i++)
            printf("%s", codes[data[i]]);
        printf("\n");
    }

    free_tree(root);
    free(data);
    return 0;
}

static void usage(void)
{
    fprintf(stderr,
            "usage:\n"
            "  ./huffman stats <file>\n"
            "  ./huffman compress <in> <out>\n"
            "  ./huffman decompress <in> <out>\n");
}

int main(int argc, char *argv[])
{
    if (argc == 3 && strcmp(argv[1], "stats") == 0)
        return stats_file(argv[2]);
    if (argc == 4 && strcmp(argv[1], "compress") == 0)
        return compress_file(argv[2], argv[3]);
    if (argc == 4 && strcmp(argv[1], "decompress") == 0)
        return decompress_file(argv[2], argv[3]);
    usage();
    return 1;
}
