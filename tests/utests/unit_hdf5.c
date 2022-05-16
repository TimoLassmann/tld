/* #include "hdf5/tld-hdf5wrap.h" */
/* #include "core/tld-core.h" */
/* #include "hdf5/tld-hdf5_struct.h" */
/* #include "hdf5/tld-hdf5wrap.h" */
#include "tld.h"
#include <stdint.h>
#include <stdio.h>

#define FILENAME "test.h5"

int main(void)
{
        struct hdf5_data* f = NULL;

        double* d1d = NULL;
        double** d2d = NULL;
        uint64_t t = 10;
        galloc(&d1d,10);
        for(int i = 0; i < 10;i++){
                d1d[i] = (double)i;
        }

        galloc(&d2d,3,3);
        for(int i = 0; i < 3;i++){
                for(int j = 0; j < 3;j++){
                        d2d[i][j] = MACRO_MAX(i, j);
                }
        }

        tldhdf5_open_file(&f, FILENAME);

        HDF_WRITE_DATA(f, "/dat1D","d1d",&d1d);
        HDF_WRITE_DATA(f, "/dat2D","d2d",&d2d);
        HDF_WRITE_DATA(f, "/dat0D","d1d",&t);

        double** read_d2d = NULL;
        HDF_READ_DATA(f, "/dat2D","d2d",&read_d2d);
        for(int i = 0; i < 3;i++){
                for(int j = 0; j < 3;j++){
                        fprintf(stdout,"%d %d %f\n",i,j,read_d2d[i][j]);
                }
        }

        /* int* g = NULL; */
        /* hdf5_read(f, "/dat2D","d2d",&g); */
        tlfhdf5_close_file(&f);

        uint64_t read_t;
        tldhdf5_open_file(&f, FILENAME);

        /* int a1[_Generic(0, int: 1, short: 2, float: 3, default: 4) == 1 ? 1 : -1]; // ext-warning {{'_Generic' is a C11 extension}} */
  /* (void) _Generic(read_t, // ext-warning {{'_Generic' is a C11 extension}} */
  /*     struct A: 0, // expected-error {{type 'struct A' in generic association incomplete}} */
  /*     void(): 0,   // expected-error {{type 'void ()' in generic association not an object type}} */
  /*     int: 0);  // expected-error {{type 'int [n]' in generic association is a variably modified type}} */


        HDF_READ_VALUE(f,"/dat0D","d1d",&read_t);
        tlfhdf5_close_file(&f);
        LOG_MSG("%d", read_t);

        gfree(d1d);

        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}
