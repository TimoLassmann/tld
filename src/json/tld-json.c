
#include "../alloc/tld-alloc.h"
#include "../string/str.h"

#include "tld-json_struct.h"
#include "tld-json_mem.h"
#include "tld-json_misc.h"

#define TLD_JSON_IMPORT
#include "tld-json.h"




static int tld_json_dump_val(tld_json_val *val, tld_strbuf *b);
static int tld_json_add_indent(tld_strbuf *b, int indent);
/* static int tld_json_get_ret_val(tld_json_val *v, tld_json_ret **ret); */
/* static int tld_json_get_arr_str(tld_json_arr *n, char *key, tld_json_val **res); */
static int tld_json_get_arr_str(tld_json_arr *n, char *key, tld_json_obj* res);
/* static int tld_json_get_arr_str(tld_json_arr *n, char *key, tld_strbuf *res); */
static int tld_json_parse_obj(tld_json_arr *lex, tld_json_obj **out);
static int tld_json_parse_arr(tld_json_arr *lex, tld_json_arr **out);
static int tld_json_lex(tld_strbuf *t, tld_json_arr **out);
static int add_val(char *buf, int len, tld_json_val *val);

static int lex_error_print(tld_json_arr *lex);
static int print_val(tld_json_val *val, FILE *out);
int print_arr(tld_json_arr* n);
/* static int tld_json_parse_arr(tld_json_arr *lex, tld_json_arr **out); */
/* static int tld_json_parse_node(tld_json_arr *lex,tld_json_obj** out); */
static int search(tld_json_obj *n, char *key, tld_json_obj* r);



static tld_json_obj* tld_json_obj_add_val(tld_json_obj *obj, char* key, char* val);
static tld_json_obj* tld_json_obj_add_obj(tld_json_obj *obj, char* key, tld_json_obj*val);
static tld_json_obj *tld_json_obj_add_arr(tld_json_obj *obj, char *key, tld_json_arr *val);
static tld_json_arr* tld_json_arr_add_val(tld_json_arr *arr, char* val);
static tld_json_arr* tld_json_arr_add_arr(tld_json_arr *arr, tld_json_arr*val);
static tld_json_arr *tld_json_arr_add_obj(tld_json_arr *arr, tld_json_obj *val);

tld_json_obj *json_obj_add_multiple(tld_json_obj *obj, tld_json_decoration *kvs, int count)
{
        for (int i = 0; i < count; i++) {
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
                }
        }
        return obj;
}


tld_json_obj* tld_json_obj_add_val(tld_json_obj *obj, char* key, char* val)
{
        tld_json_obj* n = NULL;
        int len;
        if(obj == NULL){
                tld_json_obj_alloc(&n);
        }else{
                n = obj;
        }

        tld_append(n->key[n->n], key);

        len = strlen(val);
        add_val(val,len, n->v[n->n]);

        n->n++;

        tld_json_obj_chk_space(n);
        if(obj == NULL){
                return n;
        }
        return obj;
}

tld_json_obj* tld_json_obj_add_obj(tld_json_obj *obj, char* key, tld_json_obj*val)
{
        tld_json_obj* n = NULL;
        if(obj == NULL){
                tld_json_obj_alloc(&n);
        }else{
                n = obj;
        }

        tld_append(n->key[n->n], key);
        n->v[n->n]->type = TLD_JSON_OBJ;
        n->v[n->n]->value.obj = val;

        n->n++;
        tld_json_obj_chk_space(n);
        if(obj == NULL){
                return n;
        }
        return obj;
}



tld_json_obj* tld_json_obj_add_arr(tld_json_obj *obj, char* key, tld_json_arr*val)
{
        tld_json_obj* n = NULL;
        if(obj == NULL){
                tld_json_obj_alloc(&n);
        }else{
                n = obj;
        }

        tld_append(n->key[n->n], key);
        n->v[n->n]->type = TLD_JSON_ARR;
        n->v[n->n]->value.arr = val;
        n->n++;

        tld_json_obj_chk_space(n);
        if(obj == NULL){
                return n;
        }
        return obj;
}

tld_json_arr *json_arr_add_multiple(tld_json_arr *arr, tld_json_decoration *kvs, int count)
{
        for (int i = 0; i < count; i++) {
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
                }
        }
        return arr;
}


