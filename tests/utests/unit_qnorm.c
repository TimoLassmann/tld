#include "tld.h"
#include <stdio.h>

int qnorm_test(void);

int main(void)
{
        LOG_MSG("qnorm test");
        RUN(qnorm_test());
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}


int qnorm_test(void)
{
        double** data = NULL;

        int n = 4;
        int m = 3;
        LOG_MSG("ONE");
        galloc(&data,n,m);
        data[0][0] = 5.0;
        data[0][1] = 4.0;
        data[0][2] = 3.0;
        LOG_MSG("1");
        data[1][0] = 2.0;
        data[1][1] = 1.0;
        data[1][2] = 4.0;
        LOG_MSG("2");
        data[2][0] = 3.0;
        data[2][1] = 4.0;
        data[2][2] = 6.0;
        LOG_MSG("3");
        data[3][0] = 4.0;
        data[3][1] = 2.0;
        data[3][2] = 8.0;

        RUN(norm_quantile(data,  n, m));

        gfree(data);
        return OK;
ERROR:
        return FAIL;

}
