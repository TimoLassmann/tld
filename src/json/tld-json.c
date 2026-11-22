
#include "../alloc/tld-alloc.h"
#include "../string/str.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "tld-json_struct.h"
#include "tld-json_mem.h"
#include "tld-json_misc.h"

#define TLD_JSON_IMPORT
#include "tld-json.h"

/* Maximum nesting depth of objects / arrays accepted by the parser. */
#define TLD_JSON_MAX_DEPTH 512

/* Key under which tld_json_parse stores a top-level array. */
#define TLD_JSON_TOP_LEVEL_ARR "Top Level Arr"

typedef struct json_parser {
        const uint8_t* s;
        int len;
        int pos;
        int depth;
        /* scratch buffer for decoded strings */
        char* buf;
        int b_len;
        int b_alloc;
        const char* err;
} json_parser;

static int tld_json_dump_val(tld_json_val *val, tld_strbuf *b);
static int tld_json_dump_str(tld_strbuf *b, const uint8_t *s, int len);
static int tld_json_add_indent(tld_strbuf *b, int indent);
static int tld_json_get_arr_str(tld_json_arr *n, char *key, tld_json_obj *res);


static int add_val(char *buf, int len, tld_json_val *val);
static int json_val_set_str(tld_json_val *v, const char *s, int len);
static void json_val_set_int64(tld_json_val *v, long long x);
static int json_val_from_decoration(tld_json_val *v, tld_json_decoration *d);

static int print_val(tld_json_val *val, FILE *out);
static int search(tld_json_obj *n, char *key, tld_json_obj* r);

/* parser */
static int jp_fail(json_parser *p, const char *msg);
static void jp_ws(json_parser *p);
static int jp_push(json_parser *p, uint8_t c);
static int jp_push_utf8(json_parser *p, uint32_t cp);
static int jp_hex4(json_parser *p, uint32_t *cp);
static int jp_string(json_parser *p);
static int jp_number(json_parser *p, tld_json_val *v);
static int jp_literal(json_parser *p, const char *lit);
static int jp_value(json_parser *p, tld_json_val *v);
static int jp_obj(json_parser *p, tld_json_obj **out);
static int jp_arr(json_parser *p, tld_json_arr **out);

/* path getters */
static int json_path_resolve(tld_json_obj *root, const char *path, tld_json_val **out);

static tld_json_obj* tld_json_obj_add_val(tld_json_obj *obj, char* key, char* val);
static tld_json_obj* tld_json_obj_add_obj(tld_json_obj *obj, char* key, tld_json_obj*val);
static tld_json_obj *tld_json_obj_add_arr(tld_json_obj *obj, char *key, tld_json_arr *val);
static tld_json_obj *tld_json_obj_add_typed(tld_json_obj *obj, char *key, tld_json_decoration *d);
static tld_json_arr* tld_json_arr_add_val(tld_json_arr *arr, char* val);
static tld_json_arr* tld_json_arr_add_arr(tld_json_arr *arr, tld_json_arr*val);
static tld_json_arr *tld_json_arr_add_obj(tld_json_arr *arr, tld_json_obj *val);
static tld_json_arr *tld_json_arr_add_typed(tld_json_arr *arr, tld_json_decoration *d);

static void json_free_unattached(tld_json_decoration *kvs, int from, int count)
{
        for(int k = from; k < count; k++){
                if(kvs[k].value == NULL){
                        continue;
                }
                if(kvs[k].type == VAL_OBJ){
                        tld_json_obj_free((tld_json_obj*)kvs[k].value);
                }else if(kvs[k].type == VAL_ARR){
                        tld_json_arr_free((tld_json_arr*)kvs[k].value);
                }
        }
}

/* JSON_OBJ / JSON_ARR back ends. They stop at the first entry that cannot be added. A
 * container created by the call is then freed (with the children of entries not yet
 * attached, which were built for this call) and NULL is returned; a caller-supplied
 * container is returned holding the entries added so far. */
tld_json_obj *json_obj_add_multiple(tld_json_obj *obj, tld_json_decoration *kvs, int count)
{
        tld_json_obj *in = obj;
        for (int i = 0; i < count; i++) {
                int before = obj ? obj->n : 0;
                if(kvs[i].value == NULL){
                        continue;
                }
                switch (kvs[i].type) {
                case VAL_STRING:
                        obj = tld_json_obj_add_val(obj, kvs[i].key, (char*)kvs[i].value);
                        break;
                case VAL_OBJ:
                        obj = tld_json_obj_add_obj(obj, kvs[i].key, (tld_json_obj*)kvs[i].value);
                        break;
                case VAL_ARR:
                        obj = tld_json_obj_add_arr(obj, kvs[i].key, (tld_json_arr*)kvs[i].value);
                        break;
                case VAL_JSTR:
                case VAL_NUM:
                case VAL_INT:
                case VAL_BOOL:
                case VAL_NULL:
                        obj = tld_json_obj_add_typed(obj, kvs[i].key, &kvs[i]);
                        break;
                }
                if(obj == NULL || obj->n != before + 1){
                        if(in == NULL){
                                json_free_unattached(kvs, i, count);
                                if(obj != NULL){
                                        tld_json_obj_free(obj);
                                }
                        }
                        return in;
                }
        }
        return obj;
}

/* Make sure obj (allocated here when NULL) has a free slot at index n BEFORE anything is
 * written to it. On failure an object allocated here is freed. */
static int json_obj_slot(tld_json_obj *obj, tld_json_obj **np)
{
        tld_json_obj *n = obj;
        if(n == NULL){
                RUN(tld_json_obj_alloc(&n));
        }
        if(n->n >= n->n_alloc){
                RUN(tld_json_obj_realloc(n));
        }
        *np = n;
        return OK;
ERROR:
        if(obj == NULL && n){
                tld_json_obj_free(n);
        }
        *np = NULL;
        return FAIL;
}

static int json_arr_slot(tld_json_arr *arr, tld_json_arr **np)
{
        tld_json_arr *n = arr;
        if(n == NULL){
                RUN(tld_json_arr_alloc(&n));
        }
        if(n->n >= n->n_alloc){
                RUN(tld_json_arr_realloc(n));
        }
        *np = n;
        return OK;
ERROR:
        if(arr == NULL && n){
                tld_json_arr_free(n);
        }
        *np = NULL;
        return FAIL;
}

/* The add helpers return the (possibly new) container. When an entry cannot be added it
 * is dropped; a container allocated by the call is freed and NULL returned. */
