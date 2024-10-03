#include "tld.h"


int test_new_template(void);
int test_hash_operations(void);

int main(void)
{
        test_new_template();

        test_hash_operations();
        /* LOG_MSG("Success"); */
        return EXIT_SUCCESS;
ERROR:
        /* LOG_MSG("FAIL"); */
        return EXIT_FAILURE;
}

int test_hash_operations(void) {
        tld_template_hash *hash = NULL;

        // Step 1: Initialize the hash
        printf("Initializing hash...\n");
        if (tld_template_hash_alloc(&hash, 1024) != 0) {
                fprintf(stderr, "Error: Hash initialization failed!\n");
                return -1;
        }

        // Step 2: Add key-value pairs
        printf("Adding key-value pairs...\n");
        tld_template_add(&hash, "key1", "value1");
        tld_template_add(&hash, "key2", "value2");
        tld_template_add(&hash, "key3", "value3");

        // Step 3: Retrieve values and verify correctness
        printf("Retrieving values...\n");
        char *value;
        if (tld_template_get(hash, "key1", &value) == 0 && strcmp(value, "value1") == 0) {
                printf("key1: %s\n", value); // Expected: value1
        } else {
                printf("Error: key1 retrieval failed\n");
        }

        if (tld_template_get(hash, "key2", &value) == 0 && strcmp(value, "value2") == 0) {
                printf("key2: %s\n", value); // Expected: value2
        } else {
                printf("Error: key2 retrieval failed\n");
        }

        // Step 4: Test overwriting a key
        printf("Overwriting key1...\n");
        tld_template_add(&hash, "key1", "new_value1");
        tld_template_get(hash, "key1", &value);
        printf("key1 (overwritten): %s\n", value); // Expected: new_value1

        // Step 5: Test retrieving non-existent key
        printf("Retrieving non-existent key...\n");
        if (tld_template_get(hash, "nonexistent", &value) == -1) {
                printf("nonexistent key not found\n");
        } else {
                printf("Error: nonexistent key found: %s\n", value);
        }

        // Step 6: Test deleting a key
        printf("Deleting key2...\n");
        tld_template_remove(hash, "key2");
        if (tld_template_get(hash, "key2", &value) == -1) {
                printf("key2 deleted successfully\n");
        } else {
                printf("Error: key2 still exists\n");
        }

        // Step 7: Test hash collisions by adding many keys with similar prefixes
        printf("Testing hash collisions...\n");
        char key[20], val[20];
        for (int i = 0; i < 1000; i++) {
                snprintf(key, 20,"collide_key%d", i);
                snprintf(val,20, "value%d", i);
                tld_template_add(&hash, key, val);
        }

        // Verify a few random keys
        printf("Verifying collision keys...\n");
        for (int i = 0; i < 1000; i += 100) {
                snprintf(key, 20,"collide_key%d", i);
                tld_template_get(hash, key, &value);
                printf("%s: %s\n", key, value); // Expected: valuei
        }

        // Step 8: Test hash size limit and expansion
        printf("Testing hash size limit and expansion...\n");
        for (int i = 1000; i < 1100; i++) {
                snprintf(key,20, "extra_key%d", i);
                snprintf(val,20, "value%d", i);
                tld_template_add(&hash, key, val);
        }

        // Verify new keys
        for (int i = 1000; i < 1100; i++) {
                snprintf(key, 20,"extra_key%d", i);
                tld_template_get(hash, key, &value);
                printf("%s: %s\n", key, value); // Expected: valuei
        }

        // Step 9: Stress test with large amounts of data
        printf("Stress test with large amount of data...\n");
        for (int i = 1100; i < 2100; i++) {
                snprintf(key, 20,"large_key%d", i);
                snprintf(val, 20,"value%d", i);
                tld_template_add(&hash, key, val);
        }

        // Verify a few random keys
        for (int i = 1100; i < 2100; i += 100) {
                snprintf(key, 20, "large_key%d", i);
                tld_template_get(hash, key, &value);
                printf("%s: %s\n", key, value); // Expected: valuei
        }

        // Step 10: Clean up
        printf("Freeing hash...\n");
        tld_template_hash_free(hash);

        return 0;
}


/* int test_hash_operations(void) */
/* { */
/*         tld_template_hash *hash = NULL; */

/*         // Step 1: Initialize the hash */
/*         printf("Initializing hash...\n"); */
/*         if (tld_template_hash_alloc(&hash,1024) != 0) { */
/*                 ERROR_MSG("Hash initialization failed!"); */
/*         } */

/*         // Step 2: Add key-value pairs */
/*         printf("Adding key-value pairs...\n"); */
/*         tld_template_add(&hash, "key1", "value1"); */
/*         tld_template_add(&hash, "key2", "value2"); */
/*         tld_template_add(&hash, "key3", "value3"); */

/*         // Step 3: Retrieve values */
/*         printf("Retrieving values...\n"); */
/*         char *value; */
/*         tld_template_get(hash, "key1", &value); */
/*         printf("key1: %s\n", value); // Expected: value1 */
/*         tld_template_get(hash, "key2", &value); */
/*         printf("key2: %s\n", value); // Expected: value2 */

/*         // Step 4: Test overwriting a key */
/*         printf("Overwriting key1...\n"); */
/*         tld_template_add(&hash, "key1", "new_value1"); */
/*         tld_template_add(&hash, "key1", value); */
/*         printf("key1 (overwritten): %s\n", value); // Expected: new_value1 */

