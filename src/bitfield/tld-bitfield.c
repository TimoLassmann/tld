#include "../alloc/tld-alloc.h"
#include "tld-bitfield.h"

#define BITS_W 64
#define BIT_SHIFT 6
#define BIT_MASK 0x3F

static inline int tld__bitfield_popcount(uint64_t x);
/* Here is a piece of code to calculate the hamming distance
        between two bitfields using the popcount instruction
        available on most modern CPUs
 */

int tld_bitfield_alloc(tld_bitfield **bit_f, int32_t size)
{
        tld_bitfield* b = NULL;
        ASSERT(bit_f != NULL, "No output pointer");
        MMALLOC(b, sizeof(tld_bitfield));
        b->alloc_len = 1 + size / BITS_W;
        galloc(&b->m, b->alloc_len);
        b->len = size;

        RUN(tld_bitfield_reset(b));
        *bit_f = b;
        return OK;
ERROR:
        tld_bitfield_free(b);
        return FAIL;
}

void tld_bitfield_free(tld_bitfield *b)
{
        if(b){
                if(b->m){
                        gfree(b->m);
                }
                free(b);
        }
}

int tld_bitfield_reset(tld_bitfield *b)
{
        ASSERT(b != NULL, "No bitfield");
        memset(b->m, 0, b->alloc_len * sizeof(uint64_t));
        return OK;
ERROR:
        return FAIL;
}

int tld_bitfield_set(tld_bitfield *b, int32_t index)
{
        ASSERT(b != NULL, "No bitfield");
        if(index < 0 || index >= b->len){
                return FAIL;
        }
        b->m[index >> BIT_SHIFT] |= (1ULL << (index & BIT_MASK));
        return OK;
ERROR:
        return FAIL;
}

int tld_bitfield_get(tld_bitfield *b, int32_t index, uint8_t* bit)
{
        ASSERT(b != NULL, "No bitfield");
        ASSERT(bit != NULL, "No output pointer");
        if(index < 0 || index >= b->len){
                return FAIL;
        }
        *bit = (b->m[index >> BIT_SHIFT] & (1ULL << (index & BIT_MASK))) != 0;
        return OK;
ERROR:
        return FAIL;
}

int tld_bitfield_clear(tld_bitfield *b, int32_t index)
{
        ASSERT(b != NULL, "No bitfield");
        if(index < 0 || index >= b->len){
                return FAIL;
        }
        b->m[index >> BIT_SHIFT] &= ~(1ULL << (index & BIT_MASK));
        return OK;
ERROR:
        return FAIL;
}

int tld_bitfield_copy(tld_bitfield *dst, tld_bitfield *src)
{
        ASSERT(dst != NULL, "No bitfield");
        ASSERT(src != NULL, "No bitfield");
        if(dst->len != src->len){
                ERROR_MSG("bitfields have different lengths");
        }
        memcpy(dst->m, src->m, src->alloc_len * sizeof(uint64_t));
        dst->len = src->len;
        dst->alloc_len = src->alloc_len;
        return OK;
ERROR:
        return FAIL;
}

int tld_bitfield_or(tld_bitfield *b1, tld_bitfield *b2,tld_bitfield* ret)
{
        ASSERT(b1 != NULL, "No bitfield");
        ASSERT(b2 != NULL, "No bitfield");
        ASSERT(ret != NULL, "No bitfield");
        if(b1->len != b2->len){
                ERROR_MSG("bitfields have different lengths");
        }
        if(ret->len != b1->len){
                ERROR_MSG("bitfields have different lengths");
        }

        int32_t len = b1->alloc_len;

        for(int i = 0; i <  len; i++){
                ret->m[i] = b1->m[i] | b2->m[i];
        }
        return OK;
ERROR:
        return FAIL;
}

/* here is an alternative implementation of popcountll using branchless code*/
int tld__bitfield_popcount(uint64_t x)
{
        x = x - ((x >> 1) & 0x5555555555555555ULL);
        x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
        x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
        x = x + (x >> 8);
        x = x + (x >> 16);
        x = x + (x >> 32);
        return x & 0x7F;
}

int tld_bitfield_hamming(tld_bitfield *b1, tld_bitfield *b2,tld_bitfield* mask, int32_t* dist)
{
        /* making sure the bitfields have the same length */
        ASSERT(b1 != NULL, "No bitfield");
        ASSERT(b2 != NULL, "No bitfield");
        ASSERT(dist != NULL, "No output pointer");
        if(b1->len != b2->len || b2->alloc_len < b1->alloc_len){
                ERROR_MSG("bitfields have different lengths");
        }
        int32_t len = b1->alloc_len;
        int32_t count = 0;
        if(mask == NULL){
                for(int i = 0; i <  len; i++){
                        count += tld__bitfield_popcount(b1->m[i] ^ b2->m[i]);
                }
        }else{
                if(mask->len != b1->len || mask->alloc_len < len){
                        ERROR_MSG("mask and bitfields have different lengths");
                }
                for(int i = 0; i <  len; i++){
                        count += tld__bitfield_popcount((b1->m[i] ^ b2->m[i]) & mask->m[i]);
                }
        }
        *dist = count;
        return OK;
ERROR:
        return FAIL;
}

#undef BIT_MASK
#undef BIT_SHIFT
#undef BITS_W
