
#include "../alloc/tld-alloc.h"
#include "../string/str.h"

#include "tld-json_struct.h"
#include "tld-json_mem.h"
#include "tld-json_misc.h"

#define TLD_JSON_IMPORT
#include "tld-json.h"

static int tld_json_get_ret_val(tld_json_val *v, tld_json_ret **ret);
static int tld_json_get_arr_str(tld_json_arr *n, char *key, tld_json_ret** res);
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

int tld_json_obj_get(tld_json_obj *n, char *key, tld_json_ret **ret)
{
        if(n){

                for(int i = 0; i < n->n;i++){

                        int len = strnlen(key, 256);
                        /* fprintf(stdout,"NAME: %s  LEN: %d %d\n",TLD_STR(n->key[i]),n->key[i]->len,len); */
                        if(len == n->key[i]->len){
                                if(strncmp((char*)n->key[i]->str, key, n->key[i]->len)==0){
                                        RUN(tld_json_get_ret_val(n->v[i], ret));
                                        break;
                                }
                        }

                        if(n->v[i]->type == TLD_JSON_ARR){

                                tld_json_get_arr_str(n->v[i]->value.arr,key,ret);

                        }else if(n->v[i]->type == TLD_JSON_OBJ){

                                tld_json_obj_get(n->v[i]->value.obj, key,ret);

                        }
                }
        }

        return OK;
ERROR:
        return FAIL;
}

/* int tld_json_obj_get_str(tld_json_obj *n, char *key, tld_strbuf* res) */
/* { */
/*         if(n){ */
/*                 for(int i = 0; i < n->n;i++){ */
/*                         LOG_MSG("%s %d --- in get %s", TLD_STR(n->key[i]),n->key[i]->len,key); */
/*                         if(strncmp((char*)n->key[i]->str, key, n->key[i]->len)==0){ */
/*                                 LOG_MSG("FOUNE!!!! type: %d", n->v[i]->type); */
/*                                 if(n->v[i]->type == TLD_JSON_STR){ */
/*                                         /\* fprintf(stdout, "%d %s key: %s\n",i ,TLD_STR(n->key[i]),key); *\/ */
/*                                         tld_append(res, TLD_STR( n->v[i]->value.str)); */
/*                                 }else if(n->v[i]->type == TLD_JSON_ARR){ */
/*                                         tld_json_arr *arr = n->v[i]->value.arr; */
/*                                         LOG_MSG("num entries: %d",arr->n); */
/*                                         for(int j = 0; j < arr->n;j++){ */
/*                                                 switch (arr->v[j]->type) { */
/*                                                 case TLD_JSON_BOOL_TRUE: */
/*                                                         fprintf(stdout,"true\n" ); */
/*                                                         break; */
/*                                                 case TLD_JSON_BOOL_FALSE: */
/*                                                         fprintf(stdout,"false\n" ); */
/*                                                         break; */
/*                                                 case TLD_JSON_INT: */
/*                                                         fprintf(stdout,"%d\n",arr->v[j]->value.i_num ); */
/*                                                         break; */
/*                                                 case TLD_JSON_DBL: */
/*                                                         fprintf(stdout,"%f\n",arr->v[j]->value.d_num ); */
/*                                                         break; */
/*                                                 case TLD_JSON_OBJ: */
/*                                                 case TLD_JSON_ARR: */
/*                                                 case TLD_JSON_STR: */
/*                                                 case TLD_JSON_UNDEF: */
/*                                                         LOG_MSG("Dunno what to do!"); */
/*                                                         break; */
/*                                                 } */




/*                                         } */
/*                                         /\* n->v[i]->value *\/ */
/*                                 }else{ */
/*                                         LOG_MSG("Got a different value %d !",n->v[i]->type); */
/*                                 } */
/*                         } */
/*                         if(n->v[i]->type == TLD_JSON_ARR){ */
/*                                 if(!TLD_STRLEN(res)){ */
/*                                         tld_json_get_arr_str(n->v[i]->value.arr,key,res); */
/*                                 } */
/*                         }else if(n->v[i]->type == TLD_JSON_OBJ){ */
/*                                 if(!TLD_STRLEN(res)){ */
/*                                         tld_json_obj_get_str(n->v[i]->value.obj, key,res); */
/*                                 } */
/*                         } */
/*                 } */
/*         } */