tld_json_obj* tld_json_obj_add_val(tld_json_obj *obj, char* key, char* val)
{
        tld_json_obj* n = NULL;
        RUN(json_obj_slot(obj, &n));
        tld_strbuf_clear(n->key[n->n]);
        RUN(tld_append(n->key[n->n], key ? key : ""));
        RUN(add_val(val, (int)strlen(val), n->v[n->n]));
        n->n++;
        return n;
ERROR:
        if(obj == NULL && n){
                tld_json_obj_free(n);
                return NULL;
        }
        return obj;
}

tld_json_obj* tld_json_obj_add_obj(tld_json_obj *obj, char* key, tld_json_obj*val)
{
        tld_json_obj* n = NULL;
        RUN(json_obj_slot(obj, &n));
        tld_strbuf_clear(n->key[n->n]);
        RUN(tld_append(n->key[n->n], key ? key : ""));
        n->v[n->n]->type = TLD_JSON_OBJ;
        n->v[n->n]->value.obj = val;
        n->n++;
        return n;
ERROR:
        if(obj == NULL && n){
                tld_json_obj_free(n);
                return NULL;
        }
        return obj;
}

tld_json_obj* tld_json_obj_add_arr(tld_json_obj *obj, char* key, tld_json_arr*val)
{
        tld_json_obj* n = NULL;
        RUN(json_obj_slot(obj, &n));
        tld_strbuf_clear(n->key[n->n]);
        RUN(tld_append(n->key[n->n], key ? key : ""));
        n->v[n->n]->type = TLD_JSON_ARR;
        n->v[n->n]->value.arr = val;
        n->n++;
        return n;
ERROR:
        if(obj == NULL && n){
                tld_json_obj_free(n);
                return NULL;
        }
        return obj;
}

tld_json_obj *tld_json_obj_add_typed(tld_json_obj *obj, char *key, tld_json_decoration *d)
{
        tld_json_obj* n = NULL;
        RUN(json_obj_slot(obj, &n));
        tld_strbuf_clear(n->key[n->n]);
        RUN(tld_append(n->key[n->n], key ? key : ""));
        RUN(json_val_from_decoration(n->v[n->n], d));
        n->n++;
        return n;
ERROR:
        if(obj == NULL && n){
                tld_json_obj_free(n);
                return NULL;
        }
        return obj;
}

tld_json_arr *json_arr_add_multiple(tld_json_arr *arr, tld_json_decoration *kvs, int count)
{
        tld_json_arr *in = arr;
        for (int i = 0; i < count; i++) {
                int before = arr ? arr->n : 0;
                if(kvs[i].value == NULL){
                        continue;
                }
                switch (kvs[i].type) {
                case VAL_STRING:
                        arr = tld_json_arr_add_val(arr,  (char*)kvs[i].value);
                        break;
                case VAL_OBJ:
                        arr = tld_json_arr_add_obj(arr, (tld_json_obj*)kvs[i].value);
                        break;
                case VAL_ARR:
                        arr = tld_json_arr_add_arr(arr,  (tld_json_arr*)kvs[i].value);
                        break;
                case VAL_JSTR:
                case VAL_NUM:
                case VAL_INT:
                case VAL_BOOL:
                case VAL_NULL:
                        arr = tld_json_arr_add_typed(arr, &kvs[i]);
                        break;
                }
                if(arr == NULL || arr->n != before + 1){
                        if(in == NULL){
                                json_free_unattached(kvs, i, count);
                                if(arr != NULL){
                                        tld_json_arr_free(arr);
                                }
                        }
                        return in;
                }
        }
        return arr;
}

tld_json_arr* tld_json_arr_add_val(tld_json_arr *arr, char* val)
{
        tld_json_arr* n = NULL;
        RUN(json_arr_slot(arr, &n));
        RUN(add_val(val, (int)strlen(val), n->v[n->n]));
        n->n++;
        return n;
ERROR:
        if(arr == NULL && n){
                tld_json_arr_free(n);
                return NULL;
        }
        return arr;
}

tld_json_arr* tld_json_arr_add_arr(tld_json_arr *arr, tld_json_arr*val)
{
        tld_json_arr* n = NULL;
        RUN(json_arr_slot(arr, &n));
        n->v[n->n]->type = TLD_JSON_ARR;
        n->v[n->n]->value.arr = val;
        n->n++;
        return n;
ERROR:
        if(arr == NULL && n){
                tld_json_arr_free(n);
                return NULL;
        }
        return arr;
}

tld_json_arr* tld_json_arr_add_obj(tld_json_arr *arr, tld_json_obj*val)
{
        tld_json_arr* n = NULL;
        RUN(json_arr_slot(arr, &n));
        n->v[n->n]->type = TLD_JSON_OBJ;
        n->v[n->n]->value.obj = val;
        n->n++;
        return n;
ERROR:
        if(arr == NULL && n){
                tld_json_arr_free(n);
                return NULL;
        }
        return arr;
}

tld_json_arr *tld_json_arr_add_typed(tld_json_arr *arr, tld_json_decoration *d)
{
        tld_json_arr* n = NULL;
        RUN(json_arr_slot(arr, &n));
        RUN(json_val_from_decoration(n->v[n->n], d));
        n->n++;
        return n;
ERROR:
        if(arr == NULL && n){
                tld_json_arr_free(n);
                return NULL;
        }
        return arr;
}

/* Fill an empty (TLD_JSON_UNDEF) slot from one of the typed decorations. */
int json_val_from_decoration(tld_json_val *v, tld_json_decoration *d)
{
        switch(d->type){
        case VAL_JSTR:{
                const char* s = (const char*)d->value;
                RUN(json_val_set_str(v, s, (int)strlen(s)));
                break;
        }
        case VAL_NUM:
                v->type = TLD_JSON_DBL;
                v->value.d_num = *(double*)d->value;
                break;
        case VAL_INT:
                json_val_set_int64(v, *(long*)d->value);
                break;
        case VAL_BOOL:
                if(*(int*)d->value){
                        v->type = TLD_JSON_BOOL_TRUE;
                        v->value.b_num = 1;
                }else{
                        v->type = TLD_JSON_BOOL_FALSE;
                        v->value.b_num = 0;
                }
                break;
        case VAL_NULL:
                v->type = TLD_JSON_UNDEF;
                break;
        default:
                ERROR_MSG("Unexpected decoration type %d", (int)d->type);
                break;
        }
        return OK;
ERROR:
        return FAIL;
}

