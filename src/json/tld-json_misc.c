

#include <string.h>

#define  TLD_JSON_MISC_IMPORT
#include "tld-json_misc.h"

/* Guess the JSON type of an unquoted token (used by JS()).
   The whole token must match: a number ([+-]digits[.digits][e[+-]digits],
   INT when there is no fraction or exponent) or one of
   true/True/TRUE/false/False/FALSE. Everything else, including the empty
   string, is a string. */
int detect_type(char*s, int len, tld_json_type* type)
{
        int i = 0;
        int mant = 0;
        int is_int = 1;

        *type = TLD_JSON_STR;
        if(s == NULL || len <= 0){
                return OK;
        }

        if(len == 4 && (strncmp(s, "true", 4) == 0 ||
                        strncmp(s, "True", 4) == 0 ||
                        strncmp(s, "TRUE", 4) == 0)){
                *type = TLD_JSON_BOOL_TRUE;
                return OK;
        }
        if(len == 5 && (strncmp(s, "false", 5) == 0 ||
                        strncmp(s, "False", 5) == 0 ||
                        strncmp(s, "FALSE", 5) == 0)){
                *type = TLD_JSON_BOOL_FALSE;
                return OK;
        }

        if(s[i] == '+' || s[i] == '-'){
                i++;
        }
        while(i < len && s[i] >= '0' && s[i] <= '9'){
                i++;
                mant++;
        }
        if(i < len && s[i] == '.'){
                is_int = 0;
                i++;
                while(i < len && s[i] >= '0' && s[i] <= '9'){
                        i++;
                        mant++;
                }
        }
        if(!mant){
                return OK;
        }
        if(i < len && (s[i] == 'e' || s[i] == 'E')){
                int ex = 0;
                is_int = 0;
                i++;
                if(i < len && (s[i] == '+' || s[i] == '-')){
                        i++;
                }
                while(i < len && s[i] >= '0' && s[i] <= '9'){
                        i++;
                        ex++;
                }
                if(!ex){
                        return OK;
                }
        }
        if(i != len){
                return OK;
        }
        *type = is_int ? TLD_JSON_INT : TLD_JSON_DBL;
        return OK;
}

int is_open_obj(tld_json_val *v)
{
        if(v->type == TLD_JSON_STR){
                if(v->value.str->str[0] == '{' && v->value.str->len == 1){
                        return 1;
                }
        }
        return 0;
}

int is_close_obj(tld_json_val *v)
{
        if(v->type == TLD_JSON_STR){
                if(v->value.str->str[0] == '}' && v->value.str->len == 1){
                        return 1;
                }
        }
        return 0;
}

int is_open_arr(tld_json_val *v)
{
        if(v->type == TLD_JSON_STR){
                if(v->value.str->str[0] == '[' && v->value.str->len == 1){
                        return 1;
                }
                /* LOG_MSG("%s (len: %d) in open arr test...",v->value.str->str, v->value.str->len); */
        }
        return 0;
}

int is_close_arr(tld_json_val *v)
{
        if(v->type == TLD_JSON_STR){
                if(v->value.str->str[0] == ']' && v->value.str->len == 1){
                        return 1;
                }
        }
        return 0;
}

int is_colon(tld_json_val *v)
{
        if(v->type == TLD_JSON_STR){

                if(v->value.str->str[0] == ':' && v->value.str->len == 1){
                        return 1;
                }
                /* LOG_MSG("%s in colon test...",v->value.str->str); */

        }
        return 0;
}

int is_comma(tld_json_val *v)
{
        if(v->type == TLD_JSON_STR){
                if(v->value.str->str[0] == ',' && v->value.str->len == 1){
                        return 1;
                }
        }
        return 0;
}
