#include "../core/tld-core.h"
#include "../alloc/tld-alloc.h"

/* Weird but works... */
#include "tld-tpool_internal.h"
#include "tld-tpool_internal.c"

#include <unistd.h>

#include "tld-tpool.h"

/* The library does not install signal handlers. A program that wants to
   stop running pools (e.g. on SIGINT) sets tld_global_stop_run = 1 itself;
   workers check the flag between jobs. The mutex and condition are left for
   programs that want to sleep until the flag is set. */
atomic_int tld_global_stop_run;
pthread_mutex_t tld_global_cond_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t tld_global_cond = PTHREAD_COND_INITIALIZER;

static void *tld__pool_worker(void *data);
static int tld__pool_get_tid(tld_thread_pool *p);
static void tld__pool_set_stop_locked(tld_thread_pool *p);
static void tld__pool_shutdown(tld_thread_pool *p, int n_started);

static int tld__pool_test_runtime(time_t internal_time,time_t* stop_watch,double maxtime,int* run);

/* Must be called with p->lock held. Marks the pool as stopped and wakes
   every thread: idle workers (work_cond) so they can exit and waiters
   (working_cond) so they can re-check their condition. */
static void tld__pool_set_stop_locked(tld_thread_pool *p)
{
        p->stop = 1;
        pthread_cond_broadcast(&p->work_cond);
        pthread_cond_broadcast(&p->working_cond);
}

void *tld__pool_worker(void *data)
{
        tld_thread_pool*  p = (tld_thread_pool*) data;
        void (*func_ptr)(tld_thread_pool*, void *,int64_t, int64_t, int ) = NULL;
        void* fun_data = NULL;
        const int thread_id = tld__pool_get_tid(p);
        int64_t start;
        int64_t end;

        pthread_mutex_lock(&p->lock);
        while(1){
                while(!tld__work_queue_haswork(p->work) && !p->stop){
                        pthread_cond_wait(&p->work_cond, &p->lock);
                }
                if(tld_global_stop_run == 1 && !p->stop){
                        tld__pool_set_stop_locked(p);
                }
                /* stop means abandon queued work (time limit or global
                   stop flag); tld_thread_pool_free drains the queue
                   before it sets stop. */
                if(p->stop){
                        break;
                }

                start = 0 ;
                end = 0;
                tld__work_queue_pop(p->work,  &func_ptr, &fun_data, &start, &end);
                p->n_working++;
                pthread_mutex_unlock(&p->lock);

                func_ptr(p,fun_data,start, end, thread_id);

                pthread_mutex_lock(&p->lock);
                p->n_jobs_processed++;
                p->n_working--;
                if(p->maxtime && !p->stop){
                        int timeout = 0;
                        tld__pool_test_runtime(p->internal_time, &p->stop_watch, p->maxtime, &timeout);
                        if(timeout){
                                tld__pool_set_stop_locked(p);
                        }
                }
                if(p->n_working == 0 && !tld__work_queue_haswork(p->work)){
                        pthread_cond_broadcast(&p->working_cond);
                }
        }
        p->n_active_threads--;
        pthread_cond_broadcast(&p->working_cond);
        pthread_mutex_unlock(&p->lock);
        return NULL;
}

/* Stops the workers and joins the first n_started threads, then frees the
   pool. Does not drain the queue. */
static void tld__pool_shutdown(tld_thread_pool *p, int n_started)
{
        pthread_mutex_lock(&p->lock);
        tld__pool_set_stop_locked(p);
        pthread_mutex_unlock(&p->lock);

        for(int t = 0; t < n_started; t++) {
                int rc = pthread_join(p->threads[t], NULL);
                if(rc){
                        WARNING_MSG("pthread_join() failed with return code %d", rc);
                }
        }

        MFREE(p->threads);
        gfree(p->thread_id_map);
        tld__work_queue_free(p->work);
        pthread_attr_destroy(&p->attr);
        pthread_mutex_destroy(&p->lock);
        pthread_cond_destroy(&p->work_cond);
        pthread_cond_destroy(&p->working_cond);
        MFREE(p);
}

int tld__pool_test_runtime(time_t internal_time,time_t* stop_watch,double maxtime,int* run)
{
        double diff_t = 0.0;
        time_t cur_time;

        time(&cur_time);
        diff_t = difftime(cur_time , *stop_watch) / 60.0;

        if(diff_t > 1.0){
                time(stop_watch);
        }

        diff_t = difftime(cur_time , internal_time) / 60.0;
        if(diff_t > maxtime){
                *run = 1;
        }
        return OK;
}

int tld__pool_get_tid(tld_thread_pool *p)
{
        int i;
        int64_t tid = (int64_t) pthread_self();
        int ID = -1;
        pthread_mutex_lock(&p->lock);
        for(i = 0; i < p->thread_id_idx;i++){
                if(p->thread_id_map[i] == tid){
                        ID = i;
                        break;
                }
        }
        if(ID == -1){
                ID = p->thread_id_idx;
                p->thread_id_map[p->thread_id_idx] = tid;
                p->thread_id_idx++;
        }
        pthread_mutex_unlock(&p->lock);

        return ID;
}

