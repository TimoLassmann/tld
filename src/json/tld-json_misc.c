

#define  TLD_JSON_MISC_IMPORT
#include "tld-json_misc.h"



int detect_type(char*s, int len, tld_json_type* type)
{
        tld_json_type l_type = TLD_JSON_UNDEF;
        int sign;
        int digit;
        int dot;
        int rest;
        int eeee;

        sign = 0;
        digit = 0;
        dot = 0;
        rest= 0;
        eeee = 0;

        for(int i = 0; i < len;i++){
                uint8_t c = s[i];
                switch (c) {
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                        digit++;
                        break;
                case '.':
                        dot++;
                        break;
                case '+':
                case '-':
                        sign++;
                        break;
                case 'e':
                case 'E':
                        eeee++;
                        break;
                default:
                        rest++;
                        break;
                }
        }

        /* logic  */
        if(eeee && !rest){
                digit=0;
                for(int i = 0; i < len;i++){
                        uint8_t c = s[i];
                        switch (c) {
                        case '0':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                        case '8':
                        case '9':
                                digit++;
                                break;
                        case 'e':
                        case 'E':
                                if(digit && i != len-1){
                                        l_type = TLD_JSON_DBL;
                                }else{
                                        l_type = TLD_JSON_STR;
                                }
                                eeee++;
                                break;
                        default:
                                break;
                        }

                }
        }else if(rest){
                /* LOG_MSG("Think this is a string(rest: %d)",rest); */
                l_type = TLD_JSON_STR;
                /* cell->type = RT_CELL_STRING; */
        }else if(rest ==0 && eeee == 1){
                l_type = TLD_JSON_DBL;
                /* cell->type = RT_CELL_DOUBLE; */
        }else if(sign > 1){
                /* LOG_MSG("Think this is a string (> 2 signs)"); */
                l_type = TLD_JSON_STR;
                /* cell->type = RT_CELL_STRING; */
        }else if(dot > 1){
                /* LOG_MSG("Think this is a string (> 2 signs)"); */
                l_type = TLD_JSON_STR;
                /* cell->type = RT_CELL_STRING; */
        }else if(dot == 1){
                l_type = TLD_JSON_DBL;
                /* LOG_MSG("Think this is a double"); */
                /* cell->type = RT_CELL_DOUBLE; */
        }else{
                /* LOG_MSG("Think this is an int: %s",s); */
                l_type = TLD_JSON_INT;
                /* cell->type = RT_CELL_INTEGER; */
        }

        if(l_type == TLD_JSON_STR && len < 6){

                if(s[0] == 'T' || s[0] == 'F' || s[0] == 't' || s[0] == 'f'){
                        if(strncmp(s, "FALSE", 5) == 0){
                                l_type = TLD_JSON_BOOL_FALSE;
                        }else if(strncmp(s, "False", 5) == 0){
                                l_type = TLD_JSON_BOOL_FALSE;
                        }else if(strncmp(s, "false", 5) == 0){
                                l_type = TLD_JSON_BOOL_FALSE;
                        }else if(strncmp(s, "TRUE", 4) == 0){
                                l_type = TLD_JSON_BOOL_TRUE;
                        }else if(strncmp(s, "True", 4) == 0){
                                l_type = TLD_JSON_BOOL_TRUE;
                        }else if(strncmp(s, "true", 4) == 0){
                                l_type = TLD_JSON_BOOL_TRUE;
                        }
                }
        }
        /* LOG_MSG("%s (len: %d)-> type: %d", s,len, l_type); */
        *type = l_type;
        return OK;
}

int is_open_obj(tld_json_val *v)
{
        if(v->type == TLD_JSON_STR){
                if(v->value.str->str[0] == '{'){
                        return 1;
                }
        }
        return 0;
}

int is_close_obj(tld_json_val *v)
{
        if(v->type == TLD_JSON_STR){
                if(v->value.str->str[0] == '}'){
                        return 1;
                }
        }
        return 0;
}

int is_open_arr(tld_json_val *v)
{
        if(v->type == TLD_JSON_STR){
                if(v->value.str->str[0] == '['){
                        return 1;
                }
        }
        return 0;
}

int is_close_arr(tld_json_val *v)
{
        if(v->type == TLD_JSON_STR){
                if(v->value.str->str[0] == ']'){
                        return 1;
                }
        }
        return 0;
}

int is_colon(tld_json_val *v)
{
        if(v->type == TLD_JSON_STR){
                if(v->value.str->str[0] == ':'){
                        return 1;
                }
        }
        return 0;
}

int is_comma(tld_json_val *v)
{
        if(v->type == TLD_JSON_STR){
                if(v->value.str->str[0] == ','){
                        return 1;
                }
        }
        return 0;
}
