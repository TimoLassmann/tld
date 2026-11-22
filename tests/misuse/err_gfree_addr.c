/* gfree of an address (not the pointer itself): a hard compile error. */
#include "tld.h"
int f(void);
int f(void)
{
        double *d = NULL;
        galloc(&d, 10);
        gfree(&d);
        return OK;
ERROR:
        return FAIL;
}
