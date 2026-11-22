#include "../alloc/tld-alloc.h"
#include "../string/tld-string.h"

#include "tld-json.h"

#include "tld-json_mem.h"

int tld_json_obj_alloc(tld_json_obj **node)
{
        tld_json_obj *n = NULL;
        const int want = 4;
        ASSERT(node != NULL, "No output pointer");
        MMALLOC(n, sizeof(tld_json_obj));
        n->key = NULL;
        n->v = NULL;
        n->n_alloc = 0;         /* counts slots that really exist, so n can always be freed */
        n->n = 0;

        MMALLOC(n->key, sizeof(tld_strbuf*) * want);
        MMALLOC(n->v, sizeof(tld_json_val*) * want);

        while(n->n_alloc < want){
                int i = n->n_alloc;
                n->key[i] = NULL;
                n->v[i] = NULL;
                RUN(tld_strbuf_alloc(&n->key[i], 16));
                if(tld_json_val_alloc(&n->v[i]) != OK){
                        tld_strbuf_free(n->key[i]);
                        ERROR_MSG("Could not allocate a JSON object");
                }
                n->n_alloc++;
        }
        *node = n;
        return OK;
ERROR:
        if(n){
                tld_json_obj_free(n);
        }
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

int tld_json_obj_copy(tld_json_obj **target, tld_json_obj *s)
{
        tld_json_obj* t = NULL;
        ASSERT(s != NULL, "No source object");
        ASSERT(target != NULL, "No output pointer");
        RUN(tld_json_obj_alloc(&t));
        for(int i = 0; i < s->n;i++){
                RUN(tld_strbuf_append(t->key[i], TLD_STR(s->key[i])));
                RUN(tld_json_val_copy(t->v[i], s->v[i]));
                t->n++;
                RUN(tld__json_obj_chk_space(t));
        }

        *target = t;
        return OK;
ERROR:
        if(t){
                tld_json_obj_free(t);
        }
        return FAIL;
}

int tld__json_obj_chk_space(tld_json_obj *n)
{
        if(n->n == n->n_alloc){
                RUN(tld__json_obj_realloc(n));
        }
        return OK;
ERROR:
        return FAIL;
}

int tld__json_obj_realloc(tld_json_obj *n)
{
        /* Grow the arrays first; n_alloc only counts slots that really exist, so a
         * failure part-way leaves the object consistent (and freeable). */
        int target = n->n_alloc + n->n_alloc / 2 + 1;

        MREALLOC(n->key, sizeof(tld_strbuf*) * target);
        MREALLOC(n->v, sizeof(tld_json_val*) * target);

        while(n->n_alloc < target){
                int i = n->n_alloc;
                n->key[i] = NULL;
                n->v[i] = NULL;
                if(tld_strbuf_alloc(&n->key[i], 16) != OK){
                        ERROR_MSG("Could not grow JSON object");
                }
                if(tld_json_val_alloc(&n->v[i]) != OK){
                        tld_strbuf_free(n->key[i]);
                        n->key[i] = NULL;
                        ERROR_MSG("Could not grow JSON object");
                }
                n->v[i]->type = TLD_JSON_NULL;
                n->n_alloc++;
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_json_arr_alloc(tld_json_arr **arr)
{
        tld_json_arr* n = NULL;
        const int want = 16;
        ASSERT(arr != NULL, "No output pointer");
        MMALLOC(n, sizeof(tld_json_arr));
        n->n_alloc = 0;         /* counts slots that really exist, so n can always be freed */
        n->n = 0;
        n->read_head = 0;
        n->v = NULL;
        MMALLOC(n->v,sizeof(tld_json_val*) * want);
        while(n->n_alloc < want){
                n->v[n->n_alloc] = NULL;
                RUN(tld_json_val_alloc(&n->v[n->n_alloc]));
                n->n_alloc++;
        }
        *arr = n;
        return OK;
ERROR:
        if(n){
                tld_json_arr_free(n);
        }
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

/* <> arr copy must be implemented here  */
int tld_json_arr_copy(tld_json_arr **t, tld_json_arr *s)
{
        tld_json_arr* n = NULL;
        ASSERT(s != NULL, "No source array");
        ASSERT(t != NULL, "No output pointer");
        RUN(tld_json_arr_alloc(&n));
        for(int i = 0; i < s->n;i++){
                RUN(tld_json_val_copy(n->v[n->n] , s->v[i]));
                n->n++;
                RUN(tld__json_arr_chk_space(n));
        }
        *t = n;
        return OK;
ERROR:
        tld_json_arr_free(n);
        return FAIL;
}

int tld__json_arr_chk_space(tld_json_arr *n)
{
        if(n->n == n->n_alloc){
                RUN(tld__json_arr_realloc(n));
        }
        return OK;
ERROR:
        return FAIL;
}

int tld__json_arr_realloc(tld_json_arr *n)
{
        /* As for objects: n_alloc only counts slots that exist. */
        int target = n->n_alloc + n->n_alloc / 2 + 1;

        MREALLOC(n->v, sizeof(tld_json_val*) * target);
        while(n->n_alloc < target){
                int i = n->n_alloc;
                n->v[i] = NULL;
                RUN(tld_json_val_alloc(&n->v[i]));
                n->n_alloc++;
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_json_val_alloc(tld_json_val **node)
{
        tld_json_val* n = NULL;
        ASSERT(node != NULL, "No output pointer");
        MMALLOC(n, sizeof(tld_json_val));
        n->type = TLD_JSON_NULL;
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
                        break;
                case TLD_JSON_OBJ:
                        tld_json_obj_free(n->value.obj);
                        break;
                case TLD_JSON_NULL:
                        break;
                }
                MFREE(n);
        }
}

/* Release whatever v owns and mark it empty. */
static void tld__json_val_clear(tld_json_val *v)
{
        switch (v->type) {
        case TLD_JSON_STR:
                tld_strbuf_free(v->value.str);
                break;
        case TLD_JSON_ARR:
                tld_json_arr_free(v->value.arr);
                break;
        case TLD_JSON_OBJ:
                tld_json_obj_free(v->value.obj);
                break;
        default:
                break;
        }
        v->type = TLD_JSON_NULL;
}

/* Deep copy s into t. Whatever t held before is released, so t may hold a
   value of another type. The copy is built first, so s may live inside t. */
int tld_json_val_copy(tld_json_val *t, tld_json_val *s)
{
        tld_json_val tmp;
        ASSERT(t != NULL,"No target");
        ASSERT(s != NULL,"No source");

        if(t == s){
                return OK;
        }
        tmp.type = s->type;
        switch(s->type){
        case TLD_JSON_OBJ:
                tmp.value.obj = NULL;
                if(s->value.obj){
                        RUN(tld_json_obj_copy(&tmp.value.obj, s->value.obj));
                }
                break;
        case TLD_JSON_ARR:
                tmp.value.arr = NULL;
                if(s->value.arr){
                        RUN(tld_json_arr_copy(&tmp.value.arr, s->value.arr));
                }
                break;
        case TLD_JSON_STR:
                tmp.value.str = NULL;
                if(s->value.str){
                        RUN(tld_strbuf_alloc(&tmp.value.str, s->value.str->len + 1));
                        RUN(tld_strbuf_copy(tmp.value.str, s->value.str));
                }
                break;
        case TLD_JSON_DBL:
                tmp.value.d_num = s->value.d_num;
                break;
        case TLD_JSON_INT:
                tmp.value.i_num = s->value.i_num;
                break;
        case TLD_JSON_BOOL_TRUE:
        case TLD_JSON_BOOL_FALSE:
                tmp.value.b_num = s->value.b_num;
                break;
        case TLD_JSON_NULL:
                break;
        }
        tld__json_val_clear(t);
        *t = tmp;
        return OK;
ERROR:
        return FAIL;
}
