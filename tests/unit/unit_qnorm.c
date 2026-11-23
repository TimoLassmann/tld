#include "tld.h"
#include "test.h"
#include <stdio.h>

int qnorm_test(void);
int qnorm_big_test(void);

/* unit-qnorm ROWS: quantile-normalise a matrix given as rows "a,b,c" (same length). */
static int demo(int n, char **rows)
{
        double **data = NULL;
        int m = 1;
        for(const char *p = rows[0]; *p; p++){
                if(*p == ','){
                        m++;
                }
        }
        galloc(&data, n, m);
        for(int i = 0; i < n; i++){
                const char *s = rows[i];
                for(int j = 0; j < m; j++){
                        char *end = NULL;
                        data[i][j] = strtod(s, &end);
                        ASSERT(end != s, "row %d: expected %d comma-separated numbers: %s", i + 1, m, rows[i]);
                        s = (*end == ',') ? end + 1 : end;
                }
                ASSERT(*s == 0, "row %d has more than %d values: %s", i + 1, m, rows[i]);
        }
        SECTION_MSG("tld_quantile_normalise on %d rows x %d columns", n, m);
        RUN(tld_quantile_normalise(data, n, m));
        for(int i = 0; i < n; i++){
                for(int j = 0; j < m; j++){
                        fprintf(stdout, "%s%g", j ? "\t" : "", data[i][j]);
                }
                fprintf(stdout, "\n");
        }
        OK_MSG("done");
        gfree(data);
        return OK;
ERROR:
        gfree(data);
        return FAIL;
}

int main(int argc, char *argv[])
{
        test_init(&argc, argv, "[ROW ...]",
                  "With no arguments: tld_quantile_normalise on a 4x3 matrix and on 50000x10 gaussian\n"
                  "draws.\n"
                  "With ROWs (each a comma-separated list of numbers, one value per column):\n"
                  "quantile-normalises the columns and prints the matrix, tab-separated.\n"
                  "Example: unit-qnorm 5,4,3 2,1,4 3,4,6 4,2,8");
        if(argc > 1){
                RUN(demo(argc - 1, argv + 1));
                return EXIT_SUCCESS;
        }
        SECTION_MSG("qnorm test");
        RUN(qnorm_test());
        OK_MSG("4x3 matrix normalised");
        SECTION_MSG("qnorm big test");
        RUN(qnorm_big_test());
        OK_MSG("50000x10 matrix normalised");
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

int qnorm_test(void)
{
        double** data = NULL;

        int n = 4;
        int m = 3;

        galloc(&data,n,m);

        data[0][0] = 5.0;
        data[0][1] = 4.0;
        data[0][2] = 3.0;
        data[1][0] = 2.0;
        data[1][1] = 1.0;
        data[1][2] = 4.0;
        data[2][0] = 3.0;
        data[2][1] = 4.0;
        data[2][2] = 6.0;
        data[3][0] = 4.0;
        data[3][1] = 2.0;
        data[3][2] = 8.0;

        for(int i = 0; i < n;i++){
                for(int j = 0; j < m;j++){
                        data[i][j] = log(data[i][j]+1.0);
                }

        }

        RUN(tld_quantile_normalise(data,  n, m));
        for(int i = 0; i < n;i++){
                DEBUG_MSG("row %d: %f %f %f", i, data[i][0], data[i][1], data[i][2]);
        }

        gfree(data);
        return OK;
ERROR:
        return FAIL;

}

int qnorm_big_test(void)
{
        struct tld_rng* rng = NULL;

        double** data = NULL;

        int n = 50000;
        int m = 10;

        RUN(tld_rng_alloc(&rng, 0));

        galloc(&data,n,m);
        for(int i = 0; i < n;i++){
                for(int j = 0; j < m;j++){
                        data[i][j] = tld_rng_gaussian(rng, (double) (j + 1), (double)(j + 1));
                }
                tld_progress(i + 1, n, "drawing");
        }

        RUN(tld_quantile_normalise(data,  n, m));
        DEBUG_MSG("first row after: %f %f", data[0][0], data[0][1]);

        tld_rng_free(rng);
        gfree(data);
        return OK;
ERROR:
        if(rng){
                tld_rng_free(rng);
        }
        if(data){
                gfree(data);
        }
        return FAIL;

}
