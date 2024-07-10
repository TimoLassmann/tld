
#include "../alloc/tld-alloc.h"

#include "tld-json.h"

#define TLD_JSON_MEM_IMPORT
#include "tld-json_mem.h"

int tld_json_obj_copy(tld_json_obj **target, tld_json_obj *s)
{
        tld_json_obj* t = NULL;
        RUN(tld_json_obj_alloc(&t));
        for(int i = 0; i < s->n;i++){
                tld_append(t->key[i], TLD_STR(s->key[i]));
                tld_json_val_copy(t->v[i], s->v[i]);
                t->n++;
                tld_json_obj_chk_space(t);
        }

        *target = t;
        return OK;
ERROR:
        return FAIL;
}


int tld_json_obj_alloc(tld_json_obj **node)
{
        tld_json_obj *n = NULL;
        MMALLOC(n, sizeof(tld_json_obj));
        n->key = NULL;
        n->v = NULL;
        n->n_alloc = 4;
        n->n = 0;

        MMALLOC(n->key, sizeof(tld_strbuf*) * n->n_alloc);
        MMALLOC(n->v, sizeof(tld_json_val*) * n->n_alloc);

        for(int i = 0; i < n->n_alloc;i++){
                tld_strbuf_alloc(&n->key[i], 16);
                tld_json_val_alloc(&n->v[i]);
                /* MMALLOC(n->v[i], sizeof(tld_json_val)); */
                /* n->v[i]->type = TLD_JSON_UNDEF; */
        }
        *node = n;
        return OK;
ERROR:
        return FAIL;
}

int tld_json_obj_chk_space(tld_json_obj *n)
{
        if(n->n == n->n_alloc){
                RUN(tld_json_obj_realloc(n));
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_json_obj_realloc(tld_json_obj *n)
{

        int old = n->n_alloc;
        n->n_alloc = n->n_alloc + n->n_alloc / 2;

        MREALLOC(n->key, sizeof(tld_strbuf*) * n->n_alloc);
        MREALLOC(n->v, sizeof(tld_json_val*) * n->n_alloc);

        for(int i = old; i < n->n_alloc;i++){
                tld_strbuf_alloc(&n->key[i], 16);
                tld_json_val_alloc(&n->v[i]);
                n->v[i]->type = TLD_JSON_UNDEF;
        }
        return OK;
ERROR:
        return FAIL;
}

void tld_json_obj_free(tld_json_obj *n)
{
        if(n){
                for(int i = 0 ;i < n->n_alloc;i++){
                        tld_json_val_free(n->v[i]);
                        tld_strbuf_free(n->key[i]);
                }

                MFREE(n->key);
                MFREE(n->v);
                MFREE(n);
        }
}

int tld_json_val_copy(tld_json_val *t, tld_json_val *s)
{
        ASSERT(t != NULL,"No target");
        ASSERT(s != NULL,"No source");


        switch(s->type){
        case TLD_JSON_OBJ:
                t->type = TLD_JSON_OBJ;
                tld_json_obj_copy(&t->value.obj, s->value.obj);

                /* ERROR_MSG("Node copy not implemented!"); */
                break;
        case TLD_JSON_ARR:
                tld_json_arr_copy(&t->value.arr, s->value.arr);
                /* ERROR_MSG("Copy of list not implemented. "); */
                break;
        case TLD_JSON_STR:
                if(t->type == TLD_JSON_UNDEF){
                        tld_strbuf_alloc(&t->value.str, 16);
                }
                tld_strbuf_copy(t->value.str, s->value.str);
                break;
        case TLD_JSON_DBL:
                t->value.d_num = s->value.d_num;
                break;
        case TLD_JSON_INT:
                t->value.i_num = s->value.i_num;
                break;
        case TLD_JSON_BOOL_TRUE:
        case TLD_JSON_BOOL_FALSE:
                t->value.b_num = s->value.b_num;
                break;

        case TLD_JSON_UNDEF:
                break;
        }
        t->type = s->type;
        return OK;
ERROR:
        return FAIL;
}


int tld_json_val_alloc(tld_json_val **node)
{
        tld_json_val* n = NULL;
        MMALLOC(n, sizeof(tld_json_val));
        n->type = TLD_JSON_UNDEF;
        *node = n;
        return OK;
ERROR:
        return FAIL;
}

void tld_json_val_free(tld_json_val* n)
{
        if(n){
                switch (n->type) {
                case TLD_JSON_DBL:
                case TLD_JSON_INT:
                case TLD_JSON_BOOL_TRUE:
                case TLD_JSON_BOOL_FALSE:
                        break;
                case TLD_JSON_STR:
                        tld_strbuf_free(n->value.str);
                        break;
                case TLD_JSON_ARR:
                        tld_json_arr_free(n->value.arr);
                        /* free space here !! */
                        break;
                case TLD_JSON_OBJ:
                        tld_json_obj_free(n->value.obj);
                        /* free object space here  */
                        break;
                case TLD_JSON_UNDEF:
                        break;
                }
                MFREE(n);
        }
}


/* <> arr copy must be implemented here  */
int tld_json_arr_copy(tld_json_arr **t, tld_json_arr *s)
{
        tld_json_arr* n = NULL;
        RUN(tld_json_arr_alloc(&n));
        for(int i = 0; i < s->n;i++){
                tld_json_val_copy(n->v[n->n] , s->v[i]);
                n->n++;
                tld_json_arr_chk_space(n);

        }
        *t = n;
        return OK;
ERROR:
        tld_json_arr_free(n);
        return FAIL;
}

int tld_json_arr_alloc(tld_json_arr **arr)
{
        tld_json_arr* n = NULL;
        MMALLOC(n, sizeof(tld_json_arr));
        n->n_alloc = 16;
        n->n = 0;
        n->read_head = 0;
        n->v = NULL;
        MMALLOC(n->v,sizeof(tld_json_val*) * n->n_alloc);
        for(int i = 0; i < n->n_alloc;i++){
                n->v[i] = NULL;
                RUN(tld_json_val_alloc(&n->v[i]));
                /* MMALLOC(n->v[i], sizeof(tld_json_val)); */
        }
        *arr = n;
        return OK;
ERROR:
        return FAIL;
}

int tld_json_arr_chk_space(tld_json_arr *n)
{
        if(n->n == n->n_alloc){
                RUN(tld_json_arr_realloc(n));
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_json_arr_realloc(tld_json_arr *n)
{
        int old_size = n->n_alloc;

        n->n_alloc = n->n_alloc + n->n_alloc / 2;

        MREALLOC(n->v,sizeof(tld_json_val*) * n->n_alloc);
        for(int i = old_size; i < n->n_alloc;i++){
                n->v[i] = NULL;
                RUN(tld_json_val_alloc(&n->v[i]));
                /* MMALLOC(n->v[i], sizeof(tld_json_val)); */
        }
        return OK;
ERROR:
        return FAIL;
}


void tld_json_arr_free(tld_json_arr *n)
{
        if(n){
                if(n->v){
                        for(int i = 0; i < n->n_alloc;i++){
                                tld_json_val_free(n->v[i]);
                        }
                        MFREE(n->v);
                }
                MFREE(n);
        }
}