/* Set the value at path (same syntax as the tld_json_get_* getters) to a deep copy of v.
   The last step may name a missing key (it is added) or the index one past the end of an
   array (the value is appended); everything before it must exist. */
int tld_json_set_val(tld_json_obj *n, char *key, tld_json_val *v)
{
        const char* path = key;
        const char* last = NULL;
        tld_json_val* pv = NULL;
        tld_json_obj* po = NULL;
        tld_json_arr* pa = NULL;
        char* prefix = NULL;
        size_t plen;

        ASSERT(n != NULL, "No JSON object");
        ASSERT(path != NULL && *path, "No path");
        ASSERT(v != NULL, "No value");

        /* the last step starts at the last '.' or '[' (keys contain neither) */
        for(const char* c = path; *c; c++){
                if(*c == '.' || *c == '['){
                        last = c;
                }
        }
        if(last == NULL){
                last = path;            /* a single key */
        }
        plen = (size_t)(last - path);

        if(plen == 0 && *last == '['){
                /* "[i]": the array that tld_json_parse wraps under "Top Level Arr" */
                for(int i = 0; i < n->n; i++){
                        if(strcmp(TLD_STR(n->key[i]), TLD_JSON_TOP_LEVEL_ARR) == 0){
                                pv = n->v[i];
                                break;
                        }
                }
                if(pv == NULL || pv->type != TLD_JSON_ARR){
                        ERROR_MSG("Path %s: no top-level array", path);
                }
                pa = pv->value.arr;
        }else if(plen == 0){
                po = n;
        }else{
                MMALLOC(prefix, plen + 1);
                memcpy(prefix, path, plen);
                prefix[plen] = 0;
                if(json_path_resolve(n, prefix, &pv) != OK || pv == NULL){
                        ERROR_MSG("Path %s: %s does not exist", path, prefix);
                }
                if(pv->type == TLD_JSON_OBJ){
                        po = pv->value.obj;
                }else if(pv->type == TLD_JSON_ARR){
                        pa = pv->value.arr;
                }
                MFREE(prefix);
        }

        if(*last == '['){
                long idx = 0;
                int digits = 0;
                const char* c = last + 1;
                while(*c >= '0' && *c <= '9' && digits < 9){
                        idx = idx * 10 + (*c - '0');
                        digits++;
                        c++;
                }
                if(!digits || *c != ']' || c[1] != 0){
                        ERROR_MSG("Path %s: bad array index", path);
                }
                if(pa == NULL){
                        ERROR_MSG("Path %s: not an array", path);
                }
                if(idx < pa->n){
                        RUN(tld_json_val_copy(pa->v[idx], v));
                }else if(idx == pa->n){
                        tld_json_arr* r = NULL;
                        RUN(json_arr_slot(pa, &r));
                        RUN(tld_json_val_copy(pa->v[pa->n], v));
                        pa->n++;
                }else{
                        ERROR_MSG("Path %s: index %ld is past the end (%d entries)", path, idx, pa->n);
                }
                return OK;
        }

        {
                const char* k = (*last == '.') ? last + 1 : last;
                int klen = (int)strlen(k);
                tld_json_obj* r = NULL;
                if(klen == 0){
                        ERROR_MSG("Path %s: empty key", path);
                }
                if(po == NULL){
                        ERROR_MSG("Path %s: not an object", path);
                }
                for(int i = 0; i < po->n; i++){
                        if(po->key[i]->len == klen && memcmp(po->key[i]->str, k, klen) == 0){
                                RUN(tld_json_val_copy(po->v[i], v));
                                return OK;
                        }
                }
                RUN(json_obj_slot(po, &r));
                tld_strbuf_clear(po->key[po->n]);
                RUN(tld_append(po->key[po->n], (char*)k));
                RUN(tld_json_val_copy(po->v[po->n], v));
                po->n++;
        }
        return OK;
ERROR:
        MFREE(prefix);
        return FAIL;
}

int tld_json_search(tld_json_obj *n, char *key, tld_json_obj **ret)
{
        tld_json_obj* r = NULL;
        RUN(tld_json_obj_alloc(&r));
        RUN(search(n, key, r));
        *ret = r;
        return OK;
ERROR:
        if(r){
                tld_json_obj_free(r);
        }
        return FAIL;
}

int search(tld_json_obj *n, char *key, tld_json_obj* r)
{
        if(n){
                for(int i = 0; i < n->n;i++){
                        int len = strnlen(key, 256);
                        if(len == n->key[i]->len){
                                if(strncmp((char*)n->key[i]->str, key, n->key[i]->len)==0){
                                        RUN(tld_append(r->key[r->n], TLD_STR(n->key[i])));
                                        RUN(tld_json_val_copy(r->v[r->n], n->v[i]));
                                        r->n++;
                                        RUN(tld_json_obj_chk_space(r));
                                        /* keep scanning the siblings (was
                                           'break', which missed matches in
                                           later siblings) */
                                        continue;
                                }
                        }

                        if(n->v[i]->type == TLD_JSON_ARR){
                                RUN(tld_json_get_arr_str(n->v[i]->value.arr,key,r));
                        }else if(n->v[i]->type == TLD_JSON_OBJ){
                                RUN(search(n->v[i]->value.obj, key,r));
                        }
                }
        }

        return OK;
ERROR:
        return FAIL;
}

int tld_json_get_arr_str(tld_json_arr *n, char *key, tld_json_obj* res)
{
        if(n){
                for(int i = 0; i < n->n;i++){
                        if(n->v[i]->type == TLD_JSON_ARR){
                                RUN(tld_json_get_arr_str(n->v[i]->value.arr,key,res));
                        }else if(n->v[i]->type == TLD_JSON_OBJ){
                                RUN(search(n->v[i]->value.obj, key,res));
                        }
                }
        }
        return OK;
ERROR:
        return FAIL;
}

/* ------------------------------------------------------------------ */
/* Parser: recursive descent directly over the input text.             */
/* ------------------------------------------------------------------ */