/*         return OK; */
/* /\* ERROR: *\/ */
/* /\*         return FAIL; *\/ */
/* } */

int tld_json_get_arr_str(tld_json_arr *n, char *key, tld_json_ret** res)
{
        if(n){
                for(int i = 0; i < n->n;i++){
                        if(n->v[i]->type == TLD_JSON_ARR){

                                tld_json_get_arr_str(n->v[i]->value.arr,key,res);
                        }else if(n->v[i]->type == TLD_JSON_OBJ){
                                tld_json_obj_get(n->v[i]->value.obj, key,res);

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
        s = lex->read_head - 10;
        s = MACRO_MAX(0, s);
        for(int i = s; i < lex->read_head;i++){
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
                fprintf(out,"%f",val->value.d_num);
                break;
        case TLD_JSON_INT:
                fprintf(out,"%d",val->value.i_num);
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


/* static  int tld_json_val_copy(tld_json_val *t, tld_json_val *s); */
/* static  int tld_json_val_alloc(tld_json_val **node); */
/* static void tld_json_val_free(tld_json_val *n); */

/* int detect_type(char*s, int len, tld_json_type* type); */
/* int eat_space(uint8_t *s, int len, int *pos); */
/* int print_arr(tld_json_arr* n); */
/* int get_key(uint8_t *s, int len, int *pos, int *p_len); */

/* int get_key(uint8_t *s, int len, int *pos, int *p_len) */
/* { */
/*         int start = -1; */
/*         int stop = -1; */

/*         int p = *pos; */
/*         for(int i = p;i < len;i++){ */
/*                 if(s[i] == '"'){ */
/*                         if(start == -1){ */
/*                                 start = i+1; */
/*                         }else{ */
/*                                 stop = i; */
/*                                 i = len; */
/*                         } */
/*                 } */
/*         } */
/*         *pos = start; */
/*         *p_len = stop; */
/*         return OK; */
/* } */

/* int eat_space(uint8_t *s, int len,int *pos) */
/* { */
/*         int p = *pos; */
/*         while(p < len){ */
/*                 if(s[p] == ' ' || s[p] == '\t' || s[p] == '\n'){ */
/*                         p++; */
/*                 }else{ */
/*                         break; */
/*                 } */
/*         } */
/*         *pos = p; */
/*         return OK; */
/* } */


/* int tld_json_parser(tld_json_arr *lex,tld_json_node** out) */
/* { */

/*         tld_json_node*  n = NULL; */
/*         RUN(tld_json_node_alloc(&n)); */

/*         for(int i = 0; i < lex->n;i++){ */

/*         } */

/*         tld_json_node_free(n); */
/*         return OK; */
/* ERROR: */
/*         tld_json_node_free(n); */
/*         return FAIL; */
/* } */




int tld_json_val_copy(tld_json_val *t, tld_json_val *s)
{
        ASSERT(t != NULL,"No target");
        ASSERT(s != NULL,"No source");


        switch(s->type){
        case TLD_JSON_OBJ:
                ERROR_MSG("Node copy not implemented!");
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
        case TLD_JSON_STR:
                if(t->type == TLD_JSON_UNDEF){
                        tld_strbuf_alloc(&t->value.str, 16);
                }
                tld_strbuf_copy(t->value.str, s->value.str);
                break;
        case TLD_JSON_ARR:
                ERROR_MSG("Copy of list not implemented. ");
        case TLD_JSON_UNDEF:
                break;
        }
        t->type = s->type;
        return OK;
ERROR:
        return FAIL;
}

int tld_json_get_ret_val(tld_json_val* v, tld_json_ret **ret)
{
        tld_json_ret* r = NULL;
        tld_json_arr *arr = NULL;
        int max;
        int max_idx;
        int n_elem = 0;
        int type[8];
        RUN(tld_json_ret_alloc(&r));
        print_val(v,stdout);
        switch (v->type) {
        case TLD_JSON_OBJ:

        case TLD_JSON_STR:
                LOG_MSG("Got a string %s", TLD_STR(v->value.str));
                r->type = TLD_JSON_RET_STR;
                tld_strbuf_alloc(&r->value.string,16);
                tld_append(r->value.string, TLD_STR(v->value.str));
                break;
        case TLD_JSON_DBL:
                r->type = TLD_JSON_RET_DBL;
                r->value.d_num = v->value.d_num;
                break;
        case TLD_JSON_INT:
                r->type = TLD_JSON_RET_INT;
                r->value.d_num = v->value.i_num;
                break;
        case TLD_JSON_BOOL_TRUE:
                r->type = TLD_JSON_RET_BOOL;
                r->value.b_num = 1;
                break;
        case TLD_JSON_BOOL_FALSE:
                r->type = TLD_JSON_RET_BOOL;
                r->value.b_num = 0;
                break;
        case TLD_JSON_UNDEF:
                ERROR_MSG("Node has no type!");
                break;
        case TLD_JSON_ARR:
                arr = v->value.arr;
                n_elem = arr->n;
                r->n = n_elem;
                r->n_alloc = n_elem;
                for(int i = 0; i < 8;i++){
                        type[i] = 0;
                }
                for(int i = 0; i < n_elem;i++){
                        type[arr->v[i]->type]++;
                }
                max = -1;
                max_idx = -1;
                for(int i = 0; i < 8;i++){
                        if(type[i] > max){
                                max = type[i];
                                max_idx = i;
                        }
                }
                type[TLD_JSON_BOOL_TRUE] = type[TLD_JSON_BOOL_TRUE] + type[TLD_JSON_BOOL_FALSE];
                if(type[TLD_JSON_BOOL_TRUE] > max){
                        max = type[TLD_JSON_BOOL_TRUE];
                        max_idx = TLD_JSON_BOOL_TRUE;
                }

                if(max != n_elem){
                        ERROR_MSG("Mixed types (?) in json arr!");
                }
                switch (max_idx) {
                case TLD_JSON_DBL:
                        r->type = TLD_JSON_RET_DBL_ARR;
                        galloc(&r->value.dbl_arr, n_elem);
                        for(int i = 0; i < n_elem;i++){
                                r->value.dbl_arr[i] = arr->v[i]->value.d_num;
                        }
                        break;
                case TLD_JSON_INT:
                        r->type = TLD_JSON_RET_INT_ARR;
                        galloc(&r->value.int_arr, n_elem);
                        for(int i = 0; i < n_elem;i++){
                                r->value.int_arr[i] = arr->v[i]->value.i_num;
                        }
                        break;
                case TLD_JSON_BOOL_TRUE:
                case TLD_JSON_BOOL_FALSE:
                        r->type = TLD_JSON_RET_BOOL_ARR;
                        galloc(&r->value.bool_arr, n_elem);
                        for(int i = 0; i < n_elem;i++){
                                r->value.int_arr[i] = arr->v[i]->value.b_num;
                        }
                        break;
                default:
                        ERROR_MSG("Cannot parse %d json type.",max_idx);
                        break;
                }
                break;
        }
        *ret = r;
        return OK;
ERROR:
        return FAIL;
}

int tld_json_ret_alloc(tld_json_ret** ret)
{
        tld_json_ret* n = NULL;
        MMALLOC(n, sizeof(tld_json_ret));
        n->type = TLD_JSON_RET_UNDEF;
        n->n = 0;
        n->n_alloc = 0;
        *ret = n;
        return OK;
ERROR:
        tld_json_ret_free(n);
        return FAIL;
}

void tld_json_ret_free(tld_json_ret* n)
{
        if(n){
                switch (n->type) {
                case TLD_JSON_RET_UNDEF:
                case TLD_JSON_RET_BOOL:
                case TLD_JSON_RET_DBL:
                case TLD_JSON_RET_INT:
                        break;
                case TLD_JSON_RET_STR:
                        tld_strbuf_free(n->value.string);
                        break;
                case TLD_JSON_RET_BOOL_ARR:
                        gfree(n->value.bool_arr);
                        break;
                case TLD_JSON_RET_DBL_ARR:
                        gfree(n->value.dbl_arr);
                        break;
                case TLD_JSON_RET_INT_ARR:
                        gfree(n->value.int_arr);
                        break;
                }

                MFREE(n);
        }
}
