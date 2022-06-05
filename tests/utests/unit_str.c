
#include "tld.h"
#include <stdio.h>


int main(void)
{
        char test[] = "Hello";
        tld_strbuf* b = NULL;
        tld_strbuf_alloc(&b, 10);
        tld_sprintf(b, test);
        LOG_MSG("LEN:%d", b->len);
        tld_sprintf(b, test);
        LOG_MSG("LEN:%d", b->len);
        tld_sprintf(b, test);
        LOG_MSG("LEN:%d", b->len);

        tld_sprintf(b, test);
        LOG_MSG("LEN:%d", b->len);
        RUN(tld_sprintf(b, test));
        LOG_MSG("LEN:%d", b->len);
        fprintf(stdout,"%s ; LEN:%d\n", TLD_STR(b), TLD_STRLEN(b));

        tld_strbuf_free(b);
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}
