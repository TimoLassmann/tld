/* Regression tests for the misc fixes.
 * - tld_path_full never set *full when it allocated the buffer (caller got
 *   NULL, buffer leaked) and freed a caller-supplied buffer on error (the
 *   caller's later use/free was a use-after-free / double free under ASan).
 * - tld_path_mkdir ignored mkdir's result (returned OK when nothing was made)
 *   and printed an error in the normal case.
 * - tld_path_dirname("/file") failed with "No dirname".
 * - tld_path_exists / tld_path_is_dir answered OK (0) for yes; they answer 1/0 now, and
 *   tld_path_is_dir no longer prints an error for "no".
 */
#include "tld.h"
#include "test.h"
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
        RUN(tld_path_full(&full, file));
        ASSERT(full != NULL, "*full was not set");
        ASSERT(full->len > 0 && TLD_STR(full)[0] == '/', "not an absolute path: %s", TLD_STR(full));
        ASSERT(strstr(TLD_STR(full), "afile.txt") != NULL, "wrong path %s", TLD_STR(full));

        /* reuse the same buffer */
        RUN(tld_path_full(&full, file));
        ASSERT(strstr(TLD_STR(full), "afile.txt") != NULL, "wrong path %s", TLD_STR(full));

        /* caller-supplied buffer survives an error */
        RUN(tld_strbuf_alloc(&mine, 16));
        RUN(tld_strbuf_append(mine, "keep"));
        ASSERT(tld_path_full(&mine, "/no/such/file/anywhere") == FAIL, "missing file accepted");
        ASSERT(mine != NULL, "caller buffer pointer was cleared");
        RUN(tld_strbuf_append(mine, "x"));

        /* NULL buffer and an error: nothing handed back */
        tld_strbuf *none = NULL;
        ASSERT(tld_path_full(&none, "/no/such/file/anywhere") == FAIL, "missing file accepted");
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

        RUN(tld_path_mkdir(sub));
        ASSERT(stat(sub, &st) == 0 && S_ISDIR(st.st_mode), "%s was not created", sub);
        /* already there: still OK */
        RUN(tld_path_mkdir(sub));

        /* mkdir fails (no parent): must be reported */
        ASSERT(tld_path_mkdir(deep) == FAIL, "tld_path_mkdir returned OK but made nothing");
        ASSERT(stat(deep, &st) != 0, "unexpected directory");

        /* a plain file is in the way */
        f = fopen(file, "w");
        ASSERT(f != NULL, "could not create %s", file);
        fclose(f);
        ASSERT(tld_path_mkdir(file) == FAIL, "tld_path_mkdir accepted a plain file");

        unlink(file);
        rmdir(sub);
        return OK;
ERROR:
        return FAIL;
}

static int check_dirname(const char *path, const char *expect)
{
        tld_strbuf *out = NULL;
        RUN(tld_path_dirname(path, &out));
        ASSERT(strcmp(TLD_STR(out), expect) == 0, "dirname(%s) = '%s', expected '%s'", path, TLD_STR(out), expect);
        tld_strbuf_free(out);
        return OK;
ERROR:
        tld_strbuf_free(out);
        return FAIL;
}

static int check_filename(const char *path, const char *expect)
{
        tld_strbuf *out = NULL;
        RUN(tld_path_filename(path, &out));
        ASSERT(strcmp(TLD_STR(out), expect) == 0, "filename(%s) = '%s', expected '%s'", path, TLD_STR(out), expect);
        tld_strbuf_free(out);
        return OK;
ERROR:
        tld_strbuf_free(out);
        return FAIL;
}

