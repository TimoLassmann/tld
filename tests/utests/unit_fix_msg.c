/* unit-fix-msg: the rich output macros write plain text to stderr when it is not a
 * terminal, honour TLD_VERBOSE, and ERROR_MSG reports the caller's file and line. */
#define _POSIX_C_SOURCE 200809L
#include "tld.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char buf[8192];
static int err_line;

static int fails(void)
{
        err_line = __LINE__ + 1;
        ERROR_MSG("boom %d", 7);
        return OK;
ERROR:
        return FAIL;
}

static int has(const char *needle)
{
        if (strstr(buf, needle)) return 1;
        fprintf(stdout, "missing: [%s]\n", needle);
        return 0;
}

int main(void)
{
        FILE *f = tmpfile();
        int ok = 1;
        char where[256];
        if (!f) return EXIT_FAILURE;

        setenv("TLD_VERBOSE", "1", 1);
        fflush(stderr);
        int saved = dup(fileno(stderr));
        dup2(fileno(f), fileno(stderr));

        SECTION_MSG("Stage %d", 1);
        KV_MSG("model", "%s", "m1");
        OK_MSG("done in %.1f s", 1.5);
        DEBUG_MSG("hidden at level 1");
        for (long i = 1; i <= 50; i++) tld_progress(i, 50, "work");
        WARNING_MSG("careful");
        fails();
        setenv("TLD_VERBOSE", "2", 1);
        DEBUG_MSG("shown at level 2");
        setenv("TLD_VERBOSE", "0", 1);
        LOG_MSG("hidden at level 0");
        KV_MSG("hidden", "%s", "x");
        fails();

        fflush(stderr);
        dup2(saved, fileno(stderr));
        rewind(f);
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        buf[n] = 0;
        fclose(f);

        snprintf(where, sizeof where, "unit_fix_msg.c line %d)", err_line);
        ok &= has("== Stage 1 ==\n");
        ok &= has("   model     m1\n");
        ok &= has(" OK done in 1.5 s\n");
        ok &= has("   work 50/50 done\n");
        ok &= has("careful");
        ok &= has("boom 7");
        ok &= has(where);
        ok &= has(" . shown at level 2\n");
        ok &= !strstr(buf, "hidden");
        ok &= !strstr(buf, "\033[");
        /* the ERROR_MSG at level 0 is still printed: two "boom" lines */
        ok &= strstr(strstr(buf, "boom 7") + 1, "boom 7") != NULL;
        if (!ok) {
                fprintf(stdout, "---- captured ----\n%s", buf);
                return EXIT_FAILURE;
        }
        fprintf(stdout, "unit-fix-msg: ok\n");
        return EXIT_SUCCESS;
}
