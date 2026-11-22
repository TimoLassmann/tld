#include "tld.h"
#include "test.h"

#include <signal.h>
#include <stdio.h>
#include <time.h>
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
        RUN(tld_thread_pool_alloc(&pool, 0.0, 2));
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
        RUN(tld_thread_pool_alloc(&pool, 0.0, 4));
        for(int round = 0; round < 5; round++){
                atomic_store(&counter, 0);
                for(int i = 0; i < n; i++){
                        RUN(tld_thread_pool_add(pool, inc_job, NULL, i, i + 1));
                }
                RUN(tld_thread_pool_wait(pool));        /* every job ran: OK */
                ASSERT(atomic_load(&counter) == n, "round %d: wait returned with %d of %d jobs done", round, atomic_load(&counter), n);
        }
        /* wait on an idle pool returns at once */
        RUN(tld_thread_pool_wait(pool));
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
                RUN(tld_thread_pool_alloc(&pool, 0.0, 4));
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
        RUN(tld_thread_pool_alloc(&pool, 1e-9, 4));
        atomic_store(&counter, 0);
        /* One job: the other three workers are idle on the work condition
           when the time limit is hit and must be woken to exit. */
        RUN(tld_thread_pool_add(pool, slow_job, NULL, 0, 1));
        /* the limit passed after the only job: nothing was lost, so OK */
        RUN(tld_thread_pool_wait(pool));
        ASSERT(atomic_load(&counter) == 1, "slow job did not run");
        ASSERT(pool->stop == 1, "pool not stopped after maxtime");
        /* A stopped pool refuses jobs, and wait reports that work was lost. */
        ASSERT(tld_thread_pool_add(pool, inc_job, NULL, 0, 1) == FAIL,
               "a stopped pool accepted a job");
        ASSERT(tld_thread_pool_wait(pool) == FAIL, "wait hid a refused job");
        ASSERT(atomic_load(&counter) == 1, "job ran on a stopped pool");
        tld_thread_pool_free(pool);
        pool = NULL;

        /* Jobs still queued when the limit passes are abandoned: wait says so. */
        RUN(tld_thread_pool_alloc(&pool, 1e-9, 2));
        atomic_store(&counter, 0);
        RUN(tld_thread_pool_add(pool, slow_job, NULL, 0, 1));
        RUN(tld_thread_pool_add(pool, slow_job, NULL, 0, 1));
        for(int i = 0; i < 50; i++){
                if(tld_thread_pool_add(pool, inc_job, NULL, 0, 1) != OK){
                        break;                  /* already stopped: refused, also counted */
                }
        }
        ASSERT(tld_thread_pool_wait(pool) == FAIL, "wait hid abandoned jobs");
        ASSERT(atomic_load(&counter) < 52, "every job ran despite the time limit");
        tld_thread_pool_free(pool);
        return OK;
ERROR:
        tld_thread_pool_free(pool);
        return FAIL;
}

static int test_global_stop(void)
{
        tld_thread_pool *pool = NULL;
        RUN(tld_thread_pool_alloc(&pool, 0.0, 4));
        atomic_store(&counter, 0);
        tld_global_stop_run = 1;
        ASSERT(tld_thread_pool_add(pool, inc_job, NULL, 0, 1) == FAIL,
               "job accepted after tld_global_stop_run was set");
        ASSERT(tld_thread_pool_wait(pool) == FAIL, "wait hid a refused job");
        ASSERT(atomic_load(&counter) == 0, "job ran after tld_global_stop_run was set");
        tld_thread_pool_free(pool);
        tld_global_stop_run = 0;
        return OK;
ERROR:
        tld_global_stop_run = 0;
        tld_thread_pool_free(pool);
        return FAIL;
}

/* unit-tpool THREADS JOBS [MS]: JOBS jobs of MS milliseconds on THREADS threads. */
#define DEMO_MAX_THREADS 256
static atomic_int per_thread[DEMO_MAX_THREADS];
static int demo_ms = 10;

