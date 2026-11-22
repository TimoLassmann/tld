/* alloc: checked allocation macros (MMALLOC, MREALLOC, MFREE) and galloc/gfree, which
 * allocate zeroed, typed 1D arrays and 2D matrices that remember their dimensions. All
 * allocation macros goto ERROR on failure. */
#ifndef TLD_ALLOC_H
#define TLD_ALLOC_H

#include "../core/tld-core.h"
#include <string.h>
#include <stdlib.h>

/* MFREE(p): free(p) and set p to NULL. For memory from MMALLOC, MREALLOC or malloc (use
 * gfree for galloc memory). p must be an lvalue; a NULL p is fine. */
#define MFREE(p)                                                        \
        do {                                                            \
                free(p);                                                \
                (p) = NULL;                                             \
        } while (0)

/* MMALLOC(p, size): p = malloc(size), zero-filled. p must be NULL beforehand and size
 * non-zero; either mistake, or running out of memory, prints an error and does goto
 * ERROR. Free with MFREE. */
#define MMALLOC(p, size)                                                \
        do {                                                            \
                if ((p) != NULL) {                                      \
                        ERROR_MSG("malloc on a nun-null pointer");      \
                }                                                       \
                if ((size) == 0) {                                      \
                        ERROR_MSG("malloc of size %zu failed", (size_t)(size)); \
                }                                                       \
                if (((p) = malloc(size)) == NULL) {                     \
                        ERROR_MSG("malloc of size %zu failed", (size_t)(size)); \
                }                                                       \
                memset(p, 0, size);                                     \
        } while (0)

/* MMALLOC_ALIGNED: like MMALLOC, for a struct whose declaration asks for more alignment
 * than malloc guarantees (16 bytes on 64-bit), e.g. __attribute__((aligned(64))). Using
 * plain malloc for such a struct breaks the promise the declaration makes to the compiler
 * (undefined behaviour; UBSan reports "misaligned address ... requires 64 byte alignment").
 * Pass the struct's own alignment: MMALLOC_ALIGNED(p, sizeof(struct s), _Alignof(struct s)).
 * C11 aligned_alloc requires the size to be a multiple of the alignment, so the size is
 * rounded up. The block is freed with plain free(), so MFREE works unchanged. */
#define MMALLOC_ALIGNED(p, size, align)                                 \
        do {                                                            \
                if ((p) != NULL) {                                      \
                        ERROR_MSG("malloc on a nun-null pointer");      \
                }                                                       \
                if ((size) == 0) {                                      \
                        ERROR_MSG("malloc of size %zu failed", (size_t)(size)); \
                }                                                       \
                if (((p) = aligned_alloc((align),                       \
                                         (((size) + (align) - 1) / (align)) * (align))) == NULL) { \
                        ERROR_MSG("aligned malloc of size %zu (alignment %zu) failed", \
                                  (size_t)(size), (size_t)(align));     \
                }                                                       \
                memset(p, 0, size);                                     \
        } while (0)

/* MREALLOC(p, size): resize p to size bytes (malloc when p is NULL). Bytes beyond the old
 * size are not zeroed. On failure p is unchanged, an error is printed and goto ERROR;
 * size 0 is an error. */
#define MREALLOC(p, size)                                               \
        do {                                                            \
                void *tmpp;                                             \
                if ((size) == 0) {                                      \
                        ERROR_MSG("malloc of size %zu failed", (size_t)(size)); \
                }                                                       \
                if ((p) == NULL) {                                      \
                        tmpp = malloc(size);                            \
                } else {                                                \
                        tmpp = realloc((p), (size));                    \
                }                                                       \
                if (tmpp != NULL) {                                     \
                        (p) = tmpp;                                     \
                } else {                                                \
                        ERROR_MSG("realloc for size %zu failed", (size_t)(size)); \
                }                                                       \
        } while (0)

/* tld_valid_1D(x) is 1 if x is a T ** and tld_valid_2D(x) is 1 if x is a T ***, where T
 * is a type galloc supports (char, int8_t..int64_t, uint8_t..uint64_t, float, double);
 * 0 otherwise. Compile-time checks used by galloc. */
#define tld_valid_1D(x)                                                 \
        _Generic((x), char ** : 1, int8_t ** : 1, uint8_t ** : 1, int16_t ** : 1, \
                 uint16_t ** : 1, int32_t ** : 1, uint32_t ** : 1, int64_t ** : 1, \
                 uint64_t ** : 1, float ** : 1, double ** : 1, default : 0)
#define tld_valid_2D(x)                                                 \
        _Generic((x), char *** : 1, int8_t *** : 1, uint8_t *** : 1,    \
                 int16_t *** : 1, uint16_t *** : 1, int32_t *** : 1,    \
                 uint32_t *** : 1, int64_t *** : 1, uint64_t *** : 1, float *** : 1, \
                 double *** : 1, default : 0)

/* Argument counting for galloc: NARG3(...) is the number of its 1 to 3 arguments. */
#define XARG3(_0, _1, _2, _3, ...) _3
#define NARG3(...) XARG3(__VA_ARGS__, 3, 2, 1, 0)

