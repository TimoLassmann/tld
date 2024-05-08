#include "../alloc/tld-alloc.h"
#include "bitfield.h"

#define BITS_W 64
#define BIT_SHIFT 6
#define BIT_MASK 0x3F

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

int tl_bitfield_alloc(tl_bitfield **bit_f, int32_t size)
{
        tl_bitfield* b = NULL;
        MMALLOC(b, sizeof(tl_bitfield));
        int alloc_len = 1 + size / BITS_W;
        galloc(&b->m,alloc_len);
        b->len = size;
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
