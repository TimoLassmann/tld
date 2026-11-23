#include "tld.h"
#include "test.h"
#include <stdio.h>
#include <string.h>

int test_new_template(void);
int test_get_vars_template(void);

/* unit-template TEMPLATE [key=value ...]: apply the template to the values. */
static int demo(int argc, char **argv)
{
        tld_hash *h = NULL;
        tld_hash *vars = NULL;
        tld_strbuf *t = NULL;
        const char *key = NULL;
        const char *value = NULL;
        RUN(tld_strbuf_alloc(&t, 64));
        RUN(tld_strbuf_append(t, argv[1]));
        RUN(tld_hash_alloc(&h, 16));
        SECTION_MSG("template");
        KV_MSG("template", "%s", argv[1]);
        for(int i = 2; i < argc; i++){
                char *eq = strchr(argv[i], '=');
                ASSERT(eq != NULL && eq != argv[i], "expected key=value, got %s", argv[i]);
                *eq = 0;
                RUN(tld_hash_add(&h, argv[i], eq + 1));
                KV_MSG(argv[i], "%s", eq + 1);
        }
        if(tld_template_vars(t, &vars) == OK){
                RUN(tld_hash_iter_init(vars));
                while(tld_hash_iter_next(vars, &key, &value) == OK){
                        DEBUG_MSG("template uses variable %s", key);
                }
        }
        RUN(tld_template_apply(t, h));
        fprintf(stdout, "%s\n", TLD_STR(t));
        OK_MSG("applied");
        tld_hash_free(vars);
        tld_hash_free(h);
        tld_strbuf_free(t);
        return OK;
ERROR:
        tld_hash_free(vars);
        tld_hash_free(h);
        tld_strbuf_free(t);
        return FAIL;
}

int main(int argc, char *argv[])
{
        test_init(&argc, argv, "[TEMPLATE [KEY=VALUE ...]]",
                  "With no arguments: applies 19 templates (some malformed on purpose) to a\n"
                  "fixed set of values, then extracts the variables of each template.\n"
                  "With TEMPLATE: replaces {{KEY}} with VALUE, keeps {{IF KEY}}...{{ELSE}}...\n"
                  "{{ENDIF}} branches by whether KEY is given, and prints the result (-vv also\n"
                  "lists the variables the template uses). A malformed template fails.\n"
                  "Example: unit-template '{{IF name}}Hi {{name}}!{{ELSE}}Hi stranger{{ENDIF}}' name=Ada");
        if(argc > 1){
                RUN(demo(argc, argv));
                return EXIT_SUCCESS;
        }
        SECTION_MSG("tld_template_apply");
        RUN(test_new_template());
        OK_MSG("templates applied");
        SECTION_MSG("tld_template_vars");
        RUN(test_get_vars_template());
        OK_MSG("variables extracted");
        return EXIT_SUCCESS;
ERROR:
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
        RUN(tld_strbuf_alloc(&template, 64));

        for (int i = 0; i < num_test_templates; i++) {
                tld_strbuf_clear(template);
                RUN(tld_strbuf_append(template, test_templates[i]));
                KV_MSG("template", "%s", TLD_STR(template));
                if(tld_template_vars(template, &h) == OK){
                        RUN(tld_hash_iter_init(h));
                        const char* key = NULL;
                        const char* value = NULL;
                        while(tld_hash_iter_next(h, &key, &value) == OK){
                                DEBUG_MSG("key %s value %s", key, value);
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
        RUN(tld_strbuf_alloc(&template, 64));
        RUN(tld_hash_add(&h, "name", "Alica"));
        RUN(tld_hash_add(&h, "age", "101"));
        RUN(tld_hash_add(&h, "is_student", "yes"));
        RUN(tld_hash_add(&h, "major", "Computer Science"));

        for (int i = 0; i < num_test_templates; i++) {
                tld_strbuf_clear(template);
                RUN(tld_strbuf_append(template, test_templates[i]));
                KV_MSG("template", "%s", TLD_STR(template));
                if(tld_template_apply(template, h) != OK){
                        DEBUG_MSG("template %d rejected (some test templates are malformed on purpose)", i);
                }
                KV_MSG("result", "%s", TLD_STR(template));
        }
        tld_strbuf_free(template);
        tld_hash_free(h);

        return OK;
ERROR:
        return FAIL;
}

