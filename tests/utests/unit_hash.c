// test_hash.c
#include "tld.h"
#include "hash/hash_struct.h"

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
/* int test_iteration(); */
int test_stress(void);

// Main function to run all tests
int main(void) {
        int failed = 0;

        printf("Running Hash Table Unit Tests...\n\n");

        if (test_initialization() == 0) {
                printf("Test Initialization: PASSED\n");
        } else {
                printf("Test Initialization: FAILED\n");
                failed++;
        }

        if (test_add_and_get() == 0) {
                printf("Test Add and Get: PASSED\n");
        } else {
                printf("Test Add and Get: FAILED\n");
                failed++;
        }

        if (test_overwrite() == 0) {
                printf("Test Overwrite: PASSED\n");
        } else {
                printf("Test Overwrite: FAILED\n");
                failed++;
        }

        if (test_deletion() == 0) {
                printf("Test Deletion: PASSED\n");
        } else {
                printf("Test Deletion: FAILED\n");
                failed++;
        }

        if (test_collision_handling() == 0) {
                printf("Test Collision Handling: PASSED\n");
        } else {
                printf("Test Collision Handling: FAILED\n");
                failed++;
        }

        if (test_dynamic_resizing() == 0) {
                printf("Test Dynamic Resizing: PASSED\n");
        } else {
                printf("Test Dynamic Resizing: FAILED\n");
                failed++;
        }

        /* if (test_iteration() == 0) { */
        /*         printf("Test Iteration: PASSED\n"); */
        /* } else { */
        /*         printf("Test Iteration: FAILED\n"); */
        /*         failed++; */
        /* } */

        if (test_stress() == 0) {
                printf("Test Stress: PASSED\n");
        } else {
                printf("Test Stress: FAILED\n");
                failed++;
        }

        printf("\nUnit Tests Completed: %d Passed, %d Failed\n", 8 - failed, failed);
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
        /* LOG_MSG("Hash table size: %d", hash->table_size); */
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
        char *value = NULL;
        ASSERT_TEST(tld_hash_get(hash, "key1", &value) == OK && strcmp(value, "value1") == 0, "Incorrect value for key1.");
        ASSERT_TEST(tld_hash_get(hash, "key2", &value) == OK && strcmp(value, "value2") == 0, "Incorrect value for key2.");
        ASSERT_TEST(tld_hash_get(hash, "key3", &value) == OK && strcmp(value, "value3") == 0, "Incorrect value for key3.");

        tld_hash_free(hash);
        return 0;
}

// Test 3: Overwrite Existing Key
int test_overwrite() {
        tld_hash *hash = NULL;
        tld_hash_alloc(&hash, 53);

        // Add a key
        ASSERT_TEST(tld_hash_add(&hash, "key1", "value1") == OK, "Failed to add key1.");

        // Overwrite the key
        ASSERT_TEST(tld_hash_add(&hash, "key1", "new_value1") == OK, "Failed to overwrite key1.");

        // Verify the new value
        char *value = NULL;
        ASSERT_TEST(tld_hash_get(hash, "key1", &value) == OK && strcmp(value, "new_value1") == 0, "Overwrite failed for key1.");

        tld_hash_free(hash);
        return 0;
}

// Test 4: Deletion of Keys
int test_deletion() {
        tld_hash *hash = NULL;
        tld_hash_alloc(&hash, 53);

        // Add keys
        tld_hash_add(&hash, "key1", "value1");
        tld_hash_add(&hash, "key2", "value2");

        // Delete key2
        ASSERT_TEST(tld_hash_del(hash, "key2") == OK, "Failed to delete key2.");

        // Verify deletion
        char *value = NULL;
        ASSERT_TEST(tld_hash_get(hash, "key2", &value) == FAIL, "key2 should have been deleted.");

        // Ensure key1 still exists
        ASSERT_TEST(tld_hash_get(hash, "key1", &value) == OK && strcmp(value, "value1") == 0, "key1 should still exist.");

        tld_hash_free(hash);
        return 0;
}

