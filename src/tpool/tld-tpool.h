/* tpool: a pthread pool. Jobs func(pool, data, start, end, thread_id) are queued with
 * tld_thread_pool_add and run in FIFO order by a fixed set of worker threads; optional
 * time limit and a program-wide stop (tld_thread_pool_stop_all). */
#ifndef TLD_TPOOL_H
#define TLD_TPOOL_H
#include "../core/tld-core.h"
#include <pthread.h>
#include <stdint.h>
#include <stdatomic.h>
/* Opaque pool; create with tld_thread_pool_alloc, free with tld_thread_pool_free. */
typedef struct tld_thread_pool tld_thread_pool;

/* Stop every pool, running and future, for the rest of the program: workers stop
 * between jobs and drop queued jobs, and tld_thread_pool_add refuses new ones. The
 * library installs no signal handlers; this only sets an atomic flag, so a program may
 * call it from its own handler (e.g. on SIGINT). */
TLD_API void tld_thread_pool_stop_all(void);

/* Start a pool of n_threads workers (at least 2) in *pool. max_time is a limit in
 * minutes since creation, checked after each job: once passed, the pool stops and
 * queued jobs are dropped. 0 means no limit. */
TLD_API int  tld_thread_pool_alloc(tld_thread_pool **pool, double max_time, int n_threads);

/* Queue a job: a worker calls func_ptr(p, data, start, end, thread_id), thread_id in
 * [0, n_threads). start and end are passed through (e.g. a slice of an index range).
 * Thread-safe; may be called from inside a job. Once the pool has stopped (time limit
 * or tld_thread_pool_stop_all) the job is refused: FAIL without a message, and data is still
 * the caller's to free. */
TLD_API int tld_thread_pool_add(tld_thread_pool *p, void (*func_ptr)(tld_thread_pool*,void *,int64_t, int64_t,int), void *data, int64_t start, int64_t end);

/* Block until the queue is empty and no job is running (after a stop: until every
 * worker has exited). OK when every job added to the pool has run; FAIL, without a
 * message, when a stop refused or abandoned jobs (they did not run, so whatever they
 * would have done or freed did not happen). A running pool can take more jobs
 * afterwards. NULL is ignored (OK). */
TLD_API int tld_thread_pool_wait(tld_thread_pool *p);

/* Run every job still queued, then stop and join the workers and free the pool. NULL
 * is ignored. */
TLD_API void tld_thread_pool_free(tld_thread_pool *p);

#endif
