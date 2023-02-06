#include "tld.h"

#include <unistd.h>
#include "tpool.h"
#include "tpool_internal.h"

static void add(void *data, int thread_id);
static void sub(void *data, int thread_id);

int test_queue(void);
int test_pool(void);
int main(void)
{
        LOG_MSG("Hello pool");
        RUN(test_queue());
        RUN(test_pool());
        return EXIT_SUCCESS;
ERROR:
        return EXIT_SUCCESS;
}

int test_pool(void)

{
        tld_thread_pool* pool = NULL;
        int* array = NULL;


        galloc(&array, 20);

        for(int i = 0; i < 20;i++){
                array[i] = i+5;
        }
        tld_thread_pool_create(&pool,0.15, 8);
        /* tld_thread_pool_start(pool); */

        /* LOG_MSG("Got here"); */
        for(int i = 0; i < 20;i++){
                /* sleep(5); */
                tld_thread_pool_add(pool, add, &array[i]);
        }
        /* LOG_MSG("Got here - sleep 5"); */
        /* tld_thread_pool_stop(pool); */

        tld_thread_pool_wait(pool);

        LOG_MSG("The wait is over");
        for(int i = 0; i < 20;i++){
                fprintf(stdout,"%d %d\n", i, array[i]);
                /* add(array[i]); */
                /* fprintf(stdout,"%d %d\n", i, array[i]); */

        }

        for(int i = 0; i < 20;i++){
                /* sleep(5); */
                tld_thread_pool_add(pool, add, &array[i]);
                tld_thread_pool_add(pool, add, &array[i]);
                tld_thread_pool_add(pool, add, &array[i]);
                tld_thread_pool_add(pool, add, &array[i]);
                tld_thread_pool_add(pool, add, &array[i]);
        }

        tld_thread_pool_wait(pool);

        LOG_MSG("The 2nd wait is over active threads: %d", pool->n_active_threads);
        for(int i = 0; i < 20;i++){
                fprintf(stdout,"%d %d\n", i, array[i]);
                /* add(array[i]); */
                /* fprintf(stdout,"%d %d\n", i, array[i]); */

        }


        tld_thread_pool_free(pool);
        gfree(array);
        return OK;
ERROR:
        return FAIL;
}

int test_queue(void)
{

        struct work_queue* w = NULL;
        struct rng_state* rng = NULL;


        int number = 100;

        RUN(init_rng(&rng, 0));
        RUN(work_queue_alloc(&w, 10));

        for(int i = 0;i < 1245;i++){
                double r = tl_random_double(rng);
                if(r < 0.5){
                        work_queue_push(w, add, &number ) ;
                }else{
                        work_queue_push(w, sub, &number ) ;
                }
        }
        void (*func_ptr)(void *,int) = NULL;
        void* data = NULL;

        while(work_queue_haswork(w)){
                work_queue_pop(w,  &func_ptr, &data);
                func_ptr(data,0);
        }

        LOG_MSG("OPutput : %d", number);
        work_queue_free(w);

        free_rng(rng);
        return OK;
ERROR:
        return FAIL;
}


void add(void *data,int thread_id)
{
        int* num = (int*) data;

        LOG_MSG("Thread %d doing stuff", thread_id);
        *num = *num + 1;
}


void sub(void *data,int thread_id)
{
        int* num = (int*) data;
        LOG_MSG("Thread %d doing stuff", thread_id);
        *num = *num - 1;
}