int tld_json_parse(tld_strbuf *t, tld_json_obj **out)
{
        json_parser p;
        tld_json_obj* n = NULL;
        tld_json_arr* a = NULL;

        p.s = NULL;
        p.len = 0;
        p.pos = 0;
        p.depth = 0;
        p.buf = NULL;
        p.b_len = 0;
        p.b_alloc = 0;
        p.err = NULL;

        ASSERT(out != NULL, "No output pointer");
        ASSERT(t != NULL && t->str != NULL, "No input");

        p.s = t->str;
        p.len = t->len;

        p.b_alloc = 256;
        MMALLOC(p.buf, p.b_alloc);

        jp_ws(&p);
        if(p.pos >= p.len){
                jp_fail(&p, "empty input");
                goto PARSE_ERROR;
        }
        if(p.s[p.pos] == '{'){
                if(jp_obj(&p, &n) != OK){
                        goto PARSE_ERROR;
                }
        }else if(p.s[p.pos] == '['){
                if(jp_arr(&p, &a) != OK){
                        goto PARSE_ERROR;
                }
                RUN(tld_json_obj_alloc(&n));
                RUN(tld_append(n->key[0], TLD_JSON_TOP_LEVEL_ARR));
                n->v[0]->type = TLD_JSON_ARR;
                n->v[0]->value.arr = a;
                a = NULL;
                n->n++;
                RUN(tld_json_obj_chk_space(n));
        }else{
                jp_fail(&p, "text does not start with { or [");
                goto PARSE_ERROR;
        }
        jp_ws(&p);
        if(p.pos < p.len){
                jp_fail(&p, "trailing characters after JSON value");
                goto PARSE_ERROR;
        }

        MFREE(p.buf);
        *out = n;
        return OK;
PARSE_ERROR:
        ERROR_MSG("JSON parse error at byte %d: %s", p.pos, p.err ? p.err : "unknown error");
ERROR:
        if(a){
                tld_json_arr_free(a);
        }
        if(n){
                tld_json_obj_free(n);
        }
        if(p.buf){
                MFREE(p.buf);
        }
        return FAIL;
}

int jp_fail(json_parser *p, const char *msg)
{
        if(p->err == NULL){
                p->err = msg;
        }
        return FAIL;
}

void jp_ws(json_parser *p)
{
        while(p->pos < p->len){
                uint8_t c = p->s[p->pos];
                if(c == ' ' || c == '\t' || c == '\n' || c == '\r'){
                        p->pos++;
                }else{
                        break;
                }
        }
}

/* Append one byte to the scratch buffer, always leaving room for a
   terminating 0. */
int jp_push(json_parser *p, uint8_t c)
{
        if(p->b_len + 1 >= p->b_alloc){
                int new_alloc = p->b_alloc + p->b_alloc / 2 + 16;
                MREALLOC(p->buf, new_alloc);
                p->b_alloc = new_alloc;
        }
        p->buf[p->b_len] = (char)c;
        p->b_len++;
        return OK;
ERROR:
        return FAIL;
}

int jp_push_utf8(json_parser *p, uint32_t cp)
{
        if(cp < 0x80){
                RUN(jp_push(p, (uint8_t)cp));
        }else if(cp < 0x800){
                RUN(jp_push(p, (uint8_t)(0xC0 | (cp >> 6))));
                RUN(jp_push(p, (uint8_t)(0x80 | (cp & 0x3F))));
        }else if(cp < 0x10000){
                RUN(jp_push(p, (uint8_t)(0xE0 | (cp >> 12))));
                RUN(jp_push(p, (uint8_t)(0x80 | ((cp >> 6) & 0x3F))));
                RUN(jp_push(p, (uint8_t)(0x80 | (cp & 0x3F))));
        }else{
                RUN(jp_push(p, (uint8_t)(0xF0 | (cp >> 18))));
                RUN(jp_push(p, (uint8_t)(0x80 | ((cp >> 12) & 0x3F))));
                RUN(jp_push(p, (uint8_t)(0x80 | ((cp >> 6) & 0x3F))));
                RUN(jp_push(p, (uint8_t)(0x80 | (cp & 0x3F))));
        }
        return OK;
ERROR:
        return FAIL;
}

int jp_hex4(json_parser *p, uint32_t *cp)
{
        uint32_t v = 0;
        if(p->pos + 4 > p->len){
                return jp_fail(p, "truncated \\u escape");
        }
        for(int i = 0; i < 4; i++){
                uint8_t c = p->s[p->pos + i];
                v <<= 4;
                if(c >= '0' && c <= '9'){
                        v |= (uint32_t)(c - '0');
                }else if(c >= 'a' && c <= 'f'){
                        v |= (uint32_t)(c - 'a' + 10);
                }else if(c >= 'A' && c <= 'F'){
                        v |= (uint32_t)(c - 'A' + 10);
                }else{
                        return jp_fail(p, "invalid hex digit in \\u escape");
                }
        }
        p->pos += 4;
        *cp = v;
        return OK;
}

/* Parse a quoted string starting at p->s[p->pos] == '"'. The decoded UTF-8
   bytes end up in p->buf (p->b_len bytes, 0-terminated). */
int jp_string(json_parser *p)
{
        p->pos++;               /* opening quote */
        p->b_len = 0;
        while(1){
                uint8_t c;
                if(p->pos >= p->len){
                        return jp_fail(p, "unterminated string");
                }
                c = p->s[p->pos];
                p->pos++;
                if(c == '"'){
                        break;
                }else if(c == '\\'){
                        uint8_t e;
                        if(p->pos >= p->len){
                                return jp_fail(p, "unterminated string");
                        }
                        e = p->s[p->pos];
                        p->pos++;
                        switch(e){
                        case '"':  if(jp_push(p, '"'))  return FAIL; break;
                        case '\\': if(jp_push(p, '\\')) return FAIL; break;
                        case '/':  if(jp_push(p, '/'))  return FAIL; break;
                        case 'b':  if(jp_push(p, '\b')) return FAIL; break;
                        case 'f':  if(jp_push(p, '\f')) return FAIL; break;
                        case 'n':  if(jp_push(p, '\n')) return FAIL; break;
                        case 'r':  if(jp_push(p, '\r')) return FAIL; break;
                        case 't':  if(jp_push(p, '\t')) return FAIL; break;
                        case 'u':{
                                uint32_t cp = 0;
                                if(jp_hex4(p, &cp) != OK){
                                        return FAIL;
                                }
                                if(cp >= 0xD800 && cp <= 0xDBFF){
                                        /* high surrogate: needs a following low surrogate */
                                        uint32_t lo = 0;
                                        if(p->pos + 6 <= p->len &&
                                           p->s[p->pos] == '\\' &&
                                           p->s[p->pos+1] == 'u'){
                                                int save = p->pos;
                                                p->pos += 2;
                                                if(jp_hex4(p, &lo) != OK){
                                                        return FAIL;
                                                }
                                                if(lo >= 0xDC00 && lo <= 0xDFFF){
                                                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                                                }else{
                                                        /* not a pair: emit replacement, re-read the next escape */
                                                        p->pos = save;
                                                        cp = 0xFFFD;
                                                }
                                        }else{
                                                cp = 0xFFFD;
                                        }
                                }else if(cp >= 0xDC00 && cp <= 0xDFFF){
                                        /* lone low surrogate */
                                        cp = 0xFFFD;
                                }
                                if(cp == 0){
                                        return jp_fail(p, "\\u0000 is not supported in strings");
                                }
                                if(jp_push_utf8(p, cp) != OK){
                                        return FAIL;
                                }
                                break;
                        }
                        default:
                                return jp_fail(p, "invalid escape sequence");
                        }
                }else if(c == 0){
                        return jp_fail(p, "NUL byte in string");
                }else{
                        /* Raw bytes, including (leniently) unescaped control
                           characters, are copied through. */
                        if(jp_push(p, c) != OK){
                                return FAIL;
                        }
                }
        }
        p->buf[p->b_len] = 0;
        return OK;
}