tld_json_arr* tld_json_arr_add_val(tld_json_arr *arr, char* val)
{
        tld_json_arr* n = NULL;
        int len;
        if(arr == NULL){
                tld_json_arr_alloc(&n);
        }else{
                n = arr;
        }
        len = strlen(val);
        add_val(val,len, n->v[n->n]);
        n->n++;
        tld_json_arr_chk_space(n);
        if(arr == NULL){
                return n;
        }
        return arr;
}

tld_json_arr* tld_json_arr_add_arr(tld_json_arr *arr, tld_json_arr*val)
{
        tld_json_arr* n = NULL;
        if(arr == NULL){
                tld_json_arr_alloc(&n);
        }else{
                n = arr;
        }
        n->v[n->n]->type = TLD_JSON_ARR;
        n->v[n->n]->value.arr = val;
        n->n++;
        tld_json_arr_chk_space(n);
        if(arr == NULL){
                return n;
        }
        return arr;
}

tld_json_arr* tld_json_arr_add_obj(tld_json_arr *arr, tld_json_obj*val)
{
        tld_json_arr* n = NULL;
        if(arr == NULL){
                tld_json_arr_alloc(&n);
        }else{
                n = arr;
        }
        n->v[n->n]->type = TLD_JSON_OBJ;
        n->v[n->n]->value.obj = val;
        n->n++;
        tld_json_arr_chk_space(n);
        if(arr == NULL){
                return n;
        }
        return arr;
}


int tld_json_search(tld_json_obj *n, char *key, tld_json_obj **ret)
{
        tld_json_obj* r = NULL;
        RUN(tld_json_obj_alloc(&r));
        search(n, key, r);
        /* for(int i =0; i < r->n;i++){ */
        /*         fprintf(stdout,"KEY: %s\n",TLD_STR(r->key[i])); */
        /*         print_val(r->v[i],stdout); */
        /*         fprintf(stdout,"\n"); */
        /* } */
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
                        /* fprintf(stdout,"NAME: %s  LEN: %d %d\n",TLD_STR(n->key[i]),n->key[i]->len,len); */
                        if(len == n->key[i]->len){
                                if(strncmp((char*)n->key[i]->str, key, n->key[i]->len)==0){
                                        tld_append(r->key[r->n], TLD_STR(n->key[i]));
                                        tld_json_val_copy(r->v[r->n], n->v[i]);
                                        r->n++;
                                        tld_json_obj_chk_space(n);
                                        break;
                                }
                        }

                        if(n->v[i]->type == TLD_JSON_ARR){

                                tld_json_get_arr_str(n->v[i]->value.arr,key,r);

                        }else if(n->v[i]->type == TLD_JSON_OBJ){

                                search(n->v[i]->value.obj, key,r);

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

                                tld_json_get_arr_str(n->v[i]->value.arr,key,res);
                        }else if(n->v[i]->type == TLD_JSON_OBJ){
                                search(n->v[i]->value.obj, key,res);

                        }

                }
        }
        return OK;
}

int tld_json_parse(tld_strbuf *t, tld_json_obj **out)
{
        tld_json_arr *lex = NULL;
        tld_json_val* tok = NULL;
        tld_json_obj* n = NULL;
        tld_json_lex(t, &lex);
        /* print_arr(lex); */

        tok = lex->v[lex->read_head];
        lex->read_head++;

        if(is_open_obj(tok)){
                tld_json_parse_obj(lex, &n);
        }else if(is_open_arr(tok)){
                RUN(tld_json_obj_alloc(&n));
                tld_append( n->key[0] ,"Top Level Arr");
                n->v[0]->type = TLD_JSON_ARR;
                RUN(tld_json_parse_arr(lex, &n->v[0]->value.arr));
                n->n++;
        }else{
                ERROR_MSG("Text does not start with { or [");
        }

        tld_json_arr_free(lex);

        *out = n;
        return OK;
ERROR:
        return FAIL;
}


