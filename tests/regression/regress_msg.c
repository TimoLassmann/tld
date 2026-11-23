/* regress-msg: the rich output macros write plain text to stderr when it is not a
 * terminal, honour TLD_VERBOSE, ERROR_MSG reports the caller's file and line, and no
 * message changes errno (isatty on a non-terminal stderr set it to ENOTTY). */
#define _POSIX_C_SOURCE 200809L
#include "tld.h"
#include "test.h"
#include <errno.h>
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

int main(int argc, char *argv[])
{
        test_init(&argc, argv, "",
                  "Checks that SECTION_MSG, KV_MSG, OK_MSG, DEBUG_MSG, tld_progress, WARNING_MSG\n"
                  "and ERROR_MSG write plain text to stderr when it is not a terminal, honour\n"
                  "TLD_VERBOSE (0, 1, 2), that ERROR_MSG names the caller's file and line, and\n"
                  "that no message changes errno.\n"
                  "Takes no arguments; with -v it shows the captured output.");
        if (argc > 1) {
                fprintf(stderr, "regress-msg takes no arguments (see -h)\n");
                return 2;
        }
        FILE *f = tmpfile();
        int ok = 1;
        int n_errno_kept = 0;
        char where[256];
        char level[8];
        const char *base = strrchr(__FILE__, '/');
        if (!f) return EXIT_FAILURE;
        snprintf(level, sizeof level, "%d", tld_verbosity());
        SECTION_MSG("capturing stderr at TLD_VERBOSE 1, 2 and 0");

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
        /* every message leaves errno as it was, so a caller can log and then report it */
        errno = EDOM; LOG_MSG("errno %d", 1);         n_errno_kept += errno == EDOM;
        errno = EDOM; WARNING_MSG("errno %d", 2);     n_errno_kept += errno == EDOM;
        errno = EDOM; fails();                        n_errno_kept += errno == EDOM;
        errno = EDOM; SECTION_MSG("errno %d", 4);     n_errno_kept += errno == EDOM;
        errno = EDOM; KV_MSG("errno", "%d", 5);       n_errno_kept += errno == EDOM;
        errno = EDOM; OK_MSG("errno %d", 6);          n_errno_kept += errno == EDOM;
        errno = EDOM; tld_progress(7, 7, "errno");    n_errno_kept += errno == EDOM;
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
        setenv("TLD_VERBOSE", level, 1);
        DEBUG_MSG("captured %zu bytes:\n%s", n, buf);

        snprintf(where, sizeof where, "%s line %d)", base ? base + 1 : __FILE__, err_line);
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
        if (n_errno_kept != 7) {
                fprintf(stdout, "%d of 7 messages changed errno\n", 7 - n_errno_kept);
                ok = 0;
        }
        if (!ok) {
                fprintf(stdout, "---- captured ----\n%s", buf);
                return EXIT_FAILURE;
        }
        OK_MSG("all %d checks on the captured output and errno passed", 12);
        return EXIT_SUCCESS;
}