int jp_number(json_parser *p, tld_json_val *v)
{
        char tmp[64];
        char* num = tmp;
        char* heap = NULL;
        int start = p->pos;
        int is_int = 1;
        int len;

        if(p->pos < p->len && p->s[p->pos] == '-'){
                p->pos++;
        }
        if(p->pos < p->len && p->s[p->pos] == '0'){
                p->pos++;
        }else if(p->pos < p->len && p->s[p->pos] >= '1' && p->s[p->pos] <= '9'){
                while(p->pos < p->len && p->s[p->pos] >= '0' && p->s[p->pos] <= '9'){
                        p->pos++;
                }
        }else{
                return jp_fail(p, "invalid number");
        }
        if(p->pos < p->len && p->s[p->pos] == '.'){
                int d = 0;
                is_int = 0;
                p->pos++;
                while(p->pos < p->len && p->s[p->pos] >= '0' && p->s[p->pos] <= '9'){
                        p->pos++;
                        d++;
                }
                if(!d){
                        return jp_fail(p, "invalid number: no digits after '.'");
                }
        }
        if(p->pos < p->len && (p->s[p->pos] == 'e' || p->s[p->pos] == 'E')){
                int d = 0;
                is_int = 0;
                p->pos++;
                if(p->pos < p->len && (p->s[p->pos] == '+' || p->s[p->pos] == '-')){
                        p->pos++;
                }
                while(p->pos < p->len && p->s[p->pos] >= '0' && p->s[p->pos] <= '9'){
                        p->pos++;
                        d++;
                }
                if(!d){
                        return jp_fail(p, "invalid number: no digits in exponent");
                }
        }

        len = p->pos - start;
        if(len >= (int)sizeof(tmp)){
                MMALLOC(heap, len + 1);
                num = heap;
        }
        memcpy(num, p->s + start, len);
        num[len] = 0;

        if(is_int){
                long long x;
                errno = 0;
                x = strtoll(num, NULL, 10);
                if(errno == ERANGE){
                        v->type = TLD_JSON_DBL;
                        v->value.d_num = strtod(num, NULL);
                }else{
                        json_val_set_int64(v, x);
                }
        }else{
                v->type = TLD_JSON_DBL;
                v->value.d_num = strtod(num, NULL);
        }
        if(heap){
                MFREE(heap);
        }
        return OK;
ERROR:
        return jp_fail(p, "out of memory");
}

int jp_literal(json_parser *p, const char *lit)
{
        int l = (int)strlen(lit);
        if(p->pos + l > p->len || memcmp(p->s + p->pos, lit, l) != 0){
                return jp_fail(p, "invalid literal");
        }
        p->pos += l;
        return OK;
}

/* Parse one value into the empty slot v. On failure v is left untouched
   (TLD_JSON_UNDEF) and anything allocated on the way is freed. */
int jp_value(json_parser *p, tld_json_val *v)
{
        uint8_t c;
        jp_ws(p);
        if(p->pos >= p->len){
                return jp_fail(p, "unexpected end of input");
        }
        c = p->s[p->pos];
        switch(c){
        case '{':{
                tld_json_obj* o = NULL;
                if(jp_obj(p, &o) != OK){
                        return FAIL;
                }
                v->type = TLD_JSON_OBJ;
                v->value.obj = o;
                break;
        }
        case '[':{
                tld_json_arr* a = NULL;
                if(jp_arr(p, &a) != OK){
                        return FAIL;
                }
                v->type = TLD_JSON_ARR;
                v->value.arr = a;
                break;
        }
        case '"':
                if(jp_string(p) != OK){
                        return FAIL;
                }
                if(json_val_set_str(v, p->buf, p->b_len) != OK){
                        return jp_fail(p, "out of memory");
                }
                break;
        case 't':
                if(jp_literal(p, "true") != OK){
                        return FAIL;
                }
                v->type = TLD_JSON_BOOL_TRUE;
                v->value.b_num = 1;
                break;
        case 'f':
                if(jp_literal(p, "false") != OK){
                        return FAIL;
                }
                v->type = TLD_JSON_BOOL_FALSE;
                v->value.b_num = 0;
                break;
        case 'n':
                if(jp_literal(p, "null") != OK){
                        return FAIL;
                }
                v->type = TLD_JSON_UNDEF;
                break;
        default:
                if(c == '-' || (c >= '0' && c <= '9')){
                        return jp_number(p, v);
                }
                return jp_fail(p, "unexpected character");
        }
        return OK;
}

