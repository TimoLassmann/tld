/* Regression tests for the misc fixes.
 * - tld_get_full_path never set *full when it allocated the buffer (caller got
 *   NULL, buffer leaked) and freed a caller-supplied buffer on error (the
 *   caller's later use/free was a use-after-free / double free under ASan).
 * - tld_dir_create ignored mkdir's result (returned OK when nothing was made)
 *   and printed an error in the normal case.
 * - tld_get_dirname("/file") failed with "No dirname".
 */
#include "tld.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

static int test_full_path(const char *dir)
{
        tld_strbuf *full = NULL;
        tld_strbuf *mine = NULL;
        char file[1024];
        FILE *f = NULL;

        snprintf(file, sizeof(file), "%s/afile.txt", dir);
        f = fopen(file, "w");
        ASSERT(f != NULL, "could not create %s", file);
        fclose(f);

        /* NULL buffer: the function allocates one and hands it back */
        RUN(tld_get_full_path(&full, file));
        ASSERT(full != NULL, "*full was not set");
        ASSERT(full->len > 0 && TLD_STR(full)[0] == '/', "not an absolute path: %s", TLD_STR(full));
        ASSERT(strstr(TLD_STR(full), "afile.txt") != NULL, "wrong path %s", TLD_STR(full));

        /* reuse the same buffer */
        RUN(tld_get_full_path(&full, file));
        ASSERT(strstr(TLD_STR(full), "afile.txt") != NULL, "wrong path %s", TLD_STR(full));

        /* caller-supplied buffer survives an error */
        RUN(tld_strbuf_alloc(&mine, 16));
        RUN(tld_append(mine, "keep"));
        ASSERT(tld_get_full_path(&mine, "/no/such/file/anywhere") == FAIL, "missing file accepted");
        ASSERT(mine != NULL, "caller buffer pointer was cleared");
        RUN(tld_append(mine, "x"));

        /* NULL buffer and an error: nothing handed back */
        tld_strbuf *none = NULL;
        ASSERT(tld_get_full_path(&none, "/no/such/file/anywhere") == FAIL, "missing file accepted");
        ASSERT(none == NULL, "buffer handed back on error");

        unlink(file);
        tld_strbuf_free(mine);
        tld_strbuf_free(full);
        return OK;
ERROR:
        tld_strbuf_free(mine);
        tld_strbuf_free(full);
        return FAIL;
}

static int test_dir_create(const char *dir)
{
        char sub[1024];
        char deep[1024];
        char file[1024];
        struct stat st;
        FILE *f = NULL;

        snprintf(sub, sizeof(sub), "%s/sub", dir);
        snprintf(deep, sizeof(deep), "%s/missing/parent/child", dir);
        snprintf(file, sizeof(file), "%s/plainfile", dir);

        RUN(tld_dir_create(sub));
        ASSERT(stat(sub, &st) == 0 && S_ISDIR(st.st_mode), "%s was not created", sub);
        /* already there: still OK */
        RUN(tld_dir_create(sub));

        /* mkdir fails (no parent): must be reported */
        ASSERT(tld_dir_create(deep) == FAIL, "tld_dir_create returned OK but made nothing");
        ASSERT(stat(deep, &st) != 0, "unexpected directory");

        /* a plain file is in the way */
        f = fopen(file, "w");
        ASSERT(f != NULL, "could not create %s", file);
        fclose(f);
        ASSERT(tld_dir_create(file) == FAIL, "tld_dir_create accepted a plain file");

        unlink(file);
        rmdir(sub);
        return OK;
ERROR:
        return FAIL;
}

static int check_dirname(char *path, const char *expect)
{
        char *out = NULL;
        RUN(tld_get_dirname(path, &out));
        ASSERT(strcmp(out, expect) == 0, "dirname(%s) = '%s', expected '%s'", path, out, expect);
        MFREE(out);
        return OK;
ERROR:
        MFREE(out);
        return FAIL;
}

static int test_dirname(void)
{
        char *out = NULL;
        RUN(check_dirname("/file", "/"));
        RUN(check_dirname("/", "/"));
        RUN(check_dirname("/a/b/file.txt", "/a/b"));
        RUN(check_dirname("a/file.txt", "a"));
        ASSERT(tld_get_dirname("file.txt", &out) == FAIL, "dirname of a bare name succeeded");
        ASSERT(out == NULL, "out set on failure");
        return OK;
ERROR:
        return FAIL;
}

static int test_cmd_line(void)
{
        tld_strbuf *cmd = NULL;
        char *argv[] = {"prog", "-x", "1"};
        RUN(tld_capture_cmd_line(&cmd, 3, argv));
        ASSERT(strcmp(TLD_STR(cmd), "prog -x 1 ") == 0, "got '%s'", TLD_STR(cmd));
        tld_strbuf_free(cmd);
        return OK;
ERROR:
        tld_strbuf_free(cmd);
        return FAIL;
}

int main(void)
{
        char tmpl[] = "/tmp/tld_fix_misc_XXXXXX";
        char *dir = mkdtemp(tmpl);
        ASSERT(dir != NULL, "mkdtemp failed");

        RUN(test_full_path(dir));
        RUN(test_dir_create(dir));
        RUN(test_dirname());
        RUN(test_cmd_line());

        rmdir(dir);
        LOG_MSG("unit_fix_misc: all passed");
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}
