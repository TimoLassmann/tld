#include "tld.h"
#include "test.h"
#include <stdio.h>

/* unit-misc PATH ...: the path helpers on the user's paths. */
static int demo(int n, char **args)
{
        for(int i = 0; i < n; i++){
                tld_strbuf *full = NULL;
                char *name = NULL;
                char *dir = NULL;
                SECTION_MSG("%s", args[i]);
                fprintf(stdout, "%s\n", args[i]);
                int exists = tld_path_exists(args[i]) == OK;
                fprintf(stdout, "  exists:      %s\n", exists ? "yes" : "no");
                if(tld_path_filename(args[i], &name) == OK){
                        fprintf(stdout, "  filename:    %s\n", name);
                        MFREE(name);
                }
                if(tld_path_dirname(args[i], &dir) == OK){
                        fprintf(stdout, "  dirname:     %s\n", dir);
                        MFREE(dir);
                }
                if(exists && tld_path_full(&full, args[i]) == OK){
                        fprintf(stdout, "  full path:   %s\n", TLD_STR(full));
                }
                if(full){
                        tld_strbuf_free(full);
                }
        }
        OK_MSG("done");
        return OK;
}

int main(int argc, char *argv[])
{
        char **orig = NULL;
        int orig_n = argc;
        /* keep the full command line (with options) for tld_cmdline_capture */
        orig = malloc(sizeof(char *) * (size_t)(argc + 1));
        if(!orig){
                return EXIT_FAILURE;
        }
        for(int i = 0; i <= argc; i++){
                orig[i] = argv[i];
        }
        test_init(&argc, argv, "[PATH ...]",
                  "With no arguments: captures the command line with tld_cmdline_capture.\n"
                  "With PATH: for each path prints whether it exists, its filename, dirname\n"
                  "and (if it exists) full path (tld_path_exists, tld_path_filename,\n"
                  "tld_path_dirname, tld_path_full).\n"
                  "Example: unit-misc /etc/hosts .");

        tld_strbuf* cmd = NULL;

        SECTION_MSG("tld_cmdline_capture");
        RUN(tld_cmdline_capture(&cmd, orig_n, orig));

        KV_MSG("command line", "%s", TLD_STR(cmd));

        tld_strbuf_free(cmd);
        cmd = NULL;
        OK_MSG("captured");

        if(argc > 1){
                RUN(demo(argc - 1, argv + 1));
        }
        free(orig);
        return EXIT_SUCCESS;
ERROR:
        if(cmd){
                tld_strbuf_free(cmd);
        }
        free(orig);
        return EXIT_FAILURE;
}