int jp_obj(json_parser *p, tld_json_obj **out)
{
        tld_json_obj* n = NULL;

        if(p->depth >= TLD_JSON_MAX_DEPTH){
                return jp_fail(p, "nesting too deep");
        }
        p->depth++;
        p->pos++;               /* '{' */

        if(tld_json_obj_alloc(&n) != OK){
                jp_fail(p, "out of memory");
                goto FAIL_OBJ;
        }
        jp_ws(p);
        if(p->pos < p->len && p->s[p->pos] == '}'){
                p->pos++;
                goto DONE;
        }
        while(1){
                jp_ws(p);
                if(p->pos >= p->len){
                        jp_fail(p, "unexpected end of input in object");
                        goto FAIL_OBJ;
                }
                if(p->s[p->pos] != '"'){
                        jp_fail(p, "expected string as object key");
                        goto FAIL_OBJ;
                }
                if(jp_string(p) != OK){
                        goto FAIL_OBJ;
                }
                if(tld_append(n->key[n->n], p->buf) != OK){
                        jp_fail(p, "out of memory");
                        goto FAIL_OBJ;
                }
                jp_ws(p);
                if(p->pos >= p->len || p->s[p->pos] != ':'){
                        jp_fail(p, "expected ':'");
                        goto FAIL_OBJ;
                }
                p->pos++;
                if(jp_value(p, n->v[n->n]) != OK){
                        goto FAIL_OBJ;
                }
                n->n++;
                if(tld_json_obj_chk_space(n) != OK){
                        jp_fail(p, "out of memory");
                        goto FAIL_OBJ;
                }
                jp_ws(p);
                if(p->pos >= p->len){
                        jp_fail(p, "unexpected end of input in object");
                        goto FAIL_OBJ;
                }
                if(p->s[p->pos] == ','){
                        p->pos++;
                }else if(p->s[p->pos] == '}'){
                        p->pos++;
                        break;
                }else{
                        jp_fail(p, "expected ',' or '}'");
                        goto FAIL_OBJ;
                }
        }
DONE:
        p->depth--;
        *out = n;
        return OK;
FAIL_OBJ:
        p->depth--;
        if(n){
                tld_json_obj_free(n);
        }
        return FAIL;
}

int jp_arr(json_parser *p, tld_json_arr **out)
{
        tld_json_arr* n = NULL;

        if(p->depth >= TLD_JSON_MAX_DEPTH){
                return jp_fail(p, "nesting too deep");
        }
        p->depth++;
        p->pos++;               /* '[' */

        if(tld_json_arr_alloc(&n) != OK){
                jp_fail(p, "out of memory");
                goto FAIL_ARR;
        }
        jp_ws(p);
        if(p->pos < p->len && p->s[p->pos] == ']'){
                p->pos++;
                goto DONE;
        }
        while(1){
                if(jp_value(p, n->v[n->n]) != OK){
                        goto FAIL_ARR;
                }
                n->n++;
                if(tld_json_arr_chk_space(n) != OK){
                        jp_fail(p, "out of memory");
                        goto FAIL_ARR;
                }
                jp_ws(p);
                if(p->pos >= p->len){
                        jp_fail(p, "unexpected end of input in array");
                        goto FAIL_ARR;
                }
                if(p->s[p->pos] == ','){
                        p->pos++;
                }else if(p->s[p->pos] == ']'){
                        p->pos++;
                        break;
                }else{
                        jp_fail(p, "expected ',' or ']'");
                        goto FAIL_ARR;
                }
        }
DONE:
        p->depth--;
        *out = n;
        return OK;
FAIL_ARR:
        p->depth--;
        if(n){
                tld_json_arr_free(n);
        }
        return FAIL;
}

/* ------------------------------------------------------------------ */
/* Value helpers                                                       */
/* ------------------------------------------------------------------ */

/* Store len bytes of s as a string in the empty slot v. */
int json_val_set_str(tld_json_val *v, const char *s, int len)
{
        tld_strbuf* b = NULL;
        RUN(tld_strbuf_alloc(&b, len + 1));
        memcpy(b->str, s, len);
        b->len = len;
        b->str[len] = 0;
        v->type = TLD_JSON_STR;
        v->value.str = b;
        return OK;
ERROR:
        return FAIL;
}

/* Integers that fit in int32_t are stored as TLD_JSON_INT, larger ones as
   TLD_JSON_DBL (the value layout only has an int32_t). */
void json_val_set_int64(tld_json_val *v, long long x)
{
        if(x >= INT32_MIN && x <= INT32_MAX){
                v->type = TLD_JSON_INT;
                v->value.i_num = (int32_t)x;
        }else{
                v->type = TLD_JSON_DBL;
                v->value.d_num = (double)x;
        }
}

/* Used by JS(): the type is guessed from the text (callers rely on
   JS("k","0.2") producing a number). */
int add_val(char *buf, int len, tld_json_val *val)
{
        tld_json_type type = TLD_JSON_STR;
        detect_type(buf,len, &type);

        switch (type) {
        case TLD_JSON_DBL:
                val->type = TLD_JSON_DBL;
                val->value.d_num = strtod(buf, NULL);
                break;
        case TLD_JSON_INT:{
                long long x;
                errno = 0;
                x = strtoll(buf, NULL, 10);
                if(errno == ERANGE){
                        val->type = TLD_JSON_DBL;
                        val->value.d_num = strtod(buf, NULL);
                }else{
                        json_val_set_int64(val, x);
                }
                break;
        }
        case TLD_JSON_BOOL_TRUE:
                val->type = TLD_JSON_BOOL_TRUE;
                val->value.b_num = 1;
                break;
        case TLD_JSON_BOOL_FALSE:
                val->type = TLD_JSON_BOOL_FALSE;
                val->value.b_num = 0;
                break;
        case TLD_JSON_STR:
                RUN(json_val_set_str(val, buf, len));
                break;
        case TLD_JSON_ARR:
        case TLD_JSON_OBJ:
        case TLD_JSON_UNDEF:
                ERROR_MSG("This should never happen!!@");
                break;
        }
        return OK;
ERROR:
        return FAIL;
}

/* ------------------------------------------------------------------ */
/* Path getters                                                        */
/* ------------------------------------------------------------------ */

/* Resolve "a.b[2].c". *out is NULL when the path is empty (the root
   object itself). A path starting with '[' indexes the array that
   tld_json_parse wraps under "Top Level Arr". */