/* galloc with 1 argument (an error: too few) and with 2 (a 1D array). Use galloc. */
#define XGALLOC_ARGS_1(a) tld__galloc_too_few()
#define XGALLOC_ARGS_2(a, b)                                            \
        do {                                                            \
                if (!tld_valid_1D((a))) {                               \
                        ERROR_MSG("Wrong pointer type for 1D alloc");   \
                }                                                       \
                *(a) = tld__galloc(sizeof(__typeof__(**(a))),           \
                                   sizeof(__typeof__(**(a))), (b), 0);  \
                if (*(a) == NULL) {                                     \
                        ERROR_MSG("Could not alloc 1D");                \
                }                                                       \
        } while (0)

/* galloc with 3 arguments (a 2D matrix). Use galloc. tld__galloc returns the array of
 * row pointers (sizeof(T *) each); the data block (sizeof(T) per element) hangs off its
 * header. Row offsets are computed in size_t.
 * The local names are chosen so they cannot capture an identifier used in the caller's
 * (b) or (c). */
#define XGALLOC_ARGS_3(a, b, c)                                         \
        do {                                                            \
                if (!tld_valid_2D((a))) {                               \
                        ERROR_MSG("Wrong pointer type for 2D alloc");   \
                }                                                       \
                const size_t tld_galloc_n_ = (size_t)(b);               \
                const size_t tld_galloc_m_ = (size_t)(c);               \
                __typeof__(*(a)) tld_galloc_rows_ =                     \
                        tld__galloc(sizeof(__typeof__(**(a))),          \
                                    sizeof(__typeof__(***(a))), (b), (c)); \
                void *tld_galloc_data_ = NULL;                          \
                if (tld_galloc_rows_ == NULL) {                         \
                        ERROR_MSG("Could not alloc 2D");                \
                }                                                       \
                if (tld__galloc_data(tld_galloc_rows_, &tld_galloc_data_) != OK || \
                    tld_galloc_data_ == NULL) {                         \
                        tld_free(tld_galloc_rows_);                     \
                        ERROR_MSG("Could not alloc 2D");                \
                }                                                       \
                tld_galloc_rows_[0] = (__typeof__(**(a)))tld_galloc_data_; \
                for (size_t tld_galloc_i_ = 1; tld_galloc_i_ < tld_galloc_n_; tld_galloc_i_++) { \
                        tld_galloc_rows_[tld_galloc_i_] =               \
                                tld_galloc_rows_[0] + tld_galloc_m_ * tld_galloc_i_; \
                }                                                       \
                *(a) = tld_galloc_rows_;                                \
        } while (0)

/* gfree(p): free memory from galloc (1D or 2D). A NULL p is fine; p is not set to
 * NULL. Never pass galloc memory to free() or MFREE. */
#define gfree(a) tld_free(a)

/* Dispatch from galloc to XGALLOC_ARGS_<number of arguments>. Use galloc. */
#define XXGALLOC_ARGS(N, ...) XGALLOC_ARGS_##N(__VA_ARGS__)
#define XGALLOC_ARGS(N, ...) XXGALLOC_ARGS(N, __VA_ARGS__)
#define GALLOC_ARGS(...) XGALLOC_ARGS(NARG3(__VA_ARGS__), __VA_ARGS__)

/* galloc(&a, n): a = zeroed array of n elements of a's element type.
 * galloc(&m, n, k): m = zeroed n x k matrix, used as m[i][j]; the rows share one
 * contiguous block. Element types: char, int8_t..int64_t, uint8_t..uint64_t, float,
 * double. The pointer should be NULL (an existing block is not freed or resized). On
 * failure an error is printed and goto ERROR. Free with gfree; get_dim1/get_dim2 give
 * the dimensions back. */
#define galloc(...) GALLOC_ARGS(__VA_ARGS__)

/* Internal: called by galloc with a single argument; prints an error, returns FAIL. */
tld_external int tld__galloc_too_few(void);

/* Internal back end of galloc: a zeroed block of dim1 entries of rsize bytes behind a
 * hidden header that records dim1 and dim2; with dim2 > 0 also a zeroed dim1 x dim2
 * data block of tsize-byte elements. NULL on failure (dim1 < 1, dim2 < 0, overflow, no
 * memory). */
tld_external void *tld__galloc(size_t rsize, size_t tsize, int dim1, int dim2);

/* The function behind gfree: frees a galloc block and its 2D data block. NULL is
 * ignored. */
tld_external void tld_free(void *p);

/* Dimensions of a galloc block: rows (n) into *d, and columns (k, 0 for a 1D array)
 * into *d. FAIL if ptr is NULL. Only valid for memory from galloc. */
tld_external int get_dim1(void *ptr, uint32_t *d);
tld_external int get_dim2(void *ptr, uint32_t *d);

/* Internal: the data block of a 2D galloc block into *d (NULL for a 1D array); FAIL
 * if ptr is NULL. Used by galloc. */
tld_external int tld__galloc_data(void *ptr, void **d);

#endif
