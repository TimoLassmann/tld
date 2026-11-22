#include "tld.h"
#include "test.h"
#include <stdio.h>

int test_file_parser(char *infile);
int test_obj_parser(void);
int test_parser(void);
int test_parser2(void);
int test_parser_list(void);
int test_create(void);

int main(int argc, char *argv[])
{
        test_init(&argc, argv, "[FILE]",
                  "With no arguments: parses and dumps two chat-completion replies, searches\n"
                  "one for \"content\", and builds objects with TLD_JSON_OBJECT/TLD_JSON_ARRAY/TLD_JAUTO/TLD_JOBJ/TLD_JARR\n"
                  "(including a NULL member and tld_json_set_val).\n"
                  "With FILE: reads the JSON file, parses it with tld_json_parse and dumps it to\n"
                  "stdout with tld_json_dump.\n"
                  "Example: unit-json data.json");
        if(argc > 1){
                RUN(test_file_parser(argv[1]));
                return EXIT_SUCCESS;
        }
        SECTION_MSG("parse, dump and search");
        RUN(test_parser2());
        OK_MSG("parsed and dumped");
        SECTION_MSG("build objects with the JSON_* macros");
        RUN(test_create());
        OK_MSG("built and dumped");
        /* test_parser(), test_obj_parser() and test_parser_list() are not run */
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

int test_parser2(void)
{
        char in[] = "{\"choices\":[{\"finish_reason\":\"stop\",\"index\":0,\"message\":{\"content\":\"{ \\\"age\\\" : 28, \\\"name\\\" : \\\"assistant\\\" }\\n   <|eot_id|>\",\"role\":\"assistant\"}}],\"created\":1720516888,\"model\":\"gpt-3.5-turbo\",\"object\":\"chat.completion\",\"usage\":{\"completion_tokens\":20,\"prompt_tokens\":49,\"total_tokens\":69},\"id\":\"chatcmpl-Zq01pMCDL2F3oBNKhdVBtEbR4bi68FIf\"}";

        tld_strbuf* buf = NULL;
        tld_json_obj *n = NULL;
        tld_json_obj *res = NULL;
        tld_strbuf_alloc(&buf, 1024);
        tld_strbuf_append(buf, in);
        DEBUG_MSG("%s", TLD_STR(buf));
        tld_json_parse(TLD_STR(buf), &n);
        tld_strbuf_clear(buf);
        tld_json_dump(n, buf, 0);
        DEBUG_MSG("%s", TLD_STR(buf));

        tld_json_search(n, "content", &res);
        tld_json_obj_free(n);

        tld_strbuf_clear(buf);
        tld_json_dump(res, buf, 0);
        DEBUG_MSG("%s", TLD_STR(buf));
        tld_json_obj_free(res);

        char in2[] = "{\"choices\":[{\"finish_reason\":\"stop\",\"index\":0,\"message\":{\"content\":\"Let me see...\n\nI've got it!\n\nMeet Kaelin \\\"Kae\\\" Valtor, a 32-year-old skilled astrobiologist and explorer from the planet of Xeridia. Kae is a brilliant scientist with a passion for uncovering the secrets of the universe. Born on a remote research station\",\"role\":\"assistant\"}}],\"created\":1720583143,\"model\":\"gpt-3.5-turbo\",\"object\":\"chat.completion\",\"usage\":{\"completion_tokens\":64,\"prompt_tokens\":58,\"total_tokens\":122},\"id\":\"chatcmpl-C0uWNFiitSmQwefVTnsllXDvcilXoIah\"}";
        tld_strbuf_clear(buf);
        tld_strbuf_append(buf, in2);
        KV_MSG("input", "message 2");
        DEBUG_MSG("%s", TLD_STR(buf));
        tld_json_parse(TLD_STR(buf), &n);
        tld_strbuf_clear(buf);
        tld_json_dump(n, buf, 0);
        OK_MSG("message 2 parsed");
        DEBUG_MSG("%s", TLD_STR(buf));
        tld_strbuf_free(buf);
        tld_json_obj_free(n);
        return OK;
}

int test_create(void)
{
        tld_strbuf* b = NULL;
        tld_json_arr* a = NULL;

        tld_strbuf_alloc(&b, 1024);
        a = TLD_JSON_ARRAY( TLD_JOBJ(NULL,TLD_JSON_OBJECT(TLD_JAUTO("Model","gpt-3.5-turbo"))));

                
        tld_json_dump_arr(a, b, 0);
        DEBUG_MSG("%s", TLD_STR(b));

        tld_json_arr_free(a);
        const char* message = "You are ChatGPT, an AI assistant. Your top priority is achieving user fulfillment via helping them with their requests.";

        tld_json_obj* n = TLD_JSON_OBJECT(
                TLD_JAUTO("model", "gpt-3.5-turbo"),
                TLD_JOBJ("response_format",
                   TLD_JSON_OBJECT(
                           TLD_JAUTO("type", "json_object")
                           )
                        ),
                TLD_JARR("messages",
                   TLD_JSON_ARRAY(
                           TLD_JOBJ(NULL, TLD_JSON_OBJECT(
                                      TLD_JAUTO("role", "system"),
                                      TLD_JAUTO("content", message)
                                      )),
                           TLD_JOBJ(NULL, TLD_JSON_OBJECT(
                                      TLD_JAUTO("role", "user"),
                                      TLD_JAUTO("content", "Write a limerick about python exceptions.")
                                      ))
                           )
                        )
                );

        tld_strbuf_clear(b);
        tld_json_dump(n, b, 0);
        DEBUG_MSG("%s", TLD_STR(b));
        tld_json_obj_free(n);
        tld_strbuf_clear(b);

        n = TLD_JSON_OBJECT(
                TLD_JOBJ("response_format",TLD_JSON_OBJECT(
                           TLD_JAUTO("type", "json_object"),
                           TLD_JOBJ("schema",TLD_JSON_OBJECT(
                                      TLD_JAUTO("type", "object"),
                                      TLD_JOBJ("properties",TLD_JSON_OBJECT(
                                                 TLD_JOBJ("name",
                                                    TLD_JSON_OBJECT(
                                                            TLD_JAUTO("type", "string"),
                                                            TLD_JAUTO("minLength", "1"),
                                                            TLD_JAUTO("maxLength", "100")
                                                            )
                                                         ),
                                                 TLD_JOBJ("age",
                                                    TLD_JSON_OBJECT(
                                                            TLD_JAUTO("type", "integer"),
                                                            TLD_JAUTO("minimum", "0"),
                                                            TLD_JAUTO("maximum", "120")
                                                            )
                                                         )
                                                 )
                                              ),
                                      TLD_JARR("required",
                                         TLD_JSON_ARRAY(
                                                 TLD_JAUTO(NULL,"message"),
                                                 TLD_JAUTO(NULL,"age"),
                                                 )
                                              )
                                      )
                                   )
                           )
                        )
                );
        tld_strbuf_clear(b);
        tld_json_dump(n, b, 0);
        DEBUG_MSG("%s", TLD_STR(b));
        tld_json_obj_free(n);
        LOG_MSG("Create a JSON OBJECT with a NULL schema");
        n = TLD_JSON_OBJECT(
                TLD_JOBJ("response_format",TLD_JSON_OBJECT(
                           TLD_JAUTO("type", "json_object"),
                           TLD_JOBJ("schema",TLD_JSON_OBJECT(
                                      TLD_JAUTO("type", "object"),
                                      TLD_JOBJ("properties",TLD_JSON_OBJECT(
                                                 TLD_JOBJ("name",NULL),
                                                 TLD_JOBJ("age",
                                                    TLD_JSON_OBJECT(
                                                            TLD_JAUTO("type", "integer"),
                                                            TLD_JAUTO("minimum", "0"),
                                                            TLD_JAUTO("maximum", "120")
                                                            )
                                                         )
                                                 )
                                              ),
                                      TLD_JARR("required",
                                         TLD_JSON_ARRAY(
                                                 TLD_JAUTO(NULL,"message"),
                                                 TLD_JAUTO(NULL,"age"),
                                                 )
                                              )
                                      )
                                   )
                           )
                        )
                );

        tld_strbuf_clear(b);
        tld_json_dump(n, b, 0);
        DEBUG_MSG("%s", TLD_STR(b));

        LOG_MSG("Create a JSON OBJECT with a NULL schema - Done");
        tld_json_val* v = NULL;
        tld_json_val_alloc(&v);
        v->type = TLD_JSON_STR;
        tld_strbuf_alloc(&v->value.str, 64);
        tld_strbuf_append(v->value.str, "AGARGAGAGA");
        tld_json_set_val(n, "type", v);
        tld_json_val_free(v);
        tld_json_dump(n, b, 0);
        DEBUG_MSG("%s", TLD_STR(b));

        LOG_MSG("Create a JSON OBJECT with a NULL schema - Done");

        tld_json_obj_free(n);

        tld_strbuf_clear(b);
        tld_strbuf_free(b);

        return OK;
}

int test_file_parser(char *infile)
{
        FILE* f_ptr = NULL;
        tld_strbuf* buf = NULL;
        char* buffer = NULL;
        tld_json_obj *n = NULL;
        size_t bufsize = 2048;
        if(tld_path_exists(infile) == FAIL){
                ERROR_MSG("File : %s does not exist!", infile);
        }
        SECTION_MSG("%s", infile);
        MMALLOC(buffer,sizeof(char) *bufsize );
        RUN(tld_strbuf_alloc(&buf, 1024));
        f_ptr = fopen(infile, "r");
        if(!f_ptr){
                ERROR_MSG("cannot open %s", infile);
        }
        while (getline(&buffer,&bufsize ,f_ptr) != -1){
                RUN(tld_strbuf_append(buf, buffer));
        }
        MFREE(buffer);
        fclose(f_ptr);
        f_ptr = NULL;
        KV_MSG("bytes", "%d", TLD_STRLEN(buf));

        RUN(tld_json_parse(TLD_STR(buf), &n));
        OK_MSG("parsed");
        RUN(tld_strbuf_clear(buf));
        RUN(tld_json_dump(n, buf, 0));
        fprintf(stdout, "%s\n", TLD_STR(buf));

        tld_json_obj_free(n);
        tld_strbuf_free(buf);
        return OK;
ERROR:
        if(f_ptr){
                fclose(f_ptr);
        }
        if(buffer){
                MFREE(buffer);
        }
        if(buf){
                tld_strbuf_free(buf);
        }
        if(n){
                tld_json_obj_free(n);
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
        tld_strbuf_append(buf, test);
        fprintf(stdout,"%s", TLD_STR(buf));
        tld_json_parse(TLD_STR(buf), &n);

        tld_json_obj_free(n);
        tld_strbuf_free(buf);
        return OK;

}

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
    },\n \
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
        tld_strbuf_append(buf, test);
        fprintf(stdout,"%s", TLD_STR(buf));
        tld_json_parse(TLD_STR(buf), &n);

        RUN(tld_strbuf_clear(buf));
        RUN(tld_json_dump(n, buf, 0));
        fprintf(stdout, "%s\n", TLD_STR(buf));

        tld_strbuf_clear(buf);
        tld_json_dump(n, buf,0);
        tld_json_obj_free(n);
        LOG_MSG("%s", TLD_STR(buf));

        tld_json_parse(TLD_STR(buf), &n);
        tld_strbuf_clear(buf);

        tld_json_dump(n, buf,0);
        LOG_MSG("%s", TLD_STR(buf));

        tld_json_obj_free(n);
        exit(0);

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
                }
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