int json_path_resolve(tld_json_obj *root, const char *path, tld_json_val **out)
{
        tld_json_val* v = NULL;
        tld_json_obj* o = root;
        tld_json_arr* a = NULL;
        const char* p = path;
        int first = 1;

        if(root == NULL || path == NULL){
                return FAIL;
        }

        if(*p == '['){
                /* address the wrapped top-level array */
                v = NULL;
                for(int i = 0; i < root->n; i++){
                        if(strcmp(TLD_STR(root->key[i]), TLD_JSON_TOP_LEVEL_ARR) == 0){
                                v = root->v[i];
                                break;
                        }
                }
                if(v == NULL || v->type != TLD_JSON_ARR){
                        return FAIL;
                }
                o = NULL;
                a = v->value.arr;
                first = 0;
        }

        while(*p){
                if(*p == '['){
                        long idx = 0;
                        int digits = 0;
                        p++;
                        while(*p >= '0' && *p <= '9'){
                                if(digits >= 9){
                                        return FAIL;
                                }
                                idx = idx * 10 + (*p - '0');
                                digits++;
                                p++;
                        }
                        if(!digits || *p != ']'){
                                return FAIL;
                        }
                        p++;
                        if(a == NULL || idx >= a->n){
                                return FAIL;
                        }
                        v = a->v[idx];
                }else{
                        const char* k;
                        int klen;
                        tld_json_val* found = NULL;
                        if(!first){
                                if(*p != '.'){
                                        return FAIL;
                                }
                                p++;
                        }
                        k = p;
                        while(*p && *p != '.' && *p != '['){
                                p++;
                        }
                        klen = (int)(p - k);
                        if(klen == 0 || o == NULL){
                                return FAIL;
                        }
                        for(int i = 0; i < o->n; i++){
                                if(o->key[i]->len == klen &&
                                   memcmp(o->key[i]->str, k, klen) == 0){
                                        found = o->v[i];
                                        break;
                                }
                        }
                        if(found == NULL){
                                return FAIL;
                        }
                        v = found;
                }
                first = 0;
                o = NULL;
                a = NULL;
                if(v->type == TLD_JSON_OBJ){
                        o = v->value.obj;
                }else if(v->type == TLD_JSON_ARR){
                        a = v->value.arr;
                }
        }
        *out = v;
        return OK;
}

int tld_json_get_str(tld_json_obj *obj, const char *path, const char **out)
{
        tld_json_val* v = NULL;
        if(json_path_resolve(obj, path, &v) != OK || v == NULL){
                return FAIL;
        }
        if(v->type != TLD_JSON_STR || v->value.str == NULL){
                return FAIL;
        }
        if(out){
                *out = TLD_STR(v->value.str);
        }
        return OK;
}

int tld_json_get_num(tld_json_obj *obj, const char *path, double *out)
{
        tld_json_val* v = NULL;
        if(json_path_resolve(obj, path, &v) != OK || v == NULL){
                return FAIL;
        }
        if(v->type == TLD_JSON_DBL){
                if(out){
                        *out = v->value.d_num;
                }
        }else if(v->type == TLD_JSON_INT){
                if(out){
                        *out = (double)v->value.i_num;
                }
        }else{
                return FAIL;
        }
        return OK;
}

int tld_json_get_int(tld_json_obj *obj, const char *path, int64_t *out)
{
        tld_json_val* v = NULL;
        if(json_path_resolve(obj, path, &v) != OK || v == NULL){
                return FAIL;
        }
        if(v->type == TLD_JSON_INT){
                if(out){
                        *out = (int64_t)v->value.i_num;
                }
        }else if(v->type == TLD_JSON_DBL){
                double d = v->value.d_num;
                /* only integral values inside the int64_t range */
                if(!isfinite(d) || d != floor(d) ||
                   d < -9223372036854775808.0 || d >= 9223372036854775808.0){
                        return FAIL;
                }
                if(out){
                        *out = (int64_t)d;
                }
        }else{
                return FAIL;
        }
        return OK;
}

int tld_json_get_bool(tld_json_obj *obj, const char *path, int *out)
{
        tld_json_val* v = NULL;
        if(json_path_resolve(obj, path, &v) != OK || v == NULL){
                return FAIL;
        }
        if(v->type == TLD_JSON_BOOL_TRUE){
                if(out){
                        *out = 1;
                }
        }else if(v->type == TLD_JSON_BOOL_FALSE){
                if(out){
                        *out = 0;
                }
        }else{
                return FAIL;
        }
        return OK;
}

int tld_json_get_obj(tld_json_obj *obj, const char *path, tld_json_obj **out)
{
        tld_json_val* v = NULL;
        if(json_path_resolve(obj, path, &v) != OK){
                return FAIL;
        }
        if(v == NULL){
                /* empty path: the object itself */
                if(out){
                        *out = obj;
                }
                return OK;
        }
        if(v->type != TLD_JSON_OBJ || v->value.obj == NULL){
                return FAIL;
        }
        if(out){
                *out = v->value.obj;
        }
        return OK;
}

int tld_json_get_arr(tld_json_obj *obj, const char *path, tld_json_arr **out)
{
        tld_json_val* v = NULL;
        if(json_path_resolve(obj, path, &v) != OK || v == NULL){
                return FAIL;
        }
        if(v->type != TLD_JSON_ARR || v->value.arr == NULL){
                return FAIL;
        }
        if(out){
                *out = v->value.arr;
        }
        return OK;
}

/* ------------------------------------------------------------------ */
/* Writer                                                              */
/* ------------------------------------------------------------------ */

