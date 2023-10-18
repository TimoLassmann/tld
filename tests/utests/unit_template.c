#include "tld.h"


int test_default(tld_template_map *map);
int test_mismatch(tld_template_map *map);
int test_mismatch2 (tld_template_map *map);
int main(void)
{

        char* id[] = {
                "AA",
                "BB",
        };

        char* rep[] = {
                "DOG",
                "CAT",
        };


        tld_template_map* map = NULL;

        tld_template_init(&map, id, rep, 2);

        LOG_MSG("Default template test");
        test_default(map);
        LOG_MSG("Mismatch template test");
        if(test_mismatch(map) == OK){
                ERROR_MSG("This test should FAIL!!!!");
        }

        LOG_MSG("Mismatch template test");
        if(test_mismatch2(map) == FAIL){
                ERROR_MSG("This test should PASS!!!!");
        }


        tld_template_free(map);
        /* LOG_MSG("Success"); */
        map = NULL;

        tld_template_add(&map, "AA", "Chair");
        tld_template_add(&map, "BB", "Docker");
        LOG_MSG("Default template test");
        test_default(map);
        LOG_MSG("Mismatch template test");
        if(test_mismatch(map) == OK){
                ERROR_MSG("This test should FAIL!!!!");
        }

        LOG_MSG("Mismatch template test");
        if(test_mismatch2(map) == FAIL){
                ERROR_MSG("This test should PASS!!!!");
        }
        LOG_MSG("Testing key retrieval");
        char*val = NULL;
        tld_template_get(map, "AA",&val);
        if(val){
                fprintf(stderr,"AA -> %s (retrieved)\n", val);
        }
        tld_template_get(map, "BB",&val);
        if(val){
                fprintf(stderr,"BB -> %s (retrieved)\n", val);
        }
        tld_template_free(map);
        /* LOG_MSG("Success"); */
        return EXIT_SUCCESS;
ERROR:
        /* LOG_MSG("FAIL"); */
        return EXIT_FAILURE;

}

int test_default(tld_template_map *map)
{

        char txt[] = "AAAA {{AA}} BBBB {{BB}} CCCC";
        tld_strbuf* template = NULL;

        tld_strbuf_alloc(&template, 64);
        tld_append(template, txt);
        LOG_MSG("%s", TLD_STR(template));
        tld_template_apply(template, map);
        LOG_MSG("%s", TLD_STR(template));
        if(tld_template_chk(map, 1) != OK){
                ERROR_MSG("default replacement test failed!");
        }
        tld_strbuf_free(template);
        return OK;
ERROR:
        if(template){
                tld_strbuf_free(template);
        }
        return FAIL;
}

int test_mismatch (tld_template_map *map)
{

        char txt[] = "AAAA {{AA} BBBB {{BB}} CCCC";
        tld_strbuf* template = NULL;

        tld_strbuf_alloc(&template, 64);
        tld_append(template, txt);
        LOG_MSG("%s", TLD_STR(template));
        tld_template_apply(template, map);
        LOG_MSG("%s", TLD_STR(template));
        if(tld_template_chk(map, 1) != OK){
                ERROR_MSG("mismatch test failed!");
        }
        tld_strbuf_free(template);
        return OK;
ERROR:
        if(template){
                tld_strbuf_free(template);
        }
        return FAIL;
}

int test_mismatch2 (tld_template_map *map)
{

        char txt[] = "AAAA {{AA}} BBBB {BB}} CCCC";
        tld_strbuf* template = NULL;

        tld_strbuf_alloc(&template, 64);
        tld_append(template, txt);
        LOG_MSG("%s", TLD_STR(template));
        tld_template_apply(template, map);
        LOG_MSG("%s", TLD_STR(template));
        if(tld_template_chk(map, 1) != OK){
                ERROR_MSG("mismatch test failed!");
        }
        tld_strbuf_free(template);
        return OK;
ERROR:
        if(template){
                tld_strbuf_free(template);
        }
        return FAIL;
}