// Test 5: Collision Handling
int test_collision_handling() {
        tld_hash *hash = NULL;
        // Use a small table size to force collisions
        tld_hash_alloc(&hash, 5);

        // Add multiple keys that likely collide
        for (int i = 0; i < 10; i++) {
                char key[20];
                char val[20];
                snprintf(key, sizeof(key), "key%d", i);
                snprintf(val, sizeof(val), "value%d", i);
                ASSERT_TEST(tld_hash_add(&hash, key, val) == OK, "Failed to add colliding key.");
        }

        // Verify all keys
        char *value = NULL;
        for (int i = 0; i < 10; i++) {
                char key[20];
                snprintf(key, sizeof(key), "key%d", i);
                ASSERT_TEST(tld_hash_get(hash, key, &value) == OK, "Failed to retrieve colliding key.");
                char expected_val[20];
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
                char key[20];
                char val[20];
                snprintf(key, sizeof(key), "resize_key%d", i);
                snprintf(val, sizeof(val), "resize_val%d", i);
                ASSERT_TEST(tld_hash_add(&hash, key, val) == OK, "Failed to add key for resizing.");
        }
        /* LOG_MSG("Table size after resizing: %d before: %d", hash->table_size, initial_size); */
        // Check if the table has resized
        ASSERT_TEST(hash->table_size > initial_size, "Hash table did not resize as expected.");

        // Verify all entries
        char *value = NULL;
        for (int i = 0; i < entries_to_add; i++) {
                char key[20];
                snprintf(key, sizeof(key), "resize_key%d", i);
                ASSERT_TEST(tld_hash_get(hash, key, &value) == OK, "Failed to retrieve key after resizing.");
                char expected_val[20];
                snprintf(expected_val, sizeof(expected_val), "resize_val%d", i);
                ASSERT_TEST(strcmp(value, expected_val) == 0, "Incorrect value after resizing.");
        }

        tld_hash_free(hash);
        return 0;
}

// Test 7: Iteration Over Entries
/* int test_iteration() { */
/*         tld_hash *hash = NULL; */
/*         tld_hash_alloc(&hash, 53); */

/*         // Add multiple entries */
/*         tld_hash_add(&hash, "iter_key1", "iter_val1"); */
/*         tld_hash_add(&hash, "iter_key2", "iter_val2"); */
/*         tld_hash_add(&hash, "iter_key3", "iter_val3"); */

/*         // Initialize iterator */
/*         tld_hash_iterator iter; */
/*         ASSERT_TEST(tld_hash_iterator_init(hash, &iter) == OK, "Failed to initialize iterator."); */

/*         // Collect entries */
/*         int count = 0; */
/*         char *key, *value; */
/*         while (tld_hash_iterator_next(&iter, &key, &value) == OK) { */
/*                 if (strcmp(key, "iter_key1") == 0) { */
/*                         ASSERT_TEST(strcmp(value, "iter_val1") == 0, "Incorrect value for iter_key1."); */
/*                 } else if (strcmp(key, "iter_key2") == 0) { */
/*                         ASSERT_TEST(strcmp(value, "iter_val2") == 0, "Incorrect value for iter_key2."); */
/*                 } else if (strcmp(key, "iter_key3") == 0) { */
/*                         ASSERT_TEST(strcmp(value, "iter_val3") == 0, "Incorrect value for iter_key3."); */
/*                 } else { */
/*                         fprintf(stderr, "Unexpected key found during iteration: %s\n", key); */
/*                         return -1; */
/*                 } */
/*                 count++; */
/*         } */

/*         ASSERT_TEST(count == 3, "Iterator did not traverse all entries."); */

/*         tld_hash_free(hash); */
/*         return 0; */
/* } */

// Test 8: Stress Test with Large Data
int test_stress() {
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
                char *retrieved_val = NULL;
                ASSERT_TEST(tld_hash_get(hash, key, &retrieved_val) == OK, "Failed to retrieve key during stress test.");
                ASSERT_TEST(strcmp(retrieved_val, val) == 0, "Incorrect value during stress test.");
        }

        // Optionally, print the total number of entries
        printf("Total entries after stress test: %d\n", hash->n);

        tld_hash_free(hash);
        return 0;
}
