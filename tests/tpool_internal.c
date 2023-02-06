
#include "tld.h"

#define TPOOL_INTERNAL_IMPORT
#include "tpool_internal.h"

static int  work_item_alloc(struct work_item **work);
static void work_item_free(struct work_item *w);

int work_queue_haswork(struct work_queue *q)
{
        if(q->n != -1){
                return 1;
        }
        return 0;
}

int work_queue_push(struct work_queue *q, void (*func_ptr)(void *), void *data)
{
        struct work_item* tmp = NULL;

        q->n++;
        if(q->n == q->n_alloc){
                RUN(work_queue_expand(q, 0));
        }
        tmp = q->l[q->n];

        tmp->data = data;
        tmp->func_ptr = func_ptr;
        tmp->status = WORK_TODO;
        /* LOG_MSG("Added %5d : %p %p", q->n, tmp->func_ptr, tmp->data); */
        return OK;
ERROR:
        return FAIL;
}

int work_queue_pop(struct work_queue *q, void (**func_ptr)(void *), void **data)
{
        struct work_item* tmp = NULL;
        *func_ptr = NULL;
        *data = NULL;
        if(q->n >= 0){
                tmp = q->l[q->n];
                *func_ptr = tmp->func_ptr;
                *data = tmp->data;
                /* LOG_MSG("taking:  %5d : %p %p", q->n, tmp->func_ptr, tmp->data); */
                q->n--;
        }
        return OK;
}


int work_queue_alloc(struct work_queue **queue, int size)
{
        struct work_queue* q = NULL;

        MMALLOC(q, sizeof(struct work_queue));

        q->n_alloc = size;
        q->n = -1;
        q->l = NULL;

        MMALLOC(q->l, sizeof(struct work_item*) * q->n_alloc);
        for(int i = 0; i < q->n_alloc;i++){
                q->l[i] = NULL;
                RUN(work_item_alloc(&q->l[i]));
        }

        *queue = q;
        return OK;
ERROR:
        work_queue_free(q);
        return FAIL;
}

int work_queue_expand(struct work_queue *q, int add)
{
        ASSERT(q != NULL,"No work queue");
        int old_size;
        if(add == 0){
                add = q->n_alloc/ 2;
        }


        old_size = q->n_alloc;
        q->n_alloc += add;

        MREALLOC(q->l, sizeof(struct work_item*) * q->n_alloc);
        for(int i = old_size; i < q->n_alloc;i++){
                q->l[i] = NULL;
                RUN(work_item_alloc(&q->l[i]));
        }
        return OK;
ERROR:
        work_queue_free(q);
        return FAIL;
}

void work_queue_free(struct work_queue *q)
{
        if(q){
                for(int i = 0; i < q->n_alloc;i++){
                        work_item_free(q->l[i]);
                }
                MFREE(q->l);
                MFREE(q);
        }
}

int work_item_alloc(struct work_item **work)
{
        struct work_item* w = NULL;
        MMALLOC(w, sizeof(struct work_item));
        w->func_ptr = NULL;
        w->status = WORK_EMPTY;
        *work = w;
        return OK;
ERROR:
        return FAIL;
}

void work_item_free(struct work_item *w)
{
        if(w){
                MFREE(w);
        }
}
