#include "tld-alloc.h"

#include <stdlib.h>


#define ALIGN16 16
typedef struct {
        uint32_t aligned : 1;
        uint32_t dim1 : 31;
        uint32_t dim2;
} __attribute__((aligned(ALIGN16))) mem_i;
#undef ALIGN16

#define tld_mem_shift  sizeof(mem_i)


int get_dim1(void* ptr,uint32_t* d)
{
        if(ptr){
                mem_i* h = NULL;
                h = (mem_i*) ( (size_t*)ptr - tld_mem_shift);
                *d = h->dim1;
                return OK;
        }
        return FAIL;
}

int get_dim2(void* ptr,uint32_t* d)
{
        if(ptr){
                mem_i* h = NULL;
                h = (mem_i*) ( (size_t*)ptr - tld_mem_shift);
                *d = h->dim2;
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

        MMALLOC(tmp,dim1  * tsize + tld_mem_shift);

        LOG_MSG("Allocated: %d", (int) dim1  * tsize + tld_mem_shift);
        h = (mem_i*)(tmp);
        h->aligned = 0u;
        h->dim1  = dim1;
        h->dim2  = dim2;
        LOG_MSG("Pointer : %p", h);
        LOG_MSG("Pointer ret : %p", (void *) (  (size_t*)tmp + tld_mem_shift));
        fprintf(stdout,"%d\n%d\n", (uint64_t) h , (uint64_t) (void *) (  (size_t*)tmp + tld_mem_shift));

        return (void *) (  (size_t)tmp + tld_mem_shift);
ERROR:
        return NULL;
}

int galloc_2D(void*** ptr, size_t tsize, int dim1, int dim2)
{
        mem_i* h = NULL;
        void** p = NULL;
        void* tmp1 = NULL;
        void** tmp2 = NULL;

        if(*ptr == NULL){
                MMALLOC(tmp1,(dim1  * sizeof(void*) + sizeof(mem_i)));
                MMALLOC(tmp2,((size_t)(dim1 * dim2)  * tsize));
        }else{
                ERROR_MSG("Already allocated");
        }
        h = (mem_i*)tmp1;
        h->aligned = 0u;
        h->dim1  = dim1;
        h->dim2  = dim2;
        p =(void** ) ((size_t *)tmp1 + sizeof(mem_i));
        for(int i = 0;i < dim1;i++){
                p[i] = tmp2 + sizeof(void*) * i * dim2;
        }
        *ptr = p;

        return OK;
ERROR:
        return FAIL;

}

void tld_free(void* p)
{
        mem_i* h = NULL;

        h = (mem_i*) ( (size_t)p - tld_mem_shift);
        LOG_MSG("%p P - in free", p);
        LOG_MSG("%p H - in free", h);
        LOG_MSG("Got here");
        fprintf(stdout,"%d %d\n",       h->dim1, h->dim2);
        if()
        free(h);
}
/* int galloc_2D(void** ptr, size_t tsize, int dim1, int dim2) */
/* { */

/*         return OK; */
/* ERROR: */
/*         return FAIL; */
/* } */
