#ifndef TLD_TPOOL_INTERNAL_H
#define TLD_TPOOL_INTERNAL_H

#include <pthread.h>
#include <stdint.h>
/* #include "tpool.h" */

#define TLD__WORK_EMPTY 0
#define TLD__WORK_TODO 1
#define TLD__WORK_DONE 2
typedef struct tld_thread_pool tld_thread_pool;

struct tld__work_item {
        struct tld__work_item* next;
        void (*func_ptr)(tld_thread_pool*, void*,int64_t, int64_t, int);
        void* data;
        int64_t start;
        int64_t end;
        int status;
};

struct tld__work_queue {
        struct tld__work_item** l;

        struct tld__work_item* store;
        struct tld__work_item* head;
        struct tld__work_item* tail;

        int n_alloc;
        int n;
};

static int tld__work_queue_haswork(struct tld__work_queue *q);
static int tld__work_queue_push(struct tld__work_queue *q, void (*func_ptr)(tld_thread_pool*, void *,int64_t, int64_t,int), void *data, int64_t start, int64_t end);

static int tld__work_queue_pop(struct tld__work_queue *q, void (**func_ptr)(tld_thread_pool*, void *,int64_t, int64_t,int), void **data,int64_t* start, int64_t* end);

static int tld__work_queue_alloc(struct tld__work_queue **queue, int size);
static void tld__work_queue_free(struct tld__work_queue *q);

#endif
