#include "../alloc/tld-alloc.h"
#include "bitfield.h"

#define BITS_W 64
#define BIT_SHIFT 6
#define BIT_MASK 0x3F

static inline int _tl_bitfield_popcount_branchless(uint64_t x);
/* Here is a piece of code to calculate the hamming distance
        between two bitfields using the popcount instruction
        available on most modern CPUs
 */

int tl_bitfield_hamming(tl_bitfield *b1, tl_bitfield *b2, int32_t* dist)
{
        /* making sure the bitfields have the same length */
        if(b1->len != b2->len){
                ERROR_MSG("bitfields have different lengths");
        }
        int32_t len = b1->alloc_len;
        int32_t count = 0;
        for(int i = 0; i <  len; i++){
                /* fprintf(stderr, "b1->m[%d] = %llX  b2->m[%d] = %llX\n", i, b1->m[i], i, b2->m[i]); */
                count += _tl_bitfield_popcount_branchless(b1->m[i] ^ b2->m[i]);
        }
        *dist = count;
        return OK;
ERROR:
        return FAIL;
}

/* here is an alternative implementation of popcountll using branchless code*/
int _tl_bitfield_popcount_branchless(uint64_t x)
{
        x = x - ((x >> 1) & 0x5555555555555555ULL);
        x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
        x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
        x = x + (x >> 8);
        x = x + (x >> 16);
        x = x + (x >> 32);
        return x & 0x7F;
}


int tl_bitfield_set(tl_bitfield *b, int32_t index)
{
        if(index < 0 || index >= b->len){
                return FAIL;
        }
        b->m[index >> BIT_SHIFT] |= (1ULL << (index & BIT_MASK));
        return OK;
}

int tl_bitfield_get(tl_bitfield *b, int32_t index)
{
        if(index < 0 || index >= b->len){
                return FAIL;
        }
        return (b->m[index >> BIT_SHIFT] & (1ULL << (index & BIT_MASK))) != 0;
}

int tl_bitfield_clear(tl_bitfield *b, int32_t index)
{
        if(index < 0 || index >= b->len){
                return FAIL;
        }
        b->m[index >> BIT_SHIFT] &= ~(1ULL << (index & BIT_MASK));
        return OK;
}

int tl_bitfield_reset(tl_bitfield *b)
{
        memset(b->m, 0, b->alloc_len * sizeof(uint64_t));
        return OK;
}

int tl_bitfield_alloc(tl_bitfield **bit_f, int32_t size)
{
        tl_bitfield* b = NULL;
        MMALLOC(b, sizeof(tl_bitfield));
        b->alloc_len = 1 + size / BITS_W;
        galloc(&b->m, b->alloc_len);
        b->len = size;

        RUN(tl_bitfield_reset(b));
        *bit_f = b;
        return OK;
ERROR:
        tl_bitfield_free(b);
        return FAIL;
}

void tl_bitfield_free(tl_bitfield *b)
{
        if(b){
                if(b->m){
                        gfree(b->m);
                }
                free(b);
        }
}



#undef BIT_MASK
#undef BIT_SHIFT
#undef BITS_W
