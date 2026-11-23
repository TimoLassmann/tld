// test_hash.c
#include "tld.h"
#include "hash/tld-hash_struct.h"
#include "test.h"
#include <stdio.h>

// Macro for asserting test conditions
#define ASSERT_TEST(cond, msg) do {                                 \
                if (!(cond)) {                                      \
                        fprintf(stderr, "TEST FAILED: %s\n", msg);  \
                        return -1;                                  \
                }                                                   \
        } while(0)

// Function prototypes
int test_initialization(void);
int test_add_and_get(void);
int test_overwrite(void);
int test_deletion(void);
int test_collision_handling(void);
int test_dynamic_resizing(void);
int test_stress(void);

/* unit-hash KEY=VALUE ... [KEY ...]: add pairs, look up keys, list the map. */
static int demo(int n, char **args)
{
        tld_hash *hash = NULL;
        const char *key = NULL;
        const char *value = NULL;
        RUN(tld_hash_alloc(&hash, 4));
        SECTION_MSG("add");
        for (int i = 0; i < n; i++) {
                char *eq = strchr(args[i], '=');
                if (!eq) continue;
                *eq = 0;
                RUN(tld_hash_add(&hash, args[i], eq + 1));
                KV_MSG(args[i], "%s", eq + 1);
                *eq = '=';
        }
        DEBUG_MSG("%d entries, table size %d", hash->n, hash->table_size);
        SECTION_MSG("look up");
        for (int i = 0; i < n; i++) {
                if (strchr(args[i], '=')) continue;
                if (tld_hash_get(hash, args[i], &value) == OK) {
                        fprintf(stdout, "get %s = %s\n", args[i], value);
                } else {
                        fprintf(stdout, "get %s: not found\n", args[i]);
                }
        }
        SECTION_MSG("all entries");
        RUN(tld_hash_iter_init(hash));
        while (tld_hash_iter_next(hash, &key, &value) == OK) {
                fprintf(stdout, "%s=%s\n", key, value);
        }
        OK_MSG("%d entries", hash->n);
        tld_hash_free(hash);
        return OK;
ERROR:
        if (hash) tld_hash_free(hash);
        return FAIL;
}

