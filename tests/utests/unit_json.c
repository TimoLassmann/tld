#include "tld.h"

int test_file_parser(char *infile);
int test_obj_parser(void);
int test_parser(void);

int main(int argc, char *argv[])
{
        if(argc > 1){

                RUN(test_file_parser(argv[1]));
        }else{
        /* test_obj_parser(); */
        /* exit(0); */
                test_parser();
        }

        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
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


        tld_strbuf* res = NULL;
        tld_strbuf_alloc(&res, 16);

        tld_json_obj_get_str(n, "name",res);

        LOG_MSG("RESULT:::: %s", TLD_STR(res));

        tld_json_obj_free(n);
        tld_strbuf_free(buf);
        tld_strbuf_free(res);
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
        tld_json_obj_free(n);
        tld_strbuf_free(buf);

        return OK;
}
