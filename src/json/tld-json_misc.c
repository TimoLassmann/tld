

#include <string.h>
#include "../string/str.h"

#include "tld-json_misc.h"

/* Guess the JSON type of an unquoted token (used by JS()).
   The whole token must match: a number ([+-]digits[.digits][e[+-]digits],
   INT when there is no fraction or exponent) or one of
   true/True/TRUE/false/False/FALSE. Everything else, including the empty
   string, is a string. */
int tld__json_detect_type(char*s, int len, tld_json_type* type)
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
