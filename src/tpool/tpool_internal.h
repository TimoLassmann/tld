#ifndef TPOOL_INTERNAL_H
#define TPOOL_INTERNAL_H

#include <pthread.h>

/* #include "tpool.h" */
#ifdef TPOOL_INTERNAL_IMPORT
#define EXTERN
#else
#define EXTERN extern
#endif


#define WORK_EMPTY 0
#define WORK_TODO 1
#define WORK_DONE 2
typedef struct tld_thread_pool tld_thread_pool;


struct work_item {
        struct work_item* next;
        void (*func_ptr)(tld_thread_pool*, void*,int);
        void* data;
        int status;
};


struct work_queue {
        struct work_item** l;


        struct work_item* store;
        struct work_item* head;
        struct work_item* tail;

        int n_alloc;
        int n;
};

EXTERN int work_queue_haswork(struct work_queue *q);
EXTERN int work_queue_push(struct work_queue *q, void (*func_ptr)(tld_thread_pool*, void *,int),
                           void *data);
EXTERN int work_queue_pop(struct work_queue *q, void (**func_ptr)(tld_thread_pool*, void *,int), void **data);


EXTERN int work_queue_alloc(struct work_queue **queue, int size);
EXTERN int work_queue_expand(struct work_queue *q, int add);
EXTERN void work_queue_free(struct work_queue *q);
#undef TPOOL_INTERNAL_IMPORT
#undef EXTERN


#endif
