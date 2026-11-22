#include "tld-shuffle.h"
#include "../rng/tld-rng.h"

#define TLD__SHUFFLE_FUNC(type)                                              \
        int tld_shuffle_##type(type* arr,int n, struct tld_rng* rng)  \
        {                                                               \
                int i,j;                                                \
                type tmp;                                               \
                for (i = 0; i < n - 1; i++) {                           \
                        j = i + tld_rng_int(rng, n - i);              \
                        tmp = arr[j];                                   \
                        arr[j] = arr[i];                                \
                        arr[i] = tmp;                                   \
                }                                                       \
                return OK;                                              \
        }

TLD__SHUFFLE_FUNC(char)
TLD__SHUFFLE_FUNC(int8_t)
TLD__SHUFFLE_FUNC(uint8_t)
TLD__SHUFFLE_FUNC(int16_t)
TLD__SHUFFLE_FUNC(uint16_t)
TLD__SHUFFLE_FUNC(int32_t)
TLD__SHUFFLE_FUNC(uint32_t)
TLD__SHUFFLE_FUNC(int64_t)
TLD__SHUFFLE_FUNC(uint64_t)
TLD__SHUFFLE_FUNC(float)
TLD__SHUFFLE_FUNC(double)

#undef TLD__SHUFFLE_FUNC
