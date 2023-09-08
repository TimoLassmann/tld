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
        if(test_mismatch2(map) == OK){
                ERROR_MSG("This test should FAIL!!!!");
        }


        tld_template_free(map);
        return EXIT_SUCCESS;
ERROR:
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
        return FAIL;
}
