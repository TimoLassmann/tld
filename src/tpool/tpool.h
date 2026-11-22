/* tpool: a pthread pool. Jobs func(pool, data, start, end, thread_id) are queued with
 * tld_thread_pool_add and run in FIFO order by a fixed set of worker threads; optional
 * time limit and a global stop flag. */
#ifndef TPOOL_H
#define TPOOL_H
#include "../core/tld-core.h"
#include <pthread.h>
#include <stdint.h>
#include <stdatomic.h>
struct work_queue;

/* Global stop flag. The library installs no signal handlers: a program that wants to
 * stop running pools (e.g. on SIGINT) sets tld_global_stop_run = 1 itself; workers
 * check it between jobs and stop, dropping queued jobs. The mutex and condition are not
 * used by tld; they are there for programs that want to sleep until the flag is set. */
tld_external atomic_int tld_global_stop_run;
tld_external pthread_mutex_t tld_global_cond_mutex;
tld_external  pthread_cond_t tld_global_cond;

/* A thread pool; create with tld_thread_pool_create, free with tld_thread_pool_free.
 * The fields are internal (n_threads may be read). */
typedef struct tld_thread_pool {
        struct work_queue* work;
        int n_threads;
        int n_active_threads;
        int n_jobs_processed;
        /* Pausing */
        pthread_mutex_t lock;

        pthread_cond_t work_cond;
        pthread_cond_t working_cond;
        pthread_attr_t attr;

        pthread_t* threads;
        int64_t* thread_id_map;
        int thread_id_idx;
        int n_working;
        /* int n_work; */
        int stop;
        double maxtime;
        time_t internal_time;
        time_t stop_watch;
        /* void (*function_ptr)(void*); */

} tld_thread_pool;

/* Start a pool of n_threads workers (at least 2) in *pool. max_time is a limit in
 * minutes since creation, checked after each job: once passed, the pool stops and
 * queued jobs are dropped. 0 means no limit. */
tld_external int  tld_thread_pool_create(tld_thread_pool **pool, double max_time, int n_threads);

/* Queue a job: a worker calls func_ptr(p, data, start, end, thread_id), thread_id in
 * [0, n_threads). start and end are passed through (e.g. a slice of an index range).
 * Thread-safe; may be called from inside a job. Once the pool has stopped (time limit
 * or tld_global_stop_run) the job is dropped and OK is still returned. */
tld_external int tld_thread_pool_add(tld_thread_pool *p, void (*func_ptr)(tld_thread_pool*,void *,int64_t, int64_t,int), void *data, int64_t start, int64_t end);

/* Block until the queue is empty and no job is running (after a stop: until every
 * worker has exited). The pool can take more jobs afterwards. NULL is ignored. */
tld_external void tld_thread_pool_wait(tld_thread_pool *p);

/* Run every job still queued, then stop and join the workers and free the pool. NULL
 * is ignored. */
tld_external void tld_thread_pool_free(tld_thread_pool *p);

#endif
