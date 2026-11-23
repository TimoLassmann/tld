/* ai-arena: results kept without ai_reset() produce one warning on stderr at 100000,
 * and ai_reset() starts the count again. No server: ai_fmt is local. */
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "tld.h"
#include "tld_ai.h"
#include "test.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Run n ai_fmt calls with stderr sent to a temp file; return how many warnings. */
static int warnings_during(int n)
{
        char path[] = "/tmp/ai-arena-XXXXXX";
        int fd = mkstemp(path);
        if (fd < 0) return -1;
        fflush(stderr);
        int saved = dup(2);
        dup2(fd, 2);
        for (int i = 0; i < n; i++) {
                if (!ai_fmt("value %d", i)) { n = -1; break; }
        }
        fflush(stderr);
        dup2(saved, 2);
        close(saved);
        int count = 0;
        FILE *f = fdopen(fd, "r");
        char line[512];
        rewind(f);
        while (fgets(line, sizeof line, f)) count += strstr(line, "call ai_reset()") != NULL;
        fclose(f);
        unlink(path);
        return n < 0 ? -1 : count;
}

int main(int argc, char *argv[])
{
        test_init_keep(&argc, argv, "",
                       "Checks the MEMORY warning of tld_ai.h: one warning when 100000\n"
                       "results are kept, none before, and a fresh count after ai_reset().");
        int w = warnings_during(99999);
        KV_MSG("warnings for 99999 kept", "%d", w);
        if (w != 0) { fprintf(stderr, "ai-arena: %d warnings before the limit\n", w); return 1; }
        w = warnings_during(2);
        KV_MSG("warnings at 100000", "%d", w);
        if (w != 1) { fprintf(stderr, "ai-arena: %d warnings at the limit, want 1\n", w); return 1; }
        w = warnings_during(50000);
        if (w != 0) { fprintf(stderr, "ai-arena: warned again without a reset\n"); return 1; }
        ai_reset();
        w = warnings_during(100000);
        if (w != 1) { fprintf(stderr, "ai-arena: %d warnings after ai_reset, want 1\n", w); return 1; }
        ai_reset();
        OK_MSG("one warning per 100000 results kept without ai_reset()");
        return 0;
}
