#include "tld.h"

#include <signal.h>
#include <stdatomic.h>
#include <unistd.h>

/* Thread pool tests. A hang is a failure: alarm() kills the test long
   before the ctest timeout. */

#define UNUSED(expr) do { (void)(expr); } while (0)

static atomic_int counter;

static void inc_job(tld_thread_pool *p, void *data, int64_t start, int64_t end, int thread_id)
{
        UNUSED(p);
        UNUSED(data);
        UNUSED(thread_id);
        usleep(1000);
        atomic_fetch_add(&counter, (int)(end - start));
}

static void slow_job(tld_thread_pool *p, void *data, int64_t start, int64_t end, int thread_id)
{
        UNUSED(p);
        UNUSED(data);
        UNUSED(start);
        UNUSED(end);
        UNUSED(thread_id);
        /* Long enough that time() advances past the start time, so a tiny
           maxtime is exceeded when the job finishes. */
        usleep(1050000);
        atomic_fetch_add(&counter, 1);
}

static int test_no_signal_handler(void)
{
        tld_thread_pool *pool = NULL;
        struct sigaction sa;
        RUN(tld_thread_pool_create(&pool, 0.0, 2));
        ASSERT(sigaction(SIGINT, NULL, &sa) == 0, "sigaction failed");
        ASSERT(sa.sa_handler == SIG_DFL, "library installed a SIGINT handler");
        ASSERT(sigaction(SIGABRT, NULL, &sa) == 0, "sigaction failed");
        ASSERT(sa.sa_handler == SIG_DFL, "library installed a SIGABRT handler");
        tld_thread_pool_free(pool);
        return OK;
ERROR:
        return FAIL;
}

static int test_wait(void)
{
        tld_thread_pool *pool = NULL;
        const int n = 64;
        RUN(tld_thread_pool_create(&pool, 0.0, 4));
        for(int round = 0; round < 5; round++){
                atomic_store(&counter, 0);
                for(int i = 0; i < n; i++){
                        RUN(tld_thread_pool_add(pool, inc_job, NULL, i, i + 1));
                }
                tld_thread_pool_wait(pool);
                ASSERT(atomic_load(&counter) == n, "round %d: wait returned with %d of %d jobs done", round, atomic_load(&counter), n);
        }
        /* wait on an idle pool returns at once */
        tld_thread_pool_wait(pool);
        tld_thread_pool_free(pool);
        return OK;
ERROR:
        tld_thread_pool_free(pool);
        return FAIL;
}

static int test_free_drains(void)
{
        tld_thread_pool *pool = NULL;
        const int n = 32;
        for(int round = 0; round < 3; round++){
                pool = NULL;
                atomic_store(&counter, 0);
                RUN(tld_thread_pool_create(&pool, 0.0, 4));
                for(int i = 0; i < n; i++){
                        RUN(tld_thread_pool_add(pool, inc_job, NULL, i, i + 1));
                }
                tld_thread_pool_free(pool);
                pool = NULL;
                ASSERT(atomic_load(&counter) == n, "round %d: free ran %d of %d queued jobs", round, atomic_load(&counter), n);
        }
        return OK;
ERROR:
        tld_thread_pool_free(pool);
        return FAIL;
}

static int test_maxtime(void)
{
        tld_thread_pool *pool = NULL;
        /* maxtime is in minutes; anything above zero seconds exceeds it. */
        RUN(tld_thread_pool_create(&pool, 1e-9, 4));
        atomic_store(&counter, 0);
        /* One job: the other three workers are idle on the work condition
           when the time limit is hit and must be woken to exit. */
        RUN(tld_thread_pool_add(pool, slow_job, NULL, 0, 1));
        tld_thread_pool_wait(pool);
        ASSERT(atomic_load(&counter) == 1, "slow job did not run");
        ASSERT(pool->stop == 1, "pool not stopped after maxtime");
        /* Adding to a stopped pool is accepted but nothing runs. */
        RUN(tld_thread_pool_add(pool, inc_job, NULL, 0, 1));
        tld_thread_pool_wait(pool);
        ASSERT(atomic_load(&counter) == 1, "job ran on a stopped pool");
        tld_thread_pool_free(pool);
        return OK;
ERROR:
        tld_thread_pool_free(pool);
        return FAIL;
}

static int test_global_stop(void)
{
        tld_thread_pool *pool = NULL;
        RUN(tld_thread_pool_create(&pool, 0.0, 4));
        atomic_store(&counter, 0);
        tld_global_stop_run = 1;
        RUN(tld_thread_pool_add(pool, inc_job, NULL, 0, 1));
        tld_thread_pool_wait(pool);
        ASSERT(atomic_load(&counter) == 0, "job ran after tld_global_stop_run was set");
        tld_thread_pool_free(pool);
        tld_global_stop_run = 0;
        return OK;
ERROR:
        tld_global_stop_run = 0;
        tld_thread_pool_free(pool);
        return FAIL;
}

int main(void)
{
        alarm(30);
        RUN(test_no_signal_handler());
        RUN(test_wait());
        RUN(test_free_drains());
        RUN(test_maxtime());
        RUN(test_global_stop());
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}