static int test_dirname(void)
{
        tld_strbuf *out = NULL;
        RUN(check_dirname("/file", "/"));
        RUN(check_dirname("/", "/"));
        RUN(check_dirname("/a/b/file.txt", "/a/b"));
        RUN(check_dirname("a/file.txt", "a"));
        RUN(check_filename("/a/b/file.txt", "file.txt"));
        RUN(check_filename("file.txt", "file.txt"));
        RUN(check_filename("/x", "x"));
        ASSERT(tld_path_dirname("file.txt", &out) == FAIL, "dirname of a bare name succeeded");
        ASSERT(out == NULL, "out set on failure");
        ASSERT(tld_path_filename("/a/", &out) == FAIL, "filename of a directory path succeeded");
        ASSERT(tld_path_filename("", &out) == FAIL, "filename of an empty path succeeded");
        ASSERT(out == NULL, "out set on failure");

        /* a caller's buffer is cleared and reused, not appended to */
        RUN(tld_path_filename("/a/first", &out));
        RUN(tld_path_filename("/b/second", &out));
        ASSERT(strcmp(TLD_STR(out), "second") == 0, "reused filename buffer holds '%s'", TLD_STR(out));
        RUN(tld_path_dirname("/c/d/x", &out));
        ASSERT(strcmp(TLD_STR(out), "/c/d") == 0, "reused dirname buffer holds '%s'", TLD_STR(out));
        tld_strbuf_free(out);
        return OK;
ERROR:
        tld_strbuf_free(out);
        return FAIL;
}

/* tld_path_exists / tld_path_is_dir answer 1 or 0 and print nothing. */
static int test_predicates(const char *dir)
{
        char file[1024];
        char missing[1024];
        FILE *f = NULL;
        snprintf(file, sizeof file, "%s/pfile", dir);
        snprintf(missing, sizeof missing, "%s/no-such-entry", dir);
        f = fopen(file, "w");
        ASSERT(f != NULL, "could not create %s", file);
        fclose(f);
        ASSERT(tld_path_exists(file) == 1, "file does not exist");
        ASSERT(tld_path_exists(dir) == 1, "directory does not exist");
        ASSERT(tld_path_exists(missing) == 0, "missing entry exists");
        ASSERT(tld_path_is_dir(dir) == 1, "directory is not a directory");
        ASSERT(tld_path_is_dir(file) == 0, "file is a directory");
        ASSERT(tld_path_is_dir(missing) == 0, "missing entry is a directory");
        unlink(file);
        return OK;
ERROR:
        return FAIL;
}

static int test_cmd_line(void)
{
        tld_strbuf *cmd = NULL;
        char *argv[] = {"prog", "-x", "1"};
        RUN(tld_cmdline_capture(&cmd, 3, argv));
        ASSERT(strcmp(TLD_STR(cmd), "prog -x 1 ") == 0, "got '%s'", TLD_STR(cmd));
        RUN(tld_cmdline_capture(&cmd, 2, argv));                /* replaces, not appends */
        ASSERT(strcmp(TLD_STR(cmd), "prog -x ") == 0, "reused buffer holds '%s'", TLD_STR(cmd));
        tld_strbuf_free(cmd);
        return OK;
ERROR:
        tld_strbuf_free(cmd);
        return FAIL;
}

static int test_read(const char *dir)
{
        char file[1024];
        FILE *f = NULL;
        tld_strbuf *b = NULL;
        tld_strbuf *keep = NULL;
        tld_strbuf *kept = NULL;
        static char big[200000];
        for(size_t i = 0; i < sizeof big; i++){
                big[i] = (char) (i % 251);              /* NUL bytes included */
        }
        snprintf(file, sizeof file, "%s/data.bin", dir);
        f = fopen(file, "wb");
        ASSERT(f != NULL, "cannot write %s", file);
        ASSERT(fwrite(big, 1, sizeof big, f) == sizeof big, "short write");
        fclose(f);
        f = NULL;
        RUN(tld_path_read(file, &b));
        ASSERT(TLD_STRLEN(b) == (int) sizeof big, "read %d bytes", TLD_STRLEN(b));
        ASSERT(memcmp(TLD_STR(b), big, sizeof big) == 0, "contents differ");
        ASSERT(TLD_STR(b)[sizeof big] == 0, "not NUL-terminated");
        tld_strbuf_free(b);
        b = NULL;

        snprintf(file, sizeof file, "%s/empty", dir);
        f = fopen(file, "wb");
        ASSERT(f != NULL, "cannot write %s", file);
        fclose(f);
        f = NULL;
        RUN(tld_path_read(file, &b));
        ASSERT(TLD_STRLEN(b) == 0 && TLD_STR(b)[0] == 0, "empty file read as %d bytes", TLD_STRLEN(b));
        tld_strbuf_free(b);
        b = NULL;

        /* a caller's buffer is cleared and reused */
        RUN(tld_strbuf_alloc(&keep, 8));
        RUN(tld_strbuf_append(keep, "old"));
        kept = keep;
        snprintf(file, sizeof file, "%s/data.bin", dir);
        RUN(tld_path_read(file, &keep));
        ASSERT(keep == kept && TLD_STRLEN(keep) == (int) sizeof big, "reused buffer: %d bytes", TLD_STRLEN(keep));

        snprintf(file, sizeof file, "%s/missing", dir);
        ASSERT(tld_path_read(file, &keep) == FAIL, "reading a missing file succeeded");
        ASSERT(keep == kept, "*out changed on failure");
        ASSERT(tld_path_read(dir, &b) == FAIL, "reading a directory succeeded");
        ASSERT(b == NULL, "*out set on failure");
        tld_strbuf_free(keep);
        return OK;
ERROR:
        if(f){
                fclose(f);
        }
        tld_strbuf_free(b);
        tld_strbuf_free(keep);
        return FAIL;
}

