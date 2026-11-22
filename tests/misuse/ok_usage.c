/* The correct forms of everything the err_ and warn_ files get wrong: must compile
 * without a warning, so the failures above are not caused by the setup. */
#include "tld.h"
#include "tld_ai.h"
void f(const char *text, int n, tld_hash *h);
void f(const char *text, int n, tld_hash *h)
{
        double *d = NULL;
        const char *v = NULL;
        LOG_MSG("%s", text);
        LOG_MSG("%d items", n);
        KV_MSG(text, "%d", n);
        (void)ai_text("%s", text);
        (void)ai_fmt("%s/%d", text, n);
        tld_hash_get(h, "k", &v);
        galloc(&d, 10);
        gfree(d);
        gfree(d);                       /* harmless: the first gfree cleared d */
ERROR:
        return;
}