/* parser */
static int tld_json_parse_obj(tld_json_arr *lex, tld_json_obj **out)
{
        tld_json_val* val = NULL;
        tld_json_val* key = NULL;
        tld_json_val* tok = NULL;
        tld_json_obj* n = NULL;


        RUN(tld_json_obj_alloc(&n));

        tok = lex->v[lex->read_head];
        lex->read_head++;

        /* lex->read_head++; */
        if(is_close_obj(tok)){
                /* Empty object  */
                *out = n;
                return OK;
        }
        lex->read_head--;

        while(1){
                key = lex->v[lex->read_head];
                lex->read_head++;

                if(key->type != TLD_JSON_STR){
                        lex_error_print(lex);
                        ERROR_MSG("Error in obj parse: expecting STR as key!");
                }


                tok = lex->v[lex->read_head];
                lex->read_head++;

                if(!is_colon(tok)){
                        lex_error_print(lex);
                        ERROR_MSG("Expecting ':'!");
                }
                /* lex->read_head++; */

                val = lex->v[lex->read_head];
                lex->read_head++;
                if(is_open_arr(val)){
                        /* do array strugg */
                        tld_append(n->key[n->n], TLD_STR(key->value.str));
                        n->v[n->n]->type = TLD_JSON_ARR;
                        RUN(tld_json_parse_arr(lex, &n->v[n->n]->value.arr));
                        n->n++;
                        tld_json_obj_chk_space(n);
                }else if(is_open_obj(val)){
                        tld_append(n->key[n->n], TLD_STR(key->value.str));
                        n->v[n->n]->type = TLD_JSON_OBJ;
                        RUN(tld_json_parse_obj(lex, &n->v[n->n]->value.obj));
                        n->n++;
                        tld_json_obj_chk_space(n);
                }else{
                        tld_append(n->key[n->n], TLD_STR(key->value.str));
                        tld_json_val_copy(n->v[n->n], val);
                        n->n++;
                        tld_json_obj_chk_space(n);
                }

                tok = lex->v[lex->read_head];
                lex->read_head++;

                if(is_close_obj(tok)){
                        break;
                }else if(!is_comma(tok)){
                        lex_error_print(lex);
                        fprintf(stdout,"ERROR - this should be a ',' (pos: %d): ", lex->read_head);

                        print_val(tok,NULL);
                        fprintf(stdout,"\n");
                        ERROR_MSG("Weird - this should be a , but:::");
                }
        }
        *out = n;
        return OK;
ERROR:
        tld_json_obj_free(n);
        return FAIL;
}

static int tld_json_parse_arr(tld_json_arr *lex, tld_json_arr **out)
{
        tld_json_val* tok = NULL;
        tld_json_arr* n = NULL;
        RUN(tld_json_arr_alloc(&n));

        tok = lex->v[lex->read_head];
        lex->read_head++;

        if(is_close_arr(tok)){
                *out = n;
                return OK;
        }
        lex->read_head--;


        while(1){
                tok = lex->v[lex->read_head];
                lex->read_head++;

                if(is_open_obj(tok)){
                        n->v[n->n]->type = TLD_JSON_OBJ;
                        RUN(tld_json_parse_obj(lex, &n->v[n->n]->value.obj));
                        n->n++;
                        tld_json_arr_chk_space(n);
                }else if(is_open_arr(tok)){
                        n->v[n->n]->type = TLD_JSON_ARR;
                        RUN(tld_json_parse_arr(lex, &n->v[n->n]->value.arr));
                        n->n++;
                        tld_json_arr_chk_space(n);
                        /* should check for formar */
                }else{
                        tld_json_val_copy(n->v[n->n], tok);
                        n->n++;
                        tld_json_arr_chk_space(n);
                }
                tok = lex->v[lex->read_head];
                lex->read_head++;

                if(is_close_arr(tok)){
                        break;
                }else if(!is_comma(tok)){
                        fprintf(stdout,"ERROR - this should be a ',' : ");
                        print_val(tok,NULL);
                        fprintf(stdout,"\n");
                        ERROR_MSG("Weird - this should be a , but:::");
                }
        }

        *out = n;
        return OK;
ERROR:
        tld_json_arr_free(n);
        return FAIL;

}