int tld_json_dump(tld_json_obj *n, tld_strbuf *b,int indent)
{
        if(n){
                RUN(tld_append(b, "{\n"));

                for(int i = 0; i < n->n;i++){
                        RUN(tld_json_add_indent(b, indent + 4));
                        RUN(tld_json_dump_str(b, n->key[i]->str, n->key[i]->len));
                        RUN(tld_append(b, ": "));

                        if(n->v[i]->type == TLD_JSON_ARR){
                                if(n->v[i]->value.arr){
                                        RUN(tld_json_dump_arr(n->v[i]->value.arr, b,indent + 4));
                                }else{
                                        RUN(tld_append(b, "null"));
                                }
                        }else if(n->v[i]->type == TLD_JSON_OBJ){
                                if(n->v[i]->value.obj){
                                        RUN(tld_json_dump(n->v[i]->value.obj, b,indent + 4));
                                }else{
                                        RUN(tld_append(b, "null"));
                                }
                        }else{
                                RUN(tld_json_dump_val(n->v[i], b));
                        }
                        if (i < n->n - 1) {
                                RUN(tld_append(b, ","));
                        }
                        RUN(tld_append(b, "\n"));
                }
                RUN(tld_json_add_indent(b, indent));
                RUN(tld_append(b, "}"));
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_json_dump_arr(tld_json_arr *n, tld_strbuf *b,int indent)
{

        if(n){
                RUN(tld_append(b, "[\n"));

                for (int i = 0; i < n->n; i++) {
                        RUN(tld_json_add_indent(b, indent + 4));
                        if (n->v[i]->type == TLD_JSON_ARR) {
                                if(n->v[i]->value.arr){
                                        RUN(tld_json_dump_arr(n->v[i]->value.arr, b,indent + 4));
                                }else{
                                        RUN(tld_append(b, "null"));
                                }
                        } else if (n->v[i]->type == TLD_JSON_OBJ) {
                                if(n->v[i]->value.obj){
                                        RUN(tld_json_dump(n->v[i]->value.obj, b,indent + 4));
                                }else{
                                        RUN(tld_append(b, "null"));
                                }
                        } else {
                                RUN(tld_json_dump_val(n->v[i], b));
                        }
                        if (i < n->n - 1) {
                                RUN(tld_append(b, ","));
                        }
                        RUN(tld_append(b, "\n"));
                }
                RUN(tld_json_add_indent(b, indent));
                RUN(tld_append(b, "]"));
        }
        return OK;
ERROR:
        return FAIL;
}

/* Write s as a quoted JSON string, escaping '"', '\\' and control
   characters. Bytes >= 0x80 (UTF-8) are written unchanged. */
int tld_json_dump_str(tld_strbuf *b, const uint8_t *s, int len)
{
        static const char hex[] = "0123456789abcdef";
        char* out = NULL;
        int o = 0;

        MMALLOC(out, (size_t)len * 6 + 3);
        out[o++] = '"';
        for(int i = 0; i < len; i++){
                uint8_t c = s[i];
                switch(c){
                case '"':  out[o++] = '\\'; out[o++] = '"';  break;
                case '\\': out[o++] = '\\'; out[o++] = '\\'; break;
                case '\n': out[o++] = '\\'; out[o++] = 'n';  break;
                case '\r': out[o++] = '\\'; out[o++] = 'r';  break;
                case '\t': out[o++] = '\\'; out[o++] = 't';  break;
                case '\b': out[o++] = '\\'; out[o++] = 'b';  break;
                case '\f': out[o++] = '\\'; out[o++] = 'f';  break;
                default:
                        if(c < 0x20){
                                out[o++] = '\\';
                                out[o++] = 'u';
                                out[o++] = '0';
                                out[o++] = '0';
                                out[o++] = hex[c >> 4];
                                out[o++] = hex[c & 0xF];
                        }else{
                                out[o++] = (char)c;
                        }
                        break;
                }
        }
        out[o++] = '"';
        out[o] = 0;
        RUN(tld_append(b, out));
        MFREE(out);
        return OK;
ERROR:
        if(out){
                MFREE(out);
        }
        return FAIL;
}

int tld_json_dump_val(tld_json_val *val, tld_strbuf *b)
{
        char buf[40];
        if(val->type == TLD_JSON_STR){
                if(val->value.str){
                        RUN(tld_json_dump_str(b, val->value.str->str, val->value.str->len));
                }else{
                        RUN(tld_append(b, "null"));
                }
        }else if(val->type == TLD_JSON_INT) {
                snprintf(buf, sizeof(buf), "%d", val->value.i_num);
                RUN(tld_append(b, buf));
        }else if(val->type == TLD_JSON_DBL){
                double d = val->value.d_num;
                if(!isfinite(d)){
                        /* inf / nan are not valid JSON */
                        RUN(tld_append(b, "null"));
                }else{
                        /* shortest of %.15g .. %.17g that reads back exactly */
                        for(int prec = 15; prec <= 17; prec++){
                                snprintf(buf, sizeof(buf), "%.*g", prec, d);
                                if(strtod(buf, NULL) == d){
                                        break;
                                }
                        }
                        RUN(tld_append(b, buf));
                }
        }else if(val->type == TLD_JSON_BOOL_TRUE){
                RUN(tld_append(b, "true"));
        }else if(val->type == TLD_JSON_BOOL_FALSE){
                RUN(tld_append(b, "false"));
        }else{
                RUN(tld_append(b, "null"));
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_json_add_indent(tld_strbuf *b, int indent)
{

        char buf[32];
        if(indent > 31){
                indent = 31;
        }

        for (int i = 0; i < indent; i++) {
                buf[i] = ' ';
        }
        buf[indent] = 0;
        RUN(tld_append(b, buf));
        return OK;
ERROR:
        return FAIL;
}

int tld_json_obj_print(tld_json_obj *n, FILE *out)
{
        if(out == NULL){
                out = stdout;
        }
        if(n){
                fprintf(out,"Object:\n");
                for(int i = 0; i < n->n;i++){
                        fprintf(out,"NAME: %s\n",TLD_STR(n->key[i]));
                        if(n->v[i]->type == TLD_JSON_ARR){
                                tld_json_arr_print(n->v[i]->value.arr, out);
                        }else if(n->v[i]->type == TLD_JSON_OBJ){
                                tld_json_obj_print(n->v[i]->value.obj, out);
                        }else{

                                fprintf(out,"  %s : ", TLD_STR(n->key[i]));
                                print_val(n->v[i],out);
                                fprintf(out,"\n");
                        }
                }
        }
        return OK;
}

int tld_json_arr_print(tld_json_arr *n, FILE *out)
{
        if(out == NULL){
                out = stdout;
        }
        if(n){

                fprintf(out,"ARRAY:\n");
                for(int i = 0; i < n->n;i++){
                        if(n->v[i]->type == TLD_JSON_ARR){
                                tld_json_arr_print(n->v[i]->value.arr, out);
                        }else if(n->v[i]->type == TLD_JSON_OBJ){
                                tld_json_obj_print(n->v[i]->value.obj, out);
                        }else{
                                print_val(n->v[i],out);;
                                fprintf(out,"\n");
                        }
                }
        }
        return OK;
}

int print_val(tld_json_val *val, FILE *out)
{
        if(out == NULL){
                out = stdout;
        }
        switch (val->type) {
        case TLD_JSON_DBL:
                fprintf(out,"%f\n",val->value.d_num);
                break;
        case TLD_JSON_INT:
                fprintf(out,"%d\n",val->value.i_num);
                break;
        case TLD_JSON_BOOL_TRUE:
                fprintf(out,"%s","TRUE");
                break;
        case TLD_JSON_BOOL_FALSE:
                fprintf(out,"%s","FALSE");
                break;
        case TLD_JSON_STR:
                fprintf(out,"%s", TLD_STR(val->value.str));
                break;
        case TLD_JSON_UNDEF:
                fprintf(out,"%s","null");
                break;
        case TLD_JSON_ARR:
                ERROR_MSG("This should never happen - ARR");
                break;
        case TLD_JSON_OBJ:
                ERROR_MSG("This should never happen - OBJ");
                break;
        }
        return OK;
ERROR:
        return FAIL;
}

