#include "tld-path.h"

#include "../core/tld-core.h"
#include "../alloc/tld-alloc.h"
#include "../string/tld-string.h"

#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#define TLD__MAX_CMD_LEN 16384

int tld_path_exists(const char* name)
{
        struct stat buf;
        int local_ret = 0;
        local_ret = stat( name, &buf );
        /* File found */
        if ( local_ret == 0 )
        {
                return OK;
        }
        return FAIL;
}

int tld_path_is_dir(const char* name)
{
        ASSERT(name != NULL,"No directory name");
        DIR* dir = opendir(name);
        if (dir) {
                /* Directory exists. */
                closedir(dir);
        } else if (ENOENT == errno) {
                ERROR_MSG("Directory %s does not exist", name);
                /* Directory does not exist. */
        } else {
                ERROR_MSG("Opening directory %s failed", name);
                /* opendir() failed for some other reason. */
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_path_mkdir(char* dir)
{
        struct stat st;
        ASSERT(dir != NULL, "No directory name");
        if(mkdir(dir, 0700) == 0){
                LOG_MSG("Creating output directory");
                return OK;
        }
        if(errno == EEXIST){
                if(stat(dir, &st) == 0 && S_ISDIR(st.st_mode)){
                        WARNING_MSG("Directory %s already exists",dir);
                        return OK;
                }
                ERROR_MSG("Could not create directory %s: a file of that name exists", dir);
        }
        ERROR_MSG("Could not create directory %s: %s", dir, strerror(errno));
ERROR:
        return FAIL;
}

int tld_path_full(tld_strbuf **full, char *filename)
{
        char* full_path = NULL;
        tld_strbuf *f = NULL;
        int allocated = 0;
        ASSERT(full != NULL, "tld_path_full: No output buffer pointer");
        ASSERT(filename != NULL, "tld_path_full: No filename");
        if(*full){
                f = *full;
                RUN(tld_strbuf_clear(f));
        }else{
                RUN(tld_strbuf_alloc(&f,512));
                allocated = 1;
        }
        if(tld_path_exists(filename) != OK){
                ERROR_MSG("File %s does not exist", filename);
        }

        full_path = realpath(filename, NULL);

        if(!full_path){
                ERROR_MSG("Could not get full path of %s", filename);
        }

        RUN(tld_strbuf_append(f, full_path));
        free(full_path);
        full_path = NULL;

        *full = f;
        return OK;
ERROR:
        free(full_path);
        /* free only what this call allocated; a caller-supplied buffer stays theirs */
        if(allocated){
                tld_strbuf_free(f);
        }
        return FAIL;

}

int tld_cmdline_capture(tld_strbuf **cmdline, const int argc, char *const argv[])
{

        tld_strbuf* cmd = NULL;
        int allocated = 0;

        ASSERT(cmdline != NULL, "No output buffer pointer");
        if(*cmdline == NULL){
                RUN(tld_strbuf_alloc(&cmd, 128));
                allocated = 1;
        }else{
                cmd = *cmdline;
        }
        for(int i = 0; i < argc;i++){
                RUN(tld_strbuf_append(cmd, argv[i]));
                RUN(tld_strbuf_append(cmd, " "));
        }

        *cmdline = cmd;
        return OK;
ERROR:
        if(allocated){
                tld_strbuf_free(cmd);
        }
        return FAIL;
}

/* I don't like that both libgen and string have functions to work with
   directory / filenames. The functions below copy the input path and alloc
   a new character array to store the output. Needs to be MFREE'd...
 */
int tld_path_filename(char* path, char** out)
{
        char* tmp = NULL;
        int len = 0;
        int i = 0;
        int c = 0;

        len = (int) strlen(path);
        MMALLOC(tmp, sizeof(char) * (len+1));
        c = 0;
        for(i = 0;i < len;i++){
                tmp[c] = path[i];
                c++;
                if(path[i] == '/'){
                        c = 0;
                }
        }
        tmp[c] = 0;
        if(c == 0){
                ERROR_MSG("No filename found in: %s", path);
        }
        *out = tmp;
        return OK;
ERROR:
        if(tmp){
                MFREE(tmp);
        }
        return FAIL;
}

int tld_path_dirname(char* path, char** out)
{
        char* tmp = NULL;
        int len = 0;
        int i = 0;
        int c = 0;
        int e = 0;

        len = (int) strlen(path);
        MMALLOC(tmp, sizeof(char) * (len+1));
        c = 0;
        e = -1;
        for(i = 0;i < len;i++){

                tmp[c] = path[i];
                if(path[i] == '/'){
                        e = c;
                }
                c++;
        }
        if(e < 0){
                ERROR_MSG("No dirname found in: %s", path);
        }
        if(e == 0){
                /* the only slash is the leading one: the directory is the root */
                e = 1;
        }
        tmp[e] = 0;
        *out = tmp;
        return OK;
ERROR:
        if(tmp){
                MFREE(tmp);
        }
        return FAIL;
}

#undef TLD__MAX_CMD_LEN