/* Lexer  */
int tld_json_lex(tld_strbuf *t, tld_json_arr **out)
{
        tld_json_arr* n = NULL;
        tld_json_val* val = NULL;
        uint8_t* str = NULL;
        char* buf = NULL;
        int n_buf_size = 4096;
        int b_pos = 0;

        int len = 0;
        uint8_t in_str = 0;

        MMALLOC(buf, sizeof(char) * n_buf_size);

        RUN(tld_json_arr_alloc(&n));

        str = t->str;
        len = t->len;

        for(int i = 0; i < len; i++){
                switch(str[i]){
                case '}':
                case '{':
                case '[':
                case ']':
                case ':':
                case ',':
                        if(in_str){
                                buf[b_pos] = str[i];
                                b_pos++;
                                if(b_pos ==n_buf_size){
                                        n_buf_size = n_buf_size + n_buf_size /2;
                                        MREALLOC(buf, sizeof(char) * n_buf_size);
                                }
                        }else{
                                if(b_pos){
                                        buf[b_pos] = 0;
                                        val = n->v[n->n];
                                        add_val(buf, b_pos, val);
                                        n->n++;
                                        if(n->n == n->n_alloc){
                                                tld_json_arr_realloc(n);
                                        }

                                        b_pos = 0;
                                }
                                val = n->v[n->n];
                                if(val->type == TLD_JSON_UNDEF){
                                        tld_strbuf_alloc(&val->value.str, 16);
                                }
                                val->type = TLD_JSON_STR;

                                tld_append_char(val->value.str, str[i]);
                                n->n++;
                                if(n->n == n->n_alloc){
                                        tld_json_arr_realloc(n);
                                }
                        }
                        break;
                case '\t':
                case ' ':
                case '\n':
                        if(in_str){
                                buf[b_pos] = str[i];
                                b_pos++;
                                if(b_pos ==n_buf_size){
                                        n_buf_size = n_buf_size + n_buf_size /2;
                                        MREALLOC(buf, sizeof(char) * n_buf_size);
                                }
                        }
                        break;
                case '"':
                        if(i && str[i-1] == '\\'){
                                 buf[b_pos] = str[i];
                                 b_pos++;
                        }else{
                                if(!in_str){
                                        in_str = 1;
                                }else{
                                        buf[b_pos] = 0;
                                        val = n->v[n->n];
                                        add_val(buf, b_pos , val);
                                        n->n++;
                                        if(n->n == n->n_alloc){
                                                tld_json_arr_realloc(n);
                                        }
                                        in_str = 0;
                                        b_pos = 0;
                                }
                        }
                        break;
                default:
                        buf[b_pos] = str[i];
                        b_pos++;
                        if(b_pos ==n_buf_size){
                                n_buf_size = n_buf_size + n_buf_size /2;
                                MREALLOC(buf, sizeof(char) * n_buf_size);
                        }
                        break;
                }
        }
        MFREE(buf);
        *out = n;
        return OK;
ERROR:
        tld_json_arr_free(n);
        return FAIL;
}

