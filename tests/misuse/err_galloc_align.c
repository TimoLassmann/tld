/* galloc of an element type aligned beyond what malloc gives: a hard compile error. */
#include "tld.h"
struct wide {
        _Alignas(128) double x;
};
int f(void);
int f(void)
{
        struct wide *w = NULL;
        galloc(&w, 10);
        gfree(w);
        return OK;
ERROR:
        return FAIL;
}