static void demo_job(tld_thread_pool *p, void *data, int64_t start, int64_t end, int thread_id)
{
        UNUSED(p);
        UNUSED(data);
        UNUSED(start);
        UNUSED(end);
        usleep((useconds_t)demo_ms * 1000);
        if(thread_id >= 0 && thread_id < DEMO_MAX_THREADS){
                atomic_fetch_add(&per_thread[thread_id], 1);
        }
        atomic_fetch_add(&counter, 1);
}

static int demo(int argc, char **argv)
{
        tld_thread_pool *pool = NULL;
        struct timespec t0, t1;
        int threads = atoi(argv[1]);
        int jobs = atoi(argv[2]);
        if(argc > 3){
                demo_ms = atoi(argv[3]);
        }
        ASSERT(threads > 0 && threads <= DEMO_MAX_THREADS, "THREADS must be 1..%d", DEMO_MAX_THREADS);
        ASSERT(jobs >= 0 && demo_ms >= 0, "JOBS and MS must be non-negative");
        SECTION_MSG("%d jobs of %d ms on %d threads", jobs, demo_ms, threads);
        atomic_store(&counter, 0);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        RUN(tld_thread_pool_alloc(&pool, 0.0, threads));
        for(int i = 0; i < jobs; i++){
                RUN(tld_thread_pool_add(pool, demo_job, NULL, i, i + 1));
        }
        tld_thread_pool_wait(pool);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        tld_thread_pool_free(pool);
        pool = NULL;
        double secs = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
        fprintf(stdout, "jobs done: %d\n", atomic_load(&counter));
        fprintf(stdout, "wall time: %.3f s (serial would be %.3f s)\n", secs, jobs * demo_ms / 1000.0);
        for(int i = 0; i < threads; i++){
                fprintf(stdout, "thread %d: %d jobs\n", i, atomic_load(&per_thread[i]));
        }
        ASSERT(atomic_load(&counter) == jobs, "%d of %d jobs ran", atomic_load(&counter), jobs);
        OK_MSG("all jobs ran");
        return OK;
ERROR:
        tld_thread_pool_free(pool);
        return FAIL;
}

int main(int argc, char *argv[])
{
        test_init(&argc, argv, "[THREADS JOBS [MS]]",
                  "With no arguments: thread pool checks (no signal handlers installed, wait,\n"
                  "free drains the queue, maxtime stops the pool, tld_global_stop_run). A hang\n"
                  "is a failure: the program is killed after 30 s.\n"
                  "With THREADS and JOBS: runs JOBS jobs that each sleep MS milliseconds\n"
                  "(default 10) on a pool of THREADS threads and prints the wall time and the\n"
                  "jobs per thread.\n"
                  "Example: unit-tpool 4 100 5");
        if(argc == 3 || argc == 4){
                RUN(demo(argc, argv));
                return EXIT_SUCCESS;
        }
        if(argc != 1){
                fprintf(stderr, "unit-tpool takes THREADS JOBS [MS] (see -h)\n");
                return 2;
        }
        alarm(30);
        SECTION_MSG("no signal handler installed");
        RUN(test_no_signal_handler());
        OK_MSG("SIGINT and SIGABRT untouched");
        SECTION_MSG("wait");
        RUN(test_wait());
        OK_MSG("5 rounds of 64 jobs, wait returned after all");
        SECTION_MSG("free drains the queue");
        RUN(test_free_drains());
        OK_MSG("3 rounds of 32 jobs, all ran before free returned");
        SECTION_MSG("maxtime");
        RUN(test_maxtime());
        OK_MSG("pool stopped after maxtime; no job runs on a stopped pool");
        SECTION_MSG("tld_global_stop_run");
        RUN(test_global_stop());
        OK_MSG("no job ran after the global stop");
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}