int add_val(char *buf, int len, tld_json_val *val)
{

        detect_type(buf,len,   &val->type);

        switch (val->type) {
        case TLD_JSON_DBL:
                val->value.d_num = atof(buf);
                break;
        case TLD_JSON_INT:
                val->value.i_num = atoi(buf);
                break;
        case TLD_JSON_BOOL_TRUE:
                val->value.b_num = 1;
                break;
        case TLD_JSON_BOOL_FALSE:
                val->value.b_num = 0;
                break;
        case TLD_JSON_STR:
                tld_strbuf_alloc(&val->value.str, 16);
                tld_append(val->value.str, buf);
                break;
        case TLD_JSON_ARR:
                ERROR_MSG("This should never happen!!@");
                break;
        case TLD_JSON_OBJ:
                ERROR_MSG("This should never happen!!@");
                break;
        case TLD_JSON_UNDEF:
                ERROR_MSG("This should never happen!!@");
                break;
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_json_dump(tld_json_obj *n, tld_strbuf *b,int indent)
{
        if(n){
                tld_append(b, "{\n");

                for(int i = 0; i < n->n;i++){
                        tld_json_add_indent(b, indent + 4);
                        tld_append(b, "\"");
                        tld_append(b, TLD_STR(n->key[i]));
                        tld_append(b, "\": ");

                        if(n->v[i]->type == TLD_JSON_ARR){
                                tld_json_dump_arr(n->v[i]->value.arr, b,indent + 4);
                        }else if(n->v[i]->type == TLD_JSON_OBJ){
                                tld_json_dump(n->v[i]->value.obj, b,indent + 4);
                        }else{
                                tld_json_dump_val(n->v[i], b);
                        }
                        if (i < n->n - 1) {
                                tld_append(b, ",");
                        }
                        tld_append(b, "\n");
                }
                tld_json_add_indent(b, indent);
                tld_append(b, "}");
        }
        return OK;
}

int tld_json_dump_arr(tld_json_arr *n, tld_strbuf *b,int indent)
{

        if(n){
                tld_append(b, "[\n");

                for (int i = 0; i < n->n; i++) {
                        tld_json_add_indent(b, indent + 4);
                        if (n->v[i]->type == TLD_JSON_ARR) {
                                tld_json_dump_arr(n->v[i]->value.arr, b,indent + 4);
                        } else if (n->v[i]->type == TLD_JSON_OBJ) {
                                tld_json_dump(n->v[i]->value.obj, b,indent + 4);
                        } else {
                                tld_json_dump_val(n->v[i], b);
                        }
                        if (i < n->n - 1) {
                                tld_append(b, ",");
                        }
                        tld_append(b, "\n");
                }
                tld_json_add_indent(b, indent);
                tld_append(b, "]");
        }
        return OK;
}

int tld_json_dump_val(tld_json_val *val, tld_strbuf *b)
{
        char buf[16];
        if(val->type == TLD_JSON_STR){
                tld_append(b, "\"");
                tld_append(b, TLD_STR(val->value.str));
                tld_append(b, "\"");
        }else if(val->type == TLD_JSON_INT) {
                snprintf(buf, 16, "%d", val->value.i_num);
                tld_append(b, buf);
        }else if(val->type == TLD_JSON_DBL){
                snprintf(buf, 16, "%f", val->value.d_num);
                tld_append(b, buf);
        }else if(val->type == TLD_JSON_BOOL_TRUE){
                tld_append(b, "true");
        }else if(val->type == TLD_JSON_BOOL_FALSE){
                tld_append(b, "false");
        }else{
                tld_append(b, "null");
        }
        return OK;
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
        tld_append(b, buf);
        return OK;
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
                                /* fprintf(out,"  %s : ", TLD_STR(n->key[i])); */
                                print_val(n->v[i],out);;
                                fprintf(out,"\n");
                        }
                }
        }
        return OK;
}


int lex_error_print(tld_json_arr *lex)
{
        int s = 0;
        s = 0;//lex->read_head - 10;
        s = MACRO_MAX(0, s);
        for(int i = s; i <= lex->read_head;i++){
                fprintf(stdout,"Tok %5d : ", i);
                print_val(lex->v[i], stdout);
                fprintf(stdout,"\n");
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
        case TLD_JSON_ARR:
                ERROR_MSG("This should never happen - ARR");
                break;
        case TLD_JSON_OBJ:
                ERROR_MSG("This should never happen - OBJ");
                break;
        case TLD_JSON_UNDEF:
                ERROR_MSG("This should never happen - UNDEF");
                break;
        }
        return OK;
ERROR:
        return FAIL;
}

int print_arr(tld_json_arr* n)
{
        tld_json_val* val = NULL;
        for(int i = 0; i < n->n;i++){
                val = n->v[i];
                switch (val->type) {
                case TLD_JSON_DBL:
                        LOG_MSG("Token %5d: %f - DBL",i, val->value.d_num);
                        break;
                case TLD_JSON_INT:
                        LOG_MSG("Token %5d: %d - INT",i, val->value.i_num);
                        break;
                case TLD_JSON_BOOL_TRUE:
                        LOG_MSG("Token %5d: %s - BOOL",i, "TRUE");
                        break;
                case TLD_JSON_BOOL_FALSE:
                        LOG_MSG("Token %5d: %s - BOOL",i, "FALSE");
                        break;
                case TLD_JSON_STR:
                        LOG_MSG("Token %5d: %s - STRING",i, TLD_STR(val->value.str));
                        break;
                case TLD_JSON_ARR:
                        ERROR_MSG("This should never happen!!@");
                        break;
                case TLD_JSON_OBJ:
                        ERROR_MSG("This should never happen!!@");
                        break;
                case TLD_JSON_UNDEF:
                        ERROR_MSG("This should never happen!!@");
                        break;
                }
        }
        return OK;
ERROR:
        return FAIL;
}