/*         // Step 5: Test retrieving non-existent key */
/*         printf("Retrieving non-existent key...\n"); */
/*         if (tld_template_get(hash, "nonexistent", &value) == -1) { */
/*                 printf("nonexistent key not found\n"); */
/*         } else { */
/*                 printf("nonexistent key found: %s\n", value); // Should not happen */
/*         } */

/*         // Step 6: Delete a key */
/*         printf("Deleting key2...\n"); */
/*         tld_template_remove(hash, "key2"); */
/*         if (tld_template_get(hash, "key2", &value) == -1) { */
/*                 printf("key2 deleted successfully\n"); */
/*         } else { */
/*                 printf("Error: key2 still exists\n"); */
/*         } */

/*         // Step 7: Clean up */
/*         printf("Freeing hash...\n"); */
/*         tld_template_hash_free(hash); */
/*         return OK; */
/* ERROR: */
/*         return FAIL; */
/* } */

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

        tld_template_hash* h = NULL;
        tld_strbuf* template = NULL;
        tld_strbuf_alloc(&template, 64);
        tld_template_add(&h, "name", "Alica");
        tld_template_add(&h, "age", "101");
        tld_template_add(&h, "is_student", "yes");
        tld_template_add(&h, "major", "Computer Science");

        /* tld_template_init(&map, identifiers, replacements, 4); */
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
        tld_template_hash_free(h);

        return OK;
ERROR:
        return FAIL;
}

/* int test_default(tld_template_map *map) */
/* { */

/*         char txt[] = "AAAA {{AA}} BBBB {{BB}} CCCC"; */
/*         tld_strbuf* template = NULL; */

/*         tld_strbuf_alloc(&template, 64); */
/*         tld_append(template, txt); */
/*         LOG_MSG("%s", TLD_STR(template)); */
/*         tld_template_apply(template, map); */
/*         LOG_MSG("%s", TLD_STR(template)); */
/*         if(tld_template_chk(map, 1) != OK){ */
/*                 ERROR_MSG("default replacement test failed!"); */
/*         } */
/*         tld_strbuf_free(template); */
/*         return OK; */
/* ERROR: */
/*         if(template){ */
/*                 tld_strbuf_free(template); */
/*         } */
/*         return FAIL; */
/* } */

/* int test_mismatch (tld_template_map *map) */
/* { */

/*         char txt[] = "AAAA {{AA} BBBB {{BB}} CCCC"; */
/*         tld_strbuf* template = NULL; */

/*         tld_strbuf_alloc(&template, 64); */
/*         tld_append(template, txt); */
/*         LOG_MSG("%s", TLD_STR(template)); */
/*         tld_template_apply(template, map); */
/*         LOG_MSG("%s", TLD_STR(template)); */
/*         if(tld_template_chk(map, 1) != OK){ */
/*                 ERROR_MSG("mismatch test failed!"); */
/*         } */
/*         tld_strbuf_free(template); */
/*         return OK; */
/* ERROR: */
/*         if(template){ */
/*                 tld_strbuf_free(template); */
/*         } */
/*         return FAIL; */
/* } */

/* int test_mismatch2 (tld_template_map *map) */
/* { */

/*         char txt[] = "AAAA {{AA}} BBBB {BB}} CCCC"; */
/*         tld_strbuf* template = NULL; */

/*         tld_strbuf_alloc(&template, 64); */
/*         tld_append(template, txt); */
/*         LOG_MSG("%s", TLD_STR(template)); */
/*         tld_template_apply(template, map); */
/*         LOG_MSG("%s", TLD_STR(template)); */
/*         if(tld_template_chk(map, 1) != OK){ */
/*                 ERROR_MSG("mismatch test failed!"); */
/*         } */
/*         tld_strbuf_free(template); */
/*         return OK; */
/* ERROR: */
/*         if(template){ */
/*                 tld_strbuf_free(template); */
/*         } */
/*         return FAIL; */
/* } */

/* int test_norep (tld_template_map *map) */
/* { */

/*         char txt[] = "AAAA  BBBB  CCCC"; */
/*         tld_strbuf* template = NULL; */

/*         tld_strbuf_alloc(&template, 64); */
/*         tld_append(template, txt); */
/*         LOG_MSG("%s - before", TLD_STR(template)); */
/*         tld_template_apply(template, map); */
/*         LOG_MSG("%s - after", TLD_STR(template)); */
/*         if(tld_template_chk(map, 1) != OK){ */
/*                 ERROR_MSG("mismatch test failed!"); */
/*         } */
/*         tld_strbuf_free(template); */
/*         return OK; */
/* ERROR: */
/*         if(template){ */
/*                 tld_strbuf_free(template); */
/*         } */
/*         return FAIL; */
/* } */


/* int test_ifrep (tld_template_map *map) */
/* { */

/*         char txt[] = "AAAA {{AA}} BBBB {{IF BB}} CCCC {{BB}} {{ENDIF}}"; */

/*         tld_strbuf* template = NULL; */

/*         tld_strbuf_alloc(&template, 64); */
/*         tld_append(template, txt); */
/*         LOG_MSG("%s - before", TLD_STR(template)); */
/*         tld_template_apply(template, map); */
/*         LOG_MSG("%s - after", TLD_STR(template)); */
/*         if(tld_template_chk(map, 1) != OK){ */
/*                 ERROR_MSG("mismatch test failed!"); */
/*         } */
/*         tld_strbuf_free(template); */
/*         return OK; */
/* ERROR: */
/*         if(template){ */
/*                 tld_strbuf_free(template); */
/*         } */
/*         return FAIL; */
/* } */
