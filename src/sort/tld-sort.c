#include "tld-sort.h"

#define TLD__SORT_DESC_FUNC(type)                                             \
        static int cmp_desc_##type (const void *a, const void *b);  \
                                                                    \
        int sort_desc_##type(type *a, int len)                      \
        {                                                           \
                qsort(a, len, sizeof(type), cmp_desc_ ##type);      \
                return OK;                                          \
        }                                                           \
                                                                    \
        int cmp_desc_##type (const void *a, const void *b)          \
        {                                                           \
                const type  *ia = (const type *)a;                  \
                const type  *ib = (const type *)b;                  \
                return (*ia < *ib) - (*ia > *ib);                   \
        }                                                           \
                                                                    \
        static int cmp_asc_##type (const void *a, const void *b);   \
                                                                    \
        int sort_asc_##type(type *a, int len)                       \
        {                                                           \
                qsort(a, len, sizeof(type), cmp_asc_ ##type);       \
                return OK;                                          \
        }                                                           \
                                                                    \
        int cmp_asc_##type (const void *a, const void *b)           \
        {                                                           \
                const type  *ia = (const type *)a;                  \
                const type  *ib = (const type *)b;                  \
                return (*ia > *ib) - (*ia < *ib);                   \
        }

TLD__SORT_DESC_FUNC(char)
TLD__SORT_DESC_FUNC(int8_t)
TLD__SORT_DESC_FUNC(uint8_t)
TLD__SORT_DESC_FUNC(int16_t)
TLD__SORT_DESC_FUNC(uint16_t)
TLD__SORT_DESC_FUNC(int32_t)
TLD__SORT_DESC_FUNC(uint32_t)
TLD__SORT_DESC_FUNC(int64_t)
TLD__SORT_DESC_FUNC(uint64_t)
TLD__SORT_DESC_FUNC(float)
TLD__SORT_DESC_FUNC(double)

#undef TLD__SORT_DESC_FUNC
