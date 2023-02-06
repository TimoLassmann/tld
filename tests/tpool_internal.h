#ifndef TPOOL_INTERNAL_H
#define TPOOL_INTERNAL_H

#include <pthread.h>

#include "tpool.h"
#ifdef TPOOL_INTERNAL_IMPORT
#define EXTERN
#else
#define EXTERN extern
#endif


#define WORK_EMPTY 0
#define WORK_TODO 1
#define WORK_DONE 2


struct work_item {
        void (*func_ptr)(void*);
        void* data;
        int status;
};


struct work_queue {
        struct work_item** l;
        int n_alloc;
        int n;
};

EXTERN int work_queue_haswork(struct work_queue *q);
EXTERN int work_queue_push(struct work_queue *q, void (*func_ptr)(void *), void *data);
EXTERN int work_queue_pop(struct work_queue *q, void (**func_ptr)(void *), void **data);

EXTERN int work_queue_alloc(struct work_queue **queue, int size);
EXTERN int work_queue_expand(struct work_queue *q, int add);
EXTERN void work_queue_free(struct work_queue *q);
#undef TPOOL_INTERNAL_IMPORT
#undef EXTERN


#endif