// Main function to run all tests
int main(int argc, char *argv[]) {
        test_init(&argc, argv, "[KEY=VALUE ...] [KEY ...]",
                  "With no arguments: initialisation, add/get, overwrite, delete, collisions,\n"
                  "resizing and a 10000-entry stress test of tld_hash.\n"
                  "With arguments: adds each KEY=VALUE (a repeated key overwrites), looks up\n"
                  "each bare KEY, then lists all entries with the iterator.\n"
                  "Example: unit-hash a=1 b=2 a=3 a c");
        if (argc > 1) {
                return demo(argc - 1, argv + 1) == OK ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        static const struct { const char *name; int (*fn)(void); } tests[] = {
                {"Initialization", test_initialization},
                {"Add and Get", test_add_and_get},
                {"Overwrite", test_overwrite},
                {"Deletion", test_deletion},
                {"Collision Handling", test_collision_handling},
                {"Dynamic Resizing", test_dynamic_resizing},
                /* {"Iteration", test_iteration}, */
                {"Stress", test_stress},
        };
        int n_tests = (int)(sizeof tests / sizeof tests[0]);
        int failed = 0;

        for (int i = 0; i < n_tests; i++) {
                SECTION_MSG("%s", tests[i].name);
                if (tests[i].fn() == 0) {
                        OK_MSG("%s: passed", tests[i].name);
                } else {
                        fprintf(stderr, "Test %s: FAILED\n", tests[i].name);
                        failed++;
                }
        }

        KV_MSG("passed", "%d", n_tests - failed);
        KV_MSG("failed", "%d", failed);
        return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

// Test 1: Initialization
int test_initialization(void)
{
        tld_hash *hash = NULL;
        int initial_size = 52; // Starting with a prime number

        if (tld_hash_alloc(&hash, initial_size) != OK) {
                fprintf(stderr, "Error: Hash initialization failed!\n");
                return -1;
        }
        ASSERT_TEST(hash != NULL, "Hash should not be NULL after initialization.");
        ASSERT_TEST(hash->table_size == 53, "Hash table size mismatch.");
        ASSERT_TEST(hash->n == 0, "Hash should be empty after initialization.");

        tld_hash_free(hash);
        return 0;
}

// Test 2: Add and Get Operations
int test_add_and_get(void)
{
        tld_hash *hash = NULL;
        tld_hash_alloc(&hash, 53);

        // Adding key-value pairs
        ASSERT_TEST(tld_hash_add(&hash, "key1", "value1") == OK, "Failed to add key1.");
        ASSERT_TEST(tld_hash_add(&hash, "key2", "value2") == OK, "Failed to add key2.");
        ASSERT_TEST(tld_hash_add(&hash, "key3", "value3") == OK, "Failed to add key3.");

        // Retrieving and verifying values
        const char *value = NULL;
        ASSERT_TEST(tld_hash_get(hash, "key1", &value) == OK && strcmp(value, "value1") == 0, "Incorrect value for key1.");
        ASSERT_TEST(tld_hash_get(hash, "key2", &value) == OK && strcmp(value, "value2") == 0, "Incorrect value for key2.");
        ASSERT_TEST(tld_hash_get(hash, "key3", &value) == OK && strcmp(value, "value3") == 0, "Incorrect value for key3.");

        tld_hash_free(hash);
        return 0;
}

// Test 3: Overwrite Existing Key
int test_overwrite(void) {
        tld_hash *hash = NULL;
        tld_hash_alloc(&hash, 53);

        // Add a key
        ASSERT_TEST(tld_hash_add(&hash, "key1", "value1") == OK, "Failed to add key1.");

        // Overwrite the key
        ASSERT_TEST(tld_hash_add(&hash, "key1", "new_value1") == OK, "Failed to overwrite key1.");

        // Verify the new value
        const char *value = NULL;
        ASSERT_TEST(tld_hash_get(hash, "key1", &value) == OK && strcmp(value, "new_value1") == 0, "Overwrite failed for key1.");

        tld_hash_free(hash);
        return 0;
}

// Test 4: Deletion of Keys
int test_deletion(void) {
        tld_hash *hash = NULL;
        tld_hash_alloc(&hash, 53);

        // Add keys
        tld_hash_add(&hash, "key1", "value1");
        tld_hash_add(&hash, "key2", "value2");

        // Delete key2
        ASSERT_TEST(tld_hash_del(hash, "key2") == OK, "Failed to delete key2.");

        // Verify deletion
        const char *value = NULL;
        ASSERT_TEST(tld_hash_get(hash, "key2", &value) == FAIL, "key2 should have been deleted.");

        // Ensure key1 still exists
        ASSERT_TEST(tld_hash_get(hash, "key1", &value) == OK && strcmp(value, "value1") == 0, "key1 should still exist.");

        tld_hash_free(hash);
        return 0;
}

// Test 5: Collision Handling
int test_collision_handling(void) {
        tld_hash *hash = NULL;
        // Use a small table size to force collisions
        tld_hash_alloc(&hash, 5);

        // Add multiple keys that likely collide
        for (int i = 0; i < 10; i++) {
                char key[32];
                char val[32];
                snprintf(key, sizeof(key), "key%d", i);
                snprintf(val, sizeof(val), "value%d", i);
                ASSERT_TEST(tld_hash_add(&hash, key, val) == OK, "Failed to add colliding key.");
        }

        // Verify all keys
        const char *value = NULL;
        for (int i = 0; i < 10; i++) {
                char key[32];
                snprintf(key, sizeof(key), "key%d", i);
                ASSERT_TEST(tld_hash_get(hash, key, &value) == OK, "Failed to retrieve colliding key.");
                char expected_val[32];
                snprintf(expected_val, sizeof(expected_val), "value%d", i);
                ASSERT_TEST(strcmp(value, expected_val) == 0, "Incorrect value for colliding key.");
        }

        tld_hash_free(hash);
        return 0;
}

// Test 6: Dynamic Resizing
int test_dynamic_resizing(void)
{
        tld_hash *hash = NULL;
        // Start with a small table to trigger resizing
        tld_hash_alloc(&hash, 52);

        // Define load factor threshold (e.g., 0.75)
        // Assuming the hash implementation resizes when load factor exceeds 0.75

        int initial_size = hash->table_size;

        // Add entries to exceed the load factor
        int entries_to_add = (int)(initial_size * 0.75) + 10; // This should trigger a resize
        for (int i = 0; i < entries_to_add; i++) {
                char key[32];
                char val[32];
                snprintf(key, sizeof(key), "resize_key%d", i);
                snprintf(val, sizeof(val), "resize_val%d", i);
                ASSERT_TEST(tld_hash_add(&hash, key, val) == OK, "Failed to add key for resizing.");
        }
        // Check if the table has resized
        ASSERT_TEST(hash->table_size > initial_size, "Hash table did not resize as expected.");

        // Verify all entries
        const char *value = NULL;
        for (int i = 0; i < entries_to_add; i++) {
                char key[32];
                snprintf(key, sizeof(key), "resize_key%d", i);
                ASSERT_TEST(tld_hash_get(hash, key, &value) == OK, "Failed to retrieve key after resizing.");
                char expected_val[32];
                snprintf(expected_val, sizeof(expected_val), "resize_val%d", i);
                ASSERT_TEST(strcmp(value, expected_val) == 0, "Incorrect value after resizing.");
        }

        tld_hash_free(hash);
        return 0;
}

// Test 8: Stress Test with Large Data
int test_stress(void) {
        tld_hash *hash = NULL;
        tld_hash_alloc(&hash, 53);

        int total_entries = 10000;
        char key[50], val[50];

        // Add a large number of entries
        for (int i = 0; i < total_entries; i++) {
                snprintf(key, sizeof(key), "stress_key_%d", i);
                snprintf(val, sizeof(val), "stress_val_%d", i);
                ASSERT_TEST(tld_hash_add(&hash, key, val) == OK, "Failed to add key during stress test.");
        }

        // Verify a subset of entries
        for (int i = 0; i < total_entries; i += 1000) { // Check every 1000th entry
                snprintf(key, sizeof(key), "stress_key_%d", i);
                snprintf(val, sizeof(val), "stress_val_%d", i);
                const char *retrieved_val = NULL;
                ASSERT_TEST(tld_hash_get(hash, key, &retrieved_val) == OK, "Failed to retrieve key during stress test.");
                ASSERT_TEST(strcmp(retrieved_val, val) == 0, "Incorrect value during stress test.");
        }

        // Optionally, print the total number of entries
        KV_MSG("entries", "%d", hash->n);

        tld_hash_free(hash);
        return 0;
}
