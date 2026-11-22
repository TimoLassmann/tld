
/* #include "tld.h" */
#include "../core/tld-core.h"
#include "../alloc/tld-alloc.h"
#include "tpool_internal.h"

static int  tld__work_item_alloc(struct tld__work_item **work);
static void tld__work_item_free(struct tld__work_item *w);
static int tld__work_queue_add_storage(struct tld__work_queue *q, int size);

int tld__work_queue_haswork(struct tld__work_queue *q)
{
        if(q->head != NULL){
                return 1;
        }
        return 0;
}

int tld__work_queue_push(struct tld__work_queue *q, void (*func_ptr)(tld_thread_pool*, void *,int64_t, int64_t,int), void *data, int64_t start, int64_t end)
{
        struct tld__work_item* new = NULL;

        if(q->store == NULL){
                RUN(tld__work_queue_add_storage(q, 1024));
        }

        new = q->store;

        q->store = q->store->next;

        new->next = NULL;

        new->func_ptr = func_ptr;
        new->data = data;
        new->status = TLD__WORK_TODO;
        new->start = start;
        new->end = end;

        if(q->head == NULL){
                q->tail = new;
                q->head = new;
        }else{
                q->tail->next = new;
                q->tail = q->tail->next;
        }

        return OK;
ERROR:
        return FAIL;
}

int tld__work_queue_pop(struct tld__work_queue *q, void (**func_ptr)(tld_thread_pool*,void *, int64_t, int64_t , int), void **data,int64_t* start, int64_t* end)
{

        struct tld__work_item* tmp = NULL;

        if(q->head == NULL){
                WARNING_MSG("Queue is empty");
                return OK;
        }

        tmp = q->head;

        *func_ptr = tmp->func_ptr;
        *data = tmp->data;
        *start = tmp->start;
        *end = tmp->end;
        q->head = tmp->next;

        tmp->next = q->store;
        q->store = tmp;

        return OK;
}

int tld__work_queue_add_storage(struct tld__work_queue *q, int size)
{
        struct tld__work_item* n = NULL;
        if(q->store == NULL){
                RUN(tld__work_item_alloc(&n));
                q->store = n;
        }
        for(int i = 0; i < size;i++){
                RUN(tld__work_item_alloc(&n));
                n->next = q->store;
                q->store = n;
        }
        return OK;
ERROR:
        return FAIL;
}

int tld__work_queue_alloc(struct tld__work_queue **queue, int size)
{
        struct tld__work_queue* q = NULL;

        MMALLOC(q, sizeof(struct tld__work_queue));

        q->n_alloc = size;
        q->n = -1;
        q->l = NULL;

        q->store = NULL;
        q->head = NULL;
        q->tail = NULL;

        RUN(tld__work_queue_add_storage(q, q->n_alloc));

        *queue = q;
        return OK;
ERROR:
        tld__work_queue_free(q);
        return FAIL;
}

void tld__work_queue_free(struct tld__work_queue *q)
{
        if(q){
                struct tld__work_item* n = NULL;
                struct tld__work_item* tmp = NULL;
                n = q->head;
                while(n){
                        tmp = n;
                        n = n->next;
                        tld__work_item_free(tmp);
                }

                n = q->store;
                while(n){
                        tmp = n;
                        n = n->next;
                        tld__work_item_free(tmp);
                }

                MFREE(q);
        }
}

int tld__work_item_alloc(struct tld__work_item **work)
{
        struct tld__work_item* w = NULL;
        MMALLOC(w, sizeof(struct tld__work_item));
        w->next = NULL;
        w->func_ptr = NULL;
        w->start = 0;
        w->end = 0;
        w->status = TLD__WORK_EMPTY;
        *work = w;
        return OK;
ERROR:
        return FAIL;
}

void tld__work_item_free(struct tld__work_item *w)
{
        if(w){
                MFREE(w);
        }
}
