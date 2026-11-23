/* shuffle: uniform random permutation (Fisher-Yates) in place of arrays of char,
 * int8_t..int64_t, uint8_t..uint64_t, float and double; TLD_SHUFFLE picks the function
 * from the array's type. */
#ifndef TLD_SHUFFLE_H
#define TLD_SHUFFLE_H

#include "../rng/tld-rng.h"

/* TLD__SHUFFLE_HEADER(type) declares tld_shuffle_<type>. */
#define TLD__SHUFFLE_HEADER(type)                        \
        int tld_shuffle_##type(type *a, int len, struct tld_rng* rng);

/* Shuffle len elements of a in place, drawing from rng (see tld_rng_alloc). Always OK. */
TLD__SHUFFLE_HEADER(char)
        TLD__SHUFFLE_HEADER(int8_t)
        TLD__SHUFFLE_HEADER(uint8_t)
        TLD__SHUFFLE_HEADER(int16_t)
        TLD__SHUFFLE_HEADER(uint16_t)
        TLD__SHUFFLE_HEADER(int32_t)
        TLD__SHUFFLE_HEADER(uint32_t)
        TLD__SHUFFLE_HEADER(int64_t)
        TLD__SHUFFLE_HEADER(uint64_t)
        TLD__SHUFFLE_HEADER(float)
        TLD__SHUFFLE_HEADER(double)

/* TLD_SHUFFLE(a, len, rng): tld_shuffle_<type>(a, len, rng) for a's element type. */
#define TLD_SHUFFLE(A,N,R) _Generic((A),                                 \
                                    char* : tld_shuffle_char,             \
                                    int8_t* : tld_shuffle_int8_t,         \
                                    uint8_t* : tld_shuffle_uint8_t,       \
                                    int16_t* : tld_shuffle_int16_t,       \
                                    uint16_t* : tld_shuffle_uint16_t,     \
                                    int32_t *: tld_shuffle_int32_t,       \
                                    uint32_t* : tld_shuffle_uint32_t,     \
                                    int64_t* : tld_shuffle_int64_t,       \
                                    uint64_t*: tld_shuffle_uint64_t,      \
                                    float* : tld_shuffle_float,           \
                                    double* : tld_shuffle_double)(A,N,R)

#undef TLD__SHUFFLE_HEADER

#endif
