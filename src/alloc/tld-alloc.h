#ifndef TLD_ALLOC_H
#define TLD_ALLOC_H

#include "../core/tld-core.h"
#include <string.h>
#include <stdlib.h>

/* MFREE: free(NULL) is legal, so a NULL pointer is not reported. The pointer is set
 * to NULL afterwards either way. */
#define MFREE(p)                                                        \
        do {                                                            \
                free(p);                                                \
                (p) = NULL;                                             \
        } while (0)

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

#define tld_valid_1D(x)                                                 \
        _Generic((x), char ** : 1, int8_t ** : 1, uint8_t ** : 1, int16_t ** : 1, \
                 uint16_t ** : 1, int32_t ** : 1, uint32_t ** : 1, int64_t ** : 1, \
                 uint64_t ** : 1, float ** : 1, double ** : 1, default : 0)

#define tld_valid_2D(x)                                                 \
        _Generic((x), char *** : 1, int8_t *** : 1, uint8_t *** : 1,    \
                 int16_t *** : 1, uint16_t *** : 1, int32_t *** : 1,    \
                 uint32_t *** : 1, int64_t *** : 1, uint64_t *** : 1, float *** : 1, \
                 double *** : 1, default : 0)

#define XARG3(_0, _1, _2, _3, ...) _3
#define NARG3(...) XARG3(__VA_ARGS__, 3, 2, 1, 0)

#define XGALLOC_ARGS_1(a) galloc_too_few()
#define XGALLOC_ARGS_2(a, b)                                            \
        do {                                                            \
                if (!tld_valid_1D((a))) {                               \
                        ERROR_MSG("Wrong pointer type for 1D alloc");   \
                }                                                       \
                *(a) = galloc_hlp(sizeof(__typeof__(**(a))), (b), 0);   \
                if (*(a) == NULL) {                                     \
                        ERROR_MSG("Could not alloc 1D");                \
                }                                                       \
        } while (0)

/* 2D: galloc_hlp returns the array of row pointers; the data block hangs off its
 * header. Row offsets are computed in size_t. The local names are chosen so they
 * cannot capture an identifier used in the caller's (b) or (c). */
#define XGALLOC_ARGS_3(a, b, c)                                         \
        do {                                                            \
                if (!tld_valid_2D((a))) {                               \
                        ERROR_MSG("Wrong pointer type for 2D alloc");   \
                }                                                       \
                const size_t tld_galloc_n_ = (size_t)(b);               \
                const size_t tld_galloc_m_ = (size_t)(c);               \
                __typeof__(*(a)) tld_galloc_rows_ =                     \
                        galloc_hlp(sizeof(__typeof__(**(a))), (b), (c)); \
                void *tld_galloc_data_ = NULL;                          \
                if (tld_galloc_rows_ == NULL) {                         \
                        ERROR_MSG("Could not alloc 2D");                \
                }                                                       \
                if (get_ptr(tld_galloc_rows_, &tld_galloc_data_) != OK || \
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

#define gfree(a) tld_free(a)

#define XXGALLOC_ARGS(N, ...) XGALLOC_ARGS_##N(__VA_ARGS__)
#define XGALLOC_ARGS(N, ...) XXGALLOC_ARGS(N, __VA_ARGS__)
#define GALLOC_ARGS(...) XGALLOC_ARGS(NARG3(__VA_ARGS__), __VA_ARGS__)
#define galloc(...) GALLOC_ARGS(__VA_ARGS__)

tld_external int galloc_too_few(void);
tld_external void *galloc_hlp(size_t tsize, int dim1, int dim2);
tld_external void tld_free(void *p);

tld_external int get_dim1(void *ptr, uint32_t *d);
tld_external int get_dim2(void *ptr, uint32_t *d);
tld_external int get_ptr(void *ptr, void **d);

#endif
