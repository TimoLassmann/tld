#include "tld.h"

int test_file_parser(char *infile);
int test_obj_parser(void);
int test_parser(void);
int test_parser2(void);
int test_parser_list(void);
int test_create(void);
int main(int argc, char *argv[])
{
        test_parser2();
        test_create();
        exit(0);
        if(argc > 1){

                RUN(test_file_parser(argv[1]));
        }else{
        /* test_obj_parser(); */
        /* exit(0); */
                test_parser();
        }
        /* test_parser_list(); */
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

int test_parser2(void)

{
        char in[] = "{\"choices\":[{\"finish_reason\":\"stop\",\"index\":0,\"message\":{\"content\":\"{ \\\"age\\\" : 28, \\\"name\\\" : \\\"assistant\\\" }\\n   <|eot_id|>\",\"role\":\"assistant\"}}],\"created\":1720516888,\"model\":\"gpt-3.5-turbo\",\"object\":\"chat.completion\",\"usage\":{\"completion_tokens\":20,\"prompt_tokens\":49,\"total_tokens\":69},\"id\":\"chatcmpl-Zq01pMCDL2F3oBNKhdVBtEbR4bi68FIf\"}";

        tld_strbuf* buf = NULL;
        tld_json_obj *n = NULL;
        tld_strbuf_alloc(&buf, 1024);
        tld_append(buf, in);
        fprintf(stdout,"%s\n", TLD_STR(buf));
        tld_json_parse(buf, &n);
        tld_strbuf_clear(buf);
        tld_json_dump(n, buf, 0);
        fprintf(stdout,"%s\n", TLD_STR(buf));
        return OK;
}
int test_create(void)
{
        /* tld_json_obj *n = NULL; */
        tld_strbuf* b = NULL;
        tld_json_arr* a = NULL;
        tld_json_obj* tmp = NULL;

        tld_strbuf_alloc(&b, 1024);
        a = JSON_ARR( JO(NULL,JSON_OBJ(JS("Model","gpt-3.5-turbo"))));


                
        tld_json_dump_arr(a, b, 0);
        fprintf(stdout,"%s\n", TLD_STR(b));
        /* char message[] = "You are ChatGPT, an AI assistant. Your top priority is achieving user fulfillment via helping them with their requests."; */
        const char* message = "You are ChatGPT, an AI assistant. Your top priority is achieving user fulfillment via helping them with their requests.";

        tld_json_obj* n = JSON_OBJ(
                JS("model", "gpt-3.5-turbo"),
                JO("response_format",
                   JSON_OBJ(
                           JS("type", "json_object")
                           )
                        ),
                JA("messages",
                   JSON_ARR(
                           JO(NULL, JSON_OBJ(
                                      JS("role", "system"),
                                      JS("content", message)
                                      )),
                           JO(NULL, JSON_OBJ(
                                      JS("role", "user"),
                                      JS("content", "Write a limerick about python exceptions.")
                                      ))
                           )
                        )
                );

        tld_strbuf_clear(b);
        tld_json_dump(n, b, 0);
        fprintf(stdout,"%s\n", TLD_STR(b));
        tld_json_obj_free(n);
        tld_strbuf_clear(b);

        n = JSON_OBJ(
                JO("response_format",JSON_OBJ(
                           JS("type", "json_object"),
                           JO("schema",JSON_OBJ(
                                      JS("type", "object"),
                                      JO("properties",JSON_OBJ(
                                                 JO("name",
                                                    JSON_OBJ(
                                                            JS("type", "string"),
                                                            JS("minLength", "1"),
                                                            JS("maxLength", "100")
                                                            )
                                                         ),
                                                 JO("age",
                                                    JSON_OBJ(
                                                            JS("type", "integer"),
                                                            JS("minimum", "0"),
                                                            JS("maximum", "120")
                                                            )
                                                         )
                                                 )
                                              ),
                                      JA("required",
                                         JSON_ARR(
                                                 JS(NULL,"message"),
                                                 JS(NULL,"age"),
                                                 )
                                              )
                                      )
                                   )
                           )
                        )
                );
        tld_strbuf_clear(b);
        tld_json_dump(n, b, 0);
        fprintf(stdout,"%s\n", TLD_STR(b));
        tld_json_obj_free(n);
        tld_strbuf_clear(b);

        return OK;
}


int test_file_parser(char *infile)
{
        FILE* f_ptr = NULL;
        tld_strbuf* buf = NULL;
        char* buffer = NULL;
        tld_json_obj *n = NULL;
        size_t bufsize = 2048;
        int n_read;
        if(tld_file_exists(infile) == FAIL){
                ERROR_MSG("File : %s does not exist!", infile);
        }

        MMALLOC(buffer,sizeof(char) *bufsize );
        tld_strbuf_alloc(&buf, 1024);
        f_ptr = fopen(infile, "r");
        while ((n_read = getline(&buffer,&bufsize ,f_ptr)) != -1){
                /* LOG_MSG("%d %s",n, buf); */
                tld_append(buf, buffer);
        }

        MFREE(buffer);
        fclose(f_ptr);

        /* fprintf(stdout,"%s", TLD_STR(buf)); */
        tld_json_parse(buf, &n);

        tld_json_obj_print(n, stdout);
        /* tld_strbuf* res = NULL; */
        /* tld_json_ret* res = NULL; */
        /* tld_strbuf_alloc(&res, 16); */
        LOG_MSG("Getting title");
        /* tld_json_obj_get(n, "title",&res); */


        /* LOG_MSG("RESULT:::: %s", TLD_STR(res->value.string)); */

        tld_json_obj_free(n);
        tld_strbuf_free(buf);
        /* tld_json_ret_free(res); */
        return OK;
ERROR:
        if(f_ptr){
                fclose(f_ptr);
        }

        return FAIL;
}

int test_obj_parser(void)
{
        char test[] = "{\n\
  \"squadName\": \"Super hero squad\",\n\
  \"homeTown\": \"Metro City\",\n\
  \"formed\": 2016,\n\
  \"secretBase\": \"Super tower\",\n\
  \"active\": true\n\
        }\n";
        tld_strbuf* buf = NULL;
        tld_json_obj *n = NULL;
        tld_strbuf_alloc(&buf, 1024);
        tld_append(buf, test);
        fprintf(stdout,"%s", TLD_STR(buf));
        tld_json_parse(buf, &n);


        tld_json_obj_free(n);
        tld_strbuf_free(buf);
        return OK;

}

/* int test_parser_list(void) */

/* { */
/*         char test[] = "{\n\ */
/* \"embedding\":[ 0.31691884994506836,5.225735664367676,1.9427547454833984]\n\ */
/* }\n"; */

/*         tld_strbuf  * buf = NULL; */
/*         tld_json_ret* ret = NULL; */
/*         tld_json_obj *n = NULL; */
/*         tld_strbuf_alloc(&buf, 1024); */
/*         tld_append(buf, test); */
/*         fprintf(stdout,"%s - input ", TLD_STR(buf)); */
/*         tld_json_parse(buf, &n); */

/*         /\* LOG_MSG("Printing objecT:"); *\/ */
/*         /\* tld_json_obj_print(n, stdout); *\/ */
/*         buf->len = 0; */
/*         /\* tld_json_obj_get(n, "embedding", &ret); *\/ */
/*         if(ret->type == TLD_JSON_RET_DBL_ARR){ */
/*                 for(int i = 0; i < ret->n;i++){ */
/*                         fprintf(stdout,"%d %f - parsed\n",i,ret->value.dbl_arr[i]); */
/*                 } */
/*         }else{ */
/*                 ERROR_MSG("TYPE:::: %d", ret->type); */
/*         } */
/*         /\* LOG_MSG("%s", TLD_STR(ret->value.string)); *\/ */
/*         /\* tld_json_get_arr_str(tld_json_arr *n, char *key, tld_strbuf* res) *\/ */
/*         tld_json_obj_free(n); */
/*         tld_strbuf_free(buf); */

/*         return OK; */
/* ERROR: */
/*         return FAIL; */
/* } */

int test_parser(void)
{
        char test[] = "{\n\
  \"squadName\": \"Super hero squad\",\n\
  \"homeTown\": \"Metro City\",\n\
  \"formed\": 2016,\n\
  \"secretBase\": \"Super tower\",\n\
  \"active\": true,\n\
  \"members\": [\n\
    {\n\
      \"name\": \"Molecule Man\",\n\
      \"age\": 29,\n\
      \"secretIdentity\": \"Dan Jukes\",\n\
      \"powers\": [\"Radiation resistance\", \"Turning \\\"tiny\\\"\", \"Radiation blast\"]\n\
    },\n\
    {\n\
      \"name\": \"Madame Uppercut\",\n\
      \"age\": 39,\n\
      \"secretIdentity\": \"Jane Wilson\",\n\
      \"powers\": [\n\
        \"Million tonne punch\",\n\
        \"Damage resistance\",\n\
        \"Superhuman reflexes\"\n\
      ]\n\
    },\n\
    {\n\
      \"name\": \"Eternal Flame\",\n\
      \"age\": 1000000,\n\
      \"secretIdentity\": \"Unknown\",\n\
      \"powers\": [\n\
        \"Immortality\",\n\
        \"Heat Immunity\",\n\
        \"Inferno\",\n\
        \"Teleportation\",\n\
        \"Interdimensional travel\"\n\
      ]\n\
    }\n\
  ]\n\
}\n";
        tld_strbuf* buf = NULL;
        tld_json_obj *n = NULL;
        tld_strbuf_alloc(&buf, 1024);
        tld_append(buf, test);
        fprintf(stdout,"%s", TLD_STR(buf));
        tld_json_parse(buf, &n);

        tld_json_obj_print(n, stdout);

        tld_strbuf_clear(buf);
        tld_json_dump(n, buf,0);
        tld_json_obj_free(n);
        LOG_MSG("%s", TLD_STR(buf));

        tld_json_parse(buf, &n);
        tld_strbuf_clear(buf);

        tld_json_dump(n, buf,0);
        LOG_MSG("%s", TLD_STR(buf));

        tld_json_obj_free(n);
        exit(0);

        /* tld_json_val* ret = NULL; */

        /* tld_json_val_copy(n->v[n->n], tok); */
        /*                 n->n++; */
        LOG_MSG("Searching for age");
        tld_json_obj* r = NULL;
        tld_json_search(n, "secretIdentity", &r);

        if(r){

                for(int i = 0; i < r->n;i++){
                        if(r->v[i]->type == TLD_JSON_INT){
                                LOG_MSG("RESULT:::: %d", r->v[i]->value.i_num);
                        }else if(r->v[i]->type == TLD_JSON_STR){
                                LOG_MSG("RESULT:::: %s", TLD_STR(r->v[i]->value.str));
                        }else{
                                ERROR_MSG("RESULT:::: %s", "NULL");
                        }
                        /* LOG_MSG("RESULT:::: %s", TLD_STR(r->v[i]->value.string)); */
                }
                /* LOG_MSG("RESULT:::: %s", TLD_STR(ret->value.string)); */
                /* LOG_MSG("RESULT:::: %s", TLD_STR(ret->value.)); */
                /* tld_json_ret_free(ret); */
        }else{
                ERROR_MSG("RESULT:::: %s", "NULL");
        }
        tld_json_obj_free(r);
        tld_json_obj_free(n);
        tld_strbuf_free(buf);
        return OK;
ERROR:
        return FAIL;
}