static const char *tmp_dir = NULL;

static int test_full_path_tmp(void)
{
        return test_full_path(tmp_dir);
}

static int test_dir_create_tmp(void)
{
        return test_dir_create(tmp_dir);
}

static int test_predicates_tmp(void)
{
        return test_predicates(tmp_dir);
}

static int test_read_tmp(void)
{
        return test_read(tmp_dir);
}

/* The checks, in the order a plain run does them; arguments select by name. */
static const struct {
        const char *name;
        int (*fn)(void);
        const char *about;
} checks[] = {
        {"full-path", test_full_path_tmp, "tld_path_full sets *full and keeps caller buffers"},
        {"dir-create", test_dir_create_tmp, "tld_path_mkdir reports mkdir failures"},
        {"dirname", test_dirname, "tld_path_dirname / tld_path_filename, also into a reused buffer"},
        {"predicates", test_predicates_tmp, "tld_path_exists / tld_path_is_dir answer 1 or 0"},
        {"cmd-line", test_cmd_line, "tld_cmdline_capture joins argv, replacing a reused buffer"},
        {"read", test_read_tmp, "tld_path_read reads whole files, NUL bytes included, and fails cleanly"},
};

int main(int argc, char *argv[])
{
        const int n_checks = (int) (sizeof checks / sizeof checks[0]);
        test_init(&argc, argv, "[CHECK ...]",
                  "Regression checks for the misc fixes. Works in a fresh directory under /tmp,\n"
                  "removed at the end.\n"
                  "\n"
                  "CHECK names (default: all, in this order):\n"
                  "  full-path   tld_path_full sets *full and keeps caller buffers\n"
                  "  dir-create  tld_path_mkdir reports mkdir failures\n"
                  "  dirname     tld_path_dirname / tld_path_filename, also into a reused buffer\n"
                  "  predicates  tld_path_exists / tld_path_is_dir answer 1 or 0\n"
                  "  cmd-line    tld_cmdline_capture joins argv, replacing a reused buffer\n"
                  "  read        tld_path_read reads whole files and fails cleanly\n"
                  "\n"
                  "Example: regress-misc dirname cmd-line");
        for(int i = 1; i < argc; i++){
                int k = 0;
                while(k < n_checks && strcmp(argv[i], checks[k].name) != 0){
                        k++;
                }
                if(k == n_checks){
                        fprintf(stderr, "regress-misc: no check named '%s' (see -h)\n", argv[i]);
                        return 2;
                }
        }
        char tmpl[] = "/tmp/tld_fix_misc_XXXXXX";
        tmp_dir = mkdtemp(tmpl);
        ASSERT(tmp_dir != NULL, "mkdtemp failed");
        DEBUG_MSG("working in %s", tmp_dir);
        for(int k = 0; k < n_checks; k++){
                int want = argc == 1;
                for(int i = 1; i < argc; i++){
                        want |= strcmp(argv[i], checks[k].name) == 0;
                }
                if(!want){
                        continue;
                }
                SECTION_MSG("%s: %s", checks[k].name, checks[k].about);
                test_expect_errors();
                RUN(checks[k].fn());
                test_expect_errors_end();
                OK_MSG("%s passed", checks[k].name);
        }
        rmdir(tmp_dir);
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}
