#include "tld.h"


int test_default(tld_template_map *map);
int test_mismatch(tld_template_map *map);
int test_mismatch2(tld_template_map *map);
int test_norep (tld_template_map *map);
int test_ifrep(tld_template_map *map);
int test_new_template(void);


int main(void)
{
        test_new_template();
        exit(0);
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
        LOG_MSG("Noreplacement template test");
        if(test_norep(map) == FAIL){
                ERROR_MSG("This test should not FAIL!!!!");
        }
        LOG_MSG("If replacement template test");
        if(test_ifrep(map) == OK){
                ERROR_MSG("This test should FAIL!!!!");
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

int test_new_template(void)
{
        // Array of test templates
        char* test_templates[] = {
                // Basic template with variable substitution
                "Hello, {{name}}!",
                // IF block without a variable
                "{{IF age}}You are 12 years old.{{ENDIF}}",

                // IF block with a variable
                "{{IF age}}You are {{age}} years old.{{ENDIF}}",

                // IF-ELSE block with variable substitution
                "{{IF age}}You are {{age}} years old.{{ELSE}}Your age is unknown.{{ENDIF}}",

                // Nested IF statements with variable substitution
                "{{IF is_student}}You are a student.{{IF has_major}}Your major is {{major}}.{{ENDIF}}{{ENDIF}}",

                // Empty variables
                "Hello, {{empty_var}}!",

                // Unused variables (variables in the map but not in the template)
                "Hello, {{name}}! Today is {{day}}.",

                // IF block with no ELSE, and variable doesn't exist
                "{{IF is_employed}}You are employed.{{ENDIF}}",

                // Nested IF-ELSE blocks with conditionally missing variables
                "{{IF is_student}}Welcome, student! {{IF major}}Your major is {{major}}.{{ELSE}}Major unknown.{{ENDIF}}{{ELSE}}Welcome, guest!{{ENDIF}}",

                // Multiple variables in sequence
                "{{name}} is {{age}} years old and studying {{major}}.",

                // IF block with multiple variables
                "{{IF age}}Hello {{name}}, you are {{age}} years old.{{ENDIF}}",

                // ELSE without IF (should trigger an error in a well-formed parser)
                "{{ELSE}}This should not appear.{{ENDIF}}",

                // Mismatched IF and ENDIF (should trigger an error)
                "{{IF name}}Hello {{name}}.",

                // Properly nested IF blocks
                "{{IF name}}Hello {{name}}! {{IF age}}You are {{age}} years old.{{ENDIF}}{{ENDIF}}",

                // Mixed variables and plain text
                "Your name is {{name}}. You are {{age}}. {{IF is_student}}You are a student.{{ENDIF}} Enjoy your day.",

                // No variables, just plain text
                "This is a simple template with no variables.",

                // Edge case: Variables inside conditionals, but without valid map entries
                "{{IF missing_var}}This variable is missing: {{missing_var}}{{ELSE}}No variable found.{{ENDIF}}",

                // Multiple conditional branches
                "{{IF age}}Age: {{age}}{{ELSE}}No age provided.{{ENDIF}} {{IF name}}Name: {{name}}{{ELSE}}No name provided.{{ENDIF}}",

                // Long template with multiple variables and conditionals
                "Hello {{name}}! {{IF age}}You are {{age}} years old.{{ELSE}}Your age is unknown.{{ENDIF}} {{IF is_student}}You are a student studying {{major}}.{{ENDIF}}"
        };

        // Size of the array
        int num_test_templates = sizeof(test_templates) / sizeof(test_templates[0]);

        // Create a sample template map
        char* identifiers[] = {"name", "age",  "is_student","major"};
        char* replacements[] = {"Alice","101", "yes", "Computer Science"};
        tld_template_map* map = NULL;
        tld_strbuf* template = NULL;
        tld_strbuf_alloc(&template, 64);
        tld_template_init(&map, identifiers, replacements, 4);
        /* num_test_templates = 2; */
        for (int i = 0; i < num_test_templates; i++) {
                tld_strbuf_clear(template);
                tld_append(template, test_templates[i]);
                /* const char *template = test_templates[i]; */
                LOG_MSG("--------------------------------------");
                LOG_MSG("%s", TLD_STR(template));
                tld_template_apply(template, map);
                LOG_MSG("%s", TLD_STR(template));
        }
        tld_strbuf_free(template);
        tld_template_free(map);

        return OK;
ERROR:
        return FAIL;
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

int test_norep (tld_template_map *map)
{

        char txt[] = "AAAA  BBBB  CCCC";
        tld_strbuf* template = NULL;

        tld_strbuf_alloc(&template, 64);
        tld_append(template, txt);
        LOG_MSG("%s - before", TLD_STR(template));
        tld_template_apply(template, map);
        LOG_MSG("%s - after", TLD_STR(template));
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


int test_ifrep (tld_template_map *map)
{

        char txt[] = "AAAA {{AA}} BBBB {{IF BB}} CCCC {{BB}} {{ENDIF}}";

        tld_strbuf* template = NULL;

        tld_strbuf_alloc(&template, 64);
        tld_append(template, txt);
        LOG_MSG("%s - before", TLD_STR(template));
        tld_template_apply(template, map);
        LOG_MSG("%s - after", TLD_STR(template));
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
