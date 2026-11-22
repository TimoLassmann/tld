#include "tld.h"
#include "test.h"
#include <stdio.h>

/*
  Core functions to test tl_bitfiles implem

 */

int tl_bitfield_test1(void)
{
        tl_bitfield *b = NULL;
        int status = 0;
        tl_bitfield_alloc(&b, 100);
        for(int i = 0; i < 100; i++){
                tl_bitfield_set(b, i);
        }

        status = tl_bitfield_set(b, 100);
        if(status != FAIL){
                ERROR_MSG("tl_bitfield_set failed");
        }
        tl_bitfield_free(b);
        return OK;
ERROR:
        return FAIL;
}

/*
  A more difficult test would be to test the bitfield with a large number of bits
*/

int tl_bitfield_test2(void)
{
        tl_bitfield *b = NULL;
        int status = 0;
        tl_bitfield_alloc(&b, 1000000);
        for(int i = 0; i < 1000000; i++){
                tl_bitfield_set(b, i);
        }

        status = tl_bitfield_set(b, 1000000);
        if(status != FAIL){
                ERROR_MSG("tl_bitfield_set failed");
        }
        tl_bitfield_free(b);
        return OK;
ERROR:
        return FAIL;
}

/*
To make sure bits are set and can be retrieved
 */
int tl_bitfield_test3(void) {
        tl_bitfield *b = NULL;
        int status = 0;
        uint8_t bit = 0;
        tl_bitfield_alloc(&b, 100);
        for(int i = 0; i < 100; i++){
                tl_bitfield_set(b, i);
        }

        for(int i = 0; i < 100; i++){
                RUN(tl_bitfield_get(b, i,&bit));
                if(bit != 1){
                        ERROR_MSG("tl_bitfield_get failed");
                }
        }

        status = tl_bitfield_get(b, 100,&bit);
        if(status != FAIL){
                ERROR_MSG("tl_bitfield_get failed");
        }
        tl_bitfield_free(b);
        return OK;
ERROR:
        return FAIL;

}

/*
  Here I test the hamming distance function for bitfields
 */

int tl_bitfield_test4(void)
{
        tl_bitfield *b1 = NULL;
        tl_bitfield *b2 = NULL;
        int32_t dist = 0;

        tl_bitfield_alloc(&b1, 100);
        tl_bitfield_alloc(&b2, 100);
        for(int i = 0; i < 100; i++){
                tl_bitfield_set(b1, i);
        }

        for(int i = 0; i < 100; i++){
                tl_bitfield_set(b2, i);
        }

        RUN(tl_bitfield_hamming(b1, b2, NULL,&dist));
        if(dist != 0){
                ERROR_MSG("tl_bitfield_hamming failed (d = %d) A", dist);
        }



        tl_bitfield_clear(b2, 0);
        RUN(tl_bitfield_hamming(b1, b2, NULL,&dist));
        if(dist != 1){
                ERROR_MSG("tl_bitfield_hamming failed (d = %d) B", dist);
        }
        tl_bitfield_free(b1);
        tl_bitfield_free(b2);
        return OK;
ERROR:
        return FAIL;
}

/* Test to set and get bits */
int tl_bitfield_test5(void)
{
        tl_bitfield *b = NULL;
        int status = 0;
        uint8_t bit = 0;
        tl_bitfield_alloc(&b, 100);
        for(int i = 0; i < 100; i++){
                tl_bitfield_set(b, i);
        }

        for(int i = 0; i < 100; i++){
                RUN(tl_bitfield_get(b, i,&bit));
                if(bit != 1){
                        ERROR_MSG("tl_bitfield_get failed");
                }
        }

        status = tl_bitfield_get(b, 100, &bit);
        if(status != FAIL){
                ERROR_MSG("tl_bitfield_get failed");
        }
        tl_bitfield_free(b);
        return OK;
ERROR:
        return FAIL;
}

/* unit-bitfield SIZE BIT ...: set bits in a bitfield and print it. */
static int demo(int n, char **args)
{
        tl_bitfield *b = NULL;
        int size = atoi(args[0]);
        int n_set = 0;
        uint8_t bit = 0;
        if(size < 1){
                ERROR_MSG("SIZE must be positive");
        }
        SECTION_MSG("bitfield of %d bits", size);
        RUN(tl_bitfield_alloc(&b, size));
        for(int i = 1; i < n; i++){
                int idx = atoi(args[i]);
                if(tl_bitfield_set(b, idx) != OK){
                        ERROR_MSG("bit %d is outside 0..%d", idx, size - 1);
                }
                DEBUG_MSG("set bit %d", idx);
        }
        for(int i = 0; i < size; i++){
                RUN(tl_bitfield_get(b, i, &bit));
                n_set += bit;
                fputc(bit ? '1' : '0', stdout);
        }
        fputc('\n', stdout);
        fprintf(stdout, "%d of %d bits set\n", n_set, size);
        OK_MSG("done");
        tl_bitfield_free(b);
        return OK;
ERROR:
        if(b){
                tl_bitfield_free(b);
        }
        return FAIL;
}

int main(int argc, char *argv[])
{
        test_init(&argc, argv, "[SIZE [BIT ...]]",
                  "With no arguments: sets, gets, clears bits and computes Hamming distances,\n"
                  "including out-of-range indices that must fail (those print an ERROR line).\n"
                  "With SIZE [BIT ...]: allocates a SIZE-bit field, sets the given bits and\n"
                  "prints the field as 0/1 and the number of bits set.\n"
                  "Example: unit-bitfield 16 0 3 15");
        if(argc > 1){
                RUN(demo(argc - 1, argv + 1));
                return EXIT_SUCCESS;
        }
        LOG_MSG("Running tests for bitfield");
        SECTION_MSG("set 100 bits, set out of range");
        RUN(tl_bitfield_test1());
        OK_MSG("passed");
        SECTION_MSG("set 1000000 bits, set out of range");
        RUN(tl_bitfield_test2());
        OK_MSG("passed");

        SECTION_MSG("set and get 100 bits, get out of range");
        RUN(tl_bitfield_test3());
        OK_MSG("passed");
        SECTION_MSG("Hamming distance");
        RUN(tl_bitfield_test4());
        OK_MSG("passed");
        SECTION_MSG("set and get again");
        RUN(tl_bitfield_test5());
        OK_MSG("passed");
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}
