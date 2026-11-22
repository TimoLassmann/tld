#include "tld-path.h"

#include "../core/tld-core.h"
#include "../string/tld-string.h"

#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The output buffer of a path function: the caller's *out, cleared, or a new one
   (*allocated set, so the error path frees only what the call made). */
static int tld__path_out(tld_strbuf **out, tld_strbuf **b, int *allocated)
{
        *allocated = 0;
        if(*out){
                *b = *out;
                RUN(tld_strbuf_clear(*b));
        }else{
                RUN(tld_strbuf_alloc(b, 128));
                *allocated = 1;
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_path_exists(const char *path)
{
        struct stat st;
        ASSERT(path != NULL, "No path");
        return stat(path, &st) == 0;
ERROR:
        return 0;
}

int tld_path_is_dir(const char *path)
{
        struct stat st;
        ASSERT(path != NULL, "No path");
        return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
ERROR:
        return 0;
}

int tld_path_mkdir(const char *dir)
{
        struct stat st;
        ASSERT(dir != NULL, "No directory name");
        if(mkdir(dir, 0700) == 0){
                LOG_MSG("Creating output directory");
                return OK;
        }
        if(errno == EEXIST){
                if(stat(dir, &st) == 0 && S_ISDIR(st.st_mode)){
                        return OK;
                }
                ERROR_MSG("Could not create directory %s: a file of that name exists", dir);
        }
        ERROR_MSG("Could not create directory %s: %s", dir, strerror(errno));
ERROR:
        return FAIL;
}

int tld_path_full(tld_strbuf **full, const char *path)
{
        char *full_path = NULL;
        tld_strbuf *f = NULL;
        int allocated = 0;
        ASSERT(full != NULL, "No output buffer pointer");
        ASSERT(path != NULL, "No path");
        full_path = realpath(path, NULL);
        if(!full_path){
                ERROR_MSG("Could not get the full path of %s: %s", path, strerror(errno));
        }
        RUN(tld__path_out(full, &f, &allocated));
        RUN(tld_strbuf_append(f, full_path));
        free(full_path);
        *full = f;
        return OK;
ERROR:
        free(full_path);
        if(allocated){
                tld_strbuf_free(f);
        }
        return FAIL;
}

int tld_cmdline_capture(tld_strbuf **cmdline, const int argc, char *const argv[])
{
        tld_strbuf *cmd = NULL;
        int allocated = 0;
        ASSERT(cmdline != NULL, "No output buffer pointer");
        ASSERT(argc >= 0, "Negative argc: %d", argc);
        ASSERT(argc == 0 || argv != NULL, "No argv");
        RUN(tld__path_out(cmdline, &cmd, &allocated));
        for(int i = 0; i < argc; i++){
                ASSERT(argv[i] != NULL, "argv[%d] is NULL", i);
                RUN(tld_strbuf_append(cmd, argv[i]));
                RUN(tld_strbuf_append_char(cmd, ' '));
        }
        *cmdline = cmd;
        return OK;
ERROR:
        if(allocated){
                tld_strbuf_free(cmd);
        }
        return FAIL;
}

int tld_path_filename(const char *path, tld_strbuf **out)
{
        tld_strbuf *b = NULL;
        const char *slash = NULL;
        int allocated = 0;
        ASSERT(path != NULL, "No path");
        ASSERT(out != NULL, "No output buffer pointer");
        slash = strrchr(path, '/');
        slash = slash ? slash + 1 : path;
        if(*slash == 0){
                ERROR_MSG("No filename found in: %s", path);
        }
        RUN(tld__path_out(out, &b, &allocated));
        RUN(tld_strbuf_append(b, slash));
        *out = b;
        return OK;
ERROR:
        if(allocated){
                tld_strbuf_free(b);
        }
        return FAIL;
}

int tld_path_dirname(const char *path, tld_strbuf **out)
{
        tld_strbuf *b = NULL;
        const char *slash = NULL;
        size_t len = 0;
        int allocated = 0;
        ASSERT(path != NULL, "No path");
        ASSERT(out != NULL, "No output buffer pointer");
        slash = strrchr(path, '/');
        if(slash == NULL){
                ERROR_MSG("No dirname found in: %s", path);
        }
        /* the only slash is the leading one: the directory is the root */
        len = slash == path ? 1 : (size_t) (slash - path);
        RUN(tld__path_out(out, &b, &allocated));
        RUN(tld_strbuf_append_n(b, path, len));
        *out = b;
        return OK;
ERROR:
        if(allocated){
                tld_strbuf_free(b);
        }
        return FAIL;
}

int tld_path_read(const char *path, tld_strbuf **out)
{
        FILE *f = NULL;
        tld_strbuf *b = NULL;
        char buf[1 << 16];
        size_t got = 0;
        int allocated = 0;
        ASSERT(path != NULL, "No path");
        ASSERT(out != NULL, "No output buffer pointer");
        f = fopen(path, "rb");
        if(f == NULL){
                ERROR_MSG("Cannot open %s: %s", path, strerror(errno));
        }
        RUN(tld__path_out(out, &b, &allocated));
        while((got = fread(buf, 1, sizeof buf, f)) > 0){
                if(tld_strbuf_append_n(b, buf, got) != OK){
                        ERROR_MSG("Cannot read %s: too large or out of memory", path);
                }
        }
        if(ferror(f)){
                ERROR_MSG("Cannot read %s: %s", path, strerror(errno));
        }
        fclose(f);
        *out = b;
        return OK;
ERROR:
        if(f){
                fclose(f);
        }
        if(allocated){
                tld_strbuf_free(b);
        }
        return FAIL;
}
