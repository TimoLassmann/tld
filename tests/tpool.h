#ifndef TPOOL_H
#define TPOOL_H

#include <pthread.h>

#ifdef TPOOL_IMPORT
#define EXTERN
#else
#define EXTERN extern
#endif

struct work_queue;

typedef struct tld_thread_pool {
        struct work_queue* work;
        int n_threads;
        /* Pausing */
        pthread_mutex_t lock;
        pthread_mutex_t qlock;
        pthread_cond_t work_cond;
        pthread_cond_t working_cond;
        pthread_attr_t attr;

        pthread_t* threads;
        int64_t* thread_id_map;
        int thread_id_idx;
        int n_working;
        int n_work;
        int stop;
        /* void (*function_ptr)(void*); */

} tld_thread_pool;

EXTERN int tld_thread_pool_add(tld_thread_pool *p, void (*func_ptr)(void *), void *data);

EXTERN int tld_thread_pool_start (tld_thread_pool *p);
EXTERN int tld_thread_pool_stop(tld_thread_pool *p);
EXTERN int tld_thread_pool_alloc(tld_thread_pool **pool, int n_threads);
EXTERN void  tld_thread_pool_wait(tld_thread_pool *p);
EXTERN void tld_thread_pool_free(tld_thread_pool *p);

#undef TPOOL_IMPORT
#undef EXTERN


#endif
