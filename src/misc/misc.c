#include "misc.h"

#include "../core/tld-core.h"
#include "../alloc/tld-alloc.h"

#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#define MAX_CMD_LEN 16384

int tld_file_exists(const char* name)
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

int tld_dir_exists(const char* name)
{
        ASSERT(name != NULL," No directory name!");
        DIR* dir = opendir(name);
        if (dir) {
                /* Directory exists. */
                closedir(dir);
        } else if (ENOENT == errno) {
                ERROR_MSG("Directory %s does not exist!", name);
                /* Directory does not exist. */
        } else {
                ERROR_MSG("Opening directory %s failed!", name);
                /* opendir() failed for some other reason. */
        }
        return OK;
ERROR:
        return FAIL;
}

/* I don't like that both libgen and string have functions to work with
   directory / filenames. The functions below copy the input path and alloc
   a new character array to store the output. Needs to be MFREE'd...
 */
int tlfilename(char* path, char** out)
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

int tldirname(char* path, char** out)
{
        char* tmp = NULL;
        int len = 0;
        int i = 0;
        int c = 0;
        int e = 0;

        len = (int) strlen(path);
        MMALLOC(tmp, sizeof(char) * (len+1));
        c = 0;
        e = 0;
        for(i = 0;i < len;i++){

                tmp[c] = path[i];
                if(path[i] == '/'){
                        e = c;
                }
                c++;
        }
        tmp[e] = 0;
        if(e == 0){
                ERROR_MSG("No dirname found in: %s", path);
        }
        *out = tmp;
        return OK;
ERROR:
        if(tmp){
                MFREE(tmp);
        }
        return FAIL;
}

int make_cmd_line(char** command, const int argc,char* const argv[])
{
        char* cmd = NULL;

        int i = 0;
        int j = 0;
        int c = 0;
        int g = 0;
        int alloc_len = 16;

        int old_len = 0;
        galloc(&cmd,alloc_len);
        for(i =0 ; i < alloc_len;i++){
                cmd[i] = 0;
        }
        c = 0;
        for(i = 0; i < argc;i++){
                //fprintf(stdout,"%s\n", argv[i]);
                for(j = 0; j < (int) strlen(argv[i]);j++){
                        if(c == 16384-1){
                                break;
                        }
                        cmd[c] = argv[i][j];
                        c++;
                        if(c == alloc_len){
                                old_len = alloc_len;
                                alloc_len = alloc_len + alloc_len /2;
                                galloc(&cmd,alloc_len);
                                for(g = old_len; g < alloc_len;g++){
                                        cmd[g] = 0;
                                }
                        }

                }
                if(c >= MAX_CMD_LEN){
                        ERROR_MSG("Command line too long! Allocated: %d", alloc_len);
                }
                if(c == 16384-1){
                        break;
                }
                cmd[c] = ' ';
                c++;
                if(c == alloc_len){
                        old_len = alloc_len;
                        alloc_len = alloc_len + alloc_len /2;
                        galloc(&cmd,alloc_len);
                        for(g = old_len; g < alloc_len;g++){
                                cmd[g] = 0;
                        }
                }
        }
        cmd[c] = 0;
        *command = cmd;
        return OK;
ERROR:
        if(cmd){
                gfree(cmd);
        }
        return FAIL;
}
