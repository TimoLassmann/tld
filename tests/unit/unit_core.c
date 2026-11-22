#include "tld.h"
#include "test.h"

/* unit-core MESSAGE ...: each message through the message macros. */
static int demo(int n, char **args)
{
        SECTION_MSG("message macros");
        for(int i = 0; i < n; i++){
                LOG_MSG("%s", args[i]);
                WARNING_MSG("%s", args[i]);
                KV_MSG("message", "%s", args[i]);
                OK_MSG("%s", args[i]);
                DEBUG_MSG("%s", args[i]);
        }
        fprintf(stdout, "%d message%s at TLD_VERBOSE %d\n", n, n == 1 ? "" : "s", tld_verbosity());
        return OK;
}

int main(int argc, char *argv[])
{
        test_init(&argc, argv, "[MESSAGE ...]",
                  "With no arguments: writes one LOG_MSG.\n"
                  "With MESSAGE: shows each message through LOG_MSG, WARNING_MSG, KV_MSG,\n"
                  "OK_MSG and DEBUG_MSG (stderr), so -v and -vv show what each level prints.\n"
                  "Example: unit-core -vv \"hello there\"");
        if(argc > 1){
                return demo(argc - 1, argv + 1) == OK ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        /* Tests for LOG_MSG */
        SECTION_MSG("LOG_MSG");
        LOG_MSG("This is a log message");
        OK_MSG("LOG_MSG returned");
        return EXIT_SUCCESS;
}
