#include "tld.h"

int test_new_template(void);
int test_get_vars_template(void);
int main(void)
{
        RUN(test_new_template());
        RUN(test_get_vars_template());
        return EXIT_SUCCESS;
ERROR:
        /* LOG_MSG("FAIL"); */
        return EXIT_FAILURE;
}

int test_get_vars_template(void)
{
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

        tld_hash* h = NULL;
        tld_strbuf* template = NULL;
        tld_strbuf_alloc(&template, 64);

        /* tld_hash_init(&map, identifiers, replacements, 4); */
        /* num_test_templates = 2; */
        for (int i = 0; i < num_test_templates; i++) {
                tld_strbuf_clear(template);
                tld_append(template, test_templates[i]);
                /* const char *template = test_templates[i]; */
                LOG_MSG("--------------------------------------");
                LOG_MSG("%s", TLD_STR(template));
                if(tld_template_extract_var(template, &h) == OK){
                        RUN(tld_hash_iter_init(h));
                        char* key = NULL;
                        char* value = NULL;
                        while(tld_hash_iterator_next(h, &key, &value) == OK){
                                fprintf(stdout, "  Key: %s\nValue: %s\n", key, value);
                                /* LOG_MSG("Key: %s Value: %s", key, value); */
                        }
                        tld_hash_free(h);

                        h = NULL;
                }

        }
        tld_strbuf_free(template);


        return OK;
ERROR:
        return FAIL;

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

        tld_hash* h = NULL;
        tld_strbuf* template = NULL;
        tld_strbuf_alloc(&template, 64);
        tld_hash_add(&h, "name", "Alica");
        tld_hash_add(&h, "age", "101");
        tld_hash_add(&h, "is_student", "yes");
        tld_hash_add(&h, "major", "Computer Science");

        /* tld_hash_init(&map, identifiers, replacements, 4); */
        /* num_test_templates = 2; */
        for (int i = 0; i < num_test_templates; i++) {
                tld_strbuf_clear(template);
                tld_append(template, test_templates[i]);
                /* const char *template = test_templates[i]; */
                LOG_MSG("--------------------------------------");
                LOG_MSG("%s", TLD_STR(template));
                tld_template_apply(template, h);
                LOG_MSG("%s", TLD_STR(template));
        }
        tld_strbuf_free(template);
        tld_hash_free(h);

        return OK;
ERROR:
        return FAIL;
}

