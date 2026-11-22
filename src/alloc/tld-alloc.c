#include "tld-alloc.h"
#include <stdlib.h>

#define ALIGN16 16
typedef struct {
        uint32_t aligned : 1;
        uint32_t dim1 : 31;
        uint32_t dim2;
        void* ptr;
} __attribute__((aligned(ALIGN16))) tld__mem_info;
#undef ALIGN16

#define TLD__MEM_SHIFT sizeof(tld__mem_info)

int tld_galloc_dim1(void* ptr,uint32_t* d)
{
        if(ptr){
                tld__mem_info* h = NULL;
                h = (tld__mem_info*) ( (size_t)ptr - TLD__MEM_SHIFT);
                *d = h->dim1;
                return OK;
        }
        return FAIL;
}

int tld_galloc_dim2(void* ptr,uint32_t* d)
{
        if(ptr){
                tld__mem_info* h = NULL;
                h = (tld__mem_info*) ( (size_t)ptr - TLD__MEM_SHIFT);
                *d = h->dim2;
                return OK;
        }
        return FAIL;
}

void tld__gfree(void* p)
{
        tld__mem_info* h = NULL;
        if(p == NULL){
                return;
        }
        h = (tld__mem_info*) ( (size_t)p - TLD__MEM_SHIFT);
        if(h->ptr){
                MFREE(h->ptr);
        }
        MFREE(h);
}

int tld__galloc_too_few(void)
{
        ERROR_MSG("Too few parameters passed to galloc.\nExpects at least 2");
        return OK;
ERROR:
        return FAIL;
}

void* tld__galloc(size_t rsize, size_t tsize, int dim1, int dim2)
{
        tld__mem_info* h = NULL;
        void* tmp = NULL;

        ASSERT(dim1 >= 1,"DIM1 is too small: %d",dim1);
        ASSERT(dim2 >= 0,"DIM2 is negative: %d",dim2);
        ASSERT(tsize > 0 && rsize > 0,"Element size is 0");
        /* All sizes in size_t; refuse requests whose byte count does not fit. */
        ASSERT((size_t)dim1 <= (SIZE_MAX - TLD__MEM_SHIFT) / rsize,
               "Allocation of %d x %zu bytes is too large", dim1, rsize);
        if(dim2){
                ASSERT((size_t)dim1 <= SIZE_MAX / (size_t)dim2 / tsize,
                       "Allocation of %d x %d x %zu bytes is too large", dim1, dim2, tsize);
        }

        MMALLOC(tmp, (size_t)dim1 * rsize + TLD__MEM_SHIFT);

        h = (tld__mem_info*)(tmp);
        h->aligned = 0u;
        h->dim1  = dim1;
        h->dim2  = dim2;
        h->ptr = NULL;
        if(dim2){
                MMALLOC(h->ptr, (size_t)dim1 * (size_t)dim2 * tsize);
        }
        return (void *)((size_t)tmp + TLD__MEM_SHIFT);
ERROR:
        MFREE(tmp);
        return NULL;
}

int tld__galloc_data(void* ptr,void** d)
{
        if(ptr){
                tld__mem_info* h = NULL;
                h = (tld__mem_info*) ( (size_t)ptr - TLD__MEM_SHIFT);
                *d = h->ptr;
                return OK;
        }
        return FAIL;
}

#undef TLD__MEM_SHIFT
