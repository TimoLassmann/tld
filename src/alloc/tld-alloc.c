#include "tld-alloc.h"
#include <stdlib.h>

#define ALIGN16 16
typedef struct {
        uint32_t aligned : 1;
        uint32_t dim1 : 31;
        uint32_t dim2;
        void* ptr;
} __attribute__((aligned(ALIGN16))) mem_i;
#undef ALIGN16

#define tld_mem_shift sizeof(mem_i)

int get_dim1(void* ptr,uint32_t* d)
{
        if(ptr){
                mem_i* h = NULL;
                h = (mem_i*) ( (size_t)ptr - tld_mem_shift);
                *d = h->dim1;
                return OK;
        }
        return FAIL;
}

int get_dim2(void* ptr,uint32_t* d)
{
        if(ptr){
                mem_i* h = NULL;
                h = (mem_i*) ( (size_t)ptr - tld_mem_shift);
                *d = h->dim2;
                return OK;
        }
        return FAIL;
}

int get_ptr(void* ptr,void** d)
{
        if(ptr){
                mem_i* h = NULL;
                h = (mem_i*) ( (size_t)ptr - tld_mem_shift);
                *d = h->ptr;
                return OK;
        }
        return FAIL;
}


int galloc_too_few(void)
{
        ERROR_MSG("Too few parameters passed to galloc.\nExpects at least 2");
        return OK;
ERROR:
        return FAIL;
}


void* galloc_hlp( size_t tsize, int dim1, int dim2)
{
        mem_i* h = NULL;
        void* tmp = NULL;

        ASSERT(dim1 >= 1,"DIM1 is too small: %d",dim1);
        ASSERT(dim2 >= 0,"DIM2 is negative: %d",dim2);
        ASSERT(tsize > 0,"Element size is 0");
        /* All sizes in size_t; refuse requests whose byte count does not fit. */
        ASSERT((size_t)dim1 <= (SIZE_MAX - tld_mem_shift) / tsize,
               "Allocation of %d x %zu bytes is too large", dim1, tsize);
        if(dim2){
                ASSERT((size_t)dim1 <= SIZE_MAX / (size_t)dim2 / tsize,
                       "Allocation of %d x %d x %zu bytes is too large", dim1, dim2, tsize);
        }

        MMALLOC(tmp, (size_t)dim1 * tsize + tld_mem_shift);

        h = (mem_i*)(tmp);
        h->aligned = 0u;
        h->dim1  = dim1;
        h->dim2  = dim2;
        h->ptr = NULL;
        if(dim2){
                MMALLOC(h->ptr, (size_t)dim1 * (size_t)dim2 * tsize);
        }
        return (void *)((size_t)tmp + tld_mem_shift);
ERROR:
        MFREE(tmp);
        return NULL;
}

void tld_free(void* p)
{
        mem_i* h = NULL;
        if(p == NULL){
                return;
        }
        h = (mem_i*) ( (size_t)p - tld_mem_shift);
        if(h->ptr){
                MFREE(h->ptr);
        }
        MFREE(h);
}