int tld_thread_pool_alloc(tld_thread_pool **pool, double max_time, int n_threads)
{
        tld_thread_pool* p = NULL;
        int n_started = 0;
        int sync_init = 0;
        ASSERT(pool != NULL, "No pool pointer");
        MMALLOC(p, sizeof(tld_thread_pool));

        if(n_threads < 2){
                n_threads = 2;
        }
        p->work = NULL;
        p->n_threads = n_threads;
        p->n_active_threads = 0;
        p->n_jobs_processed = 0;
        p->maxtime = max_time;
        p->n_working = 0;
        p->stop = 0;
        p->n_dropped = 0;

        p->thread_id_map = NULL;
        p->threads = NULL;
        galloc(&p->thread_id_map,p->n_threads);
        p->thread_id_idx = 0;
        MMALLOC(p->threads, sizeof(pthread_t) * p->n_threads);
        RUN(tld__work_queue_alloc(&p->work, 1024));

        pthread_mutex_init(&p->lock, NULL);
        pthread_cond_init(&p->work_cond, NULL);
        pthread_cond_init(&p->working_cond, NULL);
        pthread_attr_init(&p->attr);
        pthread_attr_setdetachstate(&p->attr, PTHREAD_CREATE_JOINABLE);
        sync_init = 1;

        time(&p->internal_time);
        time(&p->stop_watch);

        for(int t = 0; t < p->n_threads; t++){
                int rc = 0;
                pthread_mutex_lock(&p->lock);
                p->n_active_threads++;
                pthread_mutex_unlock(&p->lock);
                rc = pthread_create(&p->threads[t], &p->attr, tld__pool_worker,  (void *)p);
                if(rc){
                        pthread_mutex_lock(&p->lock);
                        p->n_active_threads--;
                        pthread_mutex_unlock(&p->lock);
                        ERROR_MSG("pthread_create() failed with return code %d", rc);
                }
                n_started++;
        }

        *pool = p;
        return OK;
ERROR:
        if(p){
                if(sync_init){
                        tld__pool_shutdown(p, n_started);
                }else{
                        if(p->threads){
                                MFREE(p->threads);
                        }
                        if(p->thread_id_map){
                                gfree(p->thread_id_map);
                        }
                        tld__work_queue_free(p->work);
                        MFREE(p);
                }
        }
        return FAIL;
}

int tld_thread_pool_add(tld_thread_pool *p, void (*func_ptr)(tld_thread_pool*,void *,int64_t, int64_t,int), void *data, int64_t start, int64_t end)
{
        int status = OK;
        ASSERT(p != NULL, "No thread pool");
        ASSERT(func_ptr != NULL, "No job function");
        pthread_mutex_lock(&p->lock);
        if(tld_global_stop_run == 1 && !p->stop){
                tld__pool_set_stop_locked(p);
        }
        if(p->stop){
                /* refused: the job will not run, so data is still the caller's */
                p->n_dropped++;
                pthread_mutex_unlock(&p->lock);
                return FAIL;
        }
        status = tld__work_queue_push(p->work, func_ptr, data, start, end);
        if(status == OK){
                pthread_cond_broadcast(&p->work_cond);
        }
        pthread_mutex_unlock(&p->lock);
        RUN(status);
        return OK;
ERROR:
        return FAIL;
}

int tld_thread_pool_wait(tld_thread_pool *p)
{
        int dropped = 0;
        if(p){
                pthread_mutex_lock(&p->lock);
                while(1){
                        if(p->stop){
                                /* Queued work is abandoned; wait for every
                                   worker to exit. */
                                if(p->n_active_threads == 0){
                                        break;
                                }
                        }else if(p->n_working == 0 && !tld__work_queue_haswork(p->work)){
                                break;
                        }
                        pthread_cond_wait(&p->working_cond, &p->lock);
                }
                /* Count (and discard) the jobs a stop left in the queue. */
                while(p->stop && tld__work_queue_haswork(p->work)){
                        void (*fn)(tld_thread_pool*, void *,int64_t, int64_t, int) = NULL;
                        void* data = NULL;
                        int64_t s = 0;
                        int64_t e = 0;
                        tld__work_queue_pop(p->work, &fn, &data, &s, &e);
                        p->n_dropped++;
                }
                dropped = p->n_dropped;
                pthread_mutex_unlock(&p->lock);
        }
        return dropped ? FAIL : OK;
}

void tld_thread_pool_free(tld_thread_pool *p)
{
        if(p){
                /* Run everything already queued before stopping. */
                (void)tld_thread_pool_wait(p);
                tld__pool_shutdown(p, p->n_threads);
        }
}

#undef TLD__WORK_DONE
#undef TLD__WORK_EMPTY
#undef TLD__WORK_TODO
