#include "tld.h"

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
        return OK;
ERROR:
        return FAIL;
}

int main(void)
{
        LOG_MSG("Running tests for bitfield");
        RUN(tl_bitfield_test1());
        RUN(tl_bitfield_test2());

        RUN(tl_bitfield_test3());
        RUN(tl_bitfield_test4());
        RUN(tl_bitfield_test5());
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}
