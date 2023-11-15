
#include "../alloc/tld-alloc.h"

#include "tld-json.h"

#define TLD_JSON_MEM_IMPORT
#include "tld-json_mem.h"


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
