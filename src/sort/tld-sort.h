/* sort: in-place sorting of arrays of char, int8_t..int64_t, uint8_t..uint64_t, float
 * and double, ascending or descending; TLD_SORT_ASC and TLD_SORT_DESC pick the function
 * from the array's type. */
#ifndef TLD_SORT_H
#define TLD_SORT_H

#include "../core/tld-core.h"

/* TLD__SORT_HEADER(type) declares sort_desc_<type> and sort_asc_<type>. */
#define TLD__SORT_HEADER(type)                       \
        int sort_desc_##type(type *a, int len); \
        int sort_asc_##type (type* a, int len);

/* Sort len elements of a in place, descending (sort_desc_*) or ascending (sort_asc_*),
 * with qsort (not stable). Always OK. Arrays holding NaN have no defined order. */
TLD__SORT_HEADER(char)
        TLD__SORT_HEADER(int8_t)
        TLD__SORT_HEADER(uint8_t)
        TLD__SORT_HEADER(int16_t)
        TLD__SORT_HEADER(uint16_t)
        TLD__SORT_HEADER(int32_t)
        TLD__SORT_HEADER(uint32_t)
        TLD__SORT_HEADER(int64_t)
        TLD__SORT_HEADER(uint64_t)
        TLD__SORT_HEADER(float)
        TLD__SORT_HEADER(double)

/* TLD_SORT_DESC(a, len): sort_desc_<type>(a, len) for a's element type. */
#define TLD_SORT_DESC(A,N) _Generic((A),                                \
                                    char* : sort_desc_char,             \
                                    int8_t* : sort_desc_int8_t,         \
                                    uint8_t* : sort_desc_uint8_t,       \
                                    int16_t* : sort_desc_int16_t,       \
                                    uint16_t* : sort_desc_uint16_t,     \
                                    int32_t *: sort_desc_int32_t,       \
                                    uint32_t* : sort_desc_uint32_t,     \
                                    int64_t* : sort_desc_int64_t,       \
                                    uint64_t*: sort_desc_uint64_t,      \
                                    float* : sort_desc_float,           \
                                    double* : sort_desc_double)(A,N)

/* TLD_SORT_ASC(a, len): sort_asc_<type>(a, len) for a's element type. */
#define TLD_SORT_ASC(A,N) _Generic((A),                             \
                                   char* : sort_asc_char,           \
                                   int8_t* : sort_asc_int8_t,       \
                                   uint8_t* : sort_asc_uint8_t,     \
                                   int16_t* : sort_asc_int16_t,     \
                                   uint16_t* : sort_asc_uint16_t,   \
                                   int32_t *: sort_asc_int32_t,     \
                                   uint32_t* : sort_asc_uint32_t,   \
                                   int64_t* : sort_asc_int64_t,     \
                                   uint64_t*: sort_asc_uint64_t,    \
                                   float* : sort_asc_float,         \
                                   double* : sort_asc_double)(A,N)

#undef TLD__SORT_HEADER

#endif
