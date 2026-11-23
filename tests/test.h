/* tests/test.h - command line for the test programs (not part of the library).
 *
 *   int main(int argc, char *argv[])
 *   {
 *           test_init(&argc, argv, "[TEXT ...]",
 *                     "With no arguments runs the checks; with TEXT runs the string functions on it.");
 *           if(argc > 1) ... argv[1..argc-1] are the positional arguments ...
 *   }
 *
 * Options are parsed with getopt_long (as in kalign), so they may come before or after
 * the positional arguments. test_init then moves the positional arguments to
 * argv[1..argc-1] (argc is updated). Options:
 *   -h, --help      usage on stdout, exit 0
 *   -v, --verbose   more output; -v sets TLD_VERBOSE=1, -vv (or -v -v) sets 2
 *   --              end of options; needed before negative numbers: unit-sort -- 5 -3 9
 * Any other option prints the usage on stderr and exits 2.
 *
 * Without -v and without TLD_VERBOSE in the environment the program runs quiet
 * (TLD_VERBOSE=0: only errors show). test_init_keep() leaves the library default
 * (TLD_VERBOSE unset means 1) for programs whose warnings a caller checks. */
#ifndef TLD_TEST_H
#define TLD_TEST_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void test_usage_(FILE *f, const char *prog, const char *args, const char *help, int quiet)
{
        const char *base = strrchr(prog, '/');
        base = base ? base + 1 : prog;
        fprintf(f, "Usage: %s [-h] [-v] %s\n", base, args ? args : "");
        if(help && *help){
                fprintf(f, "\n%s\n", help);
        }
        fprintf(f, "\nOptions:\n"
                "  -h, --help      show this help and exit\n"
                "  -v, --verbose   show progress and results on stderr (-vv: also debug detail)\n");
        if(quiet){
                fprintf(f, "\nWithout -v (and without TLD_VERBOSE set) only errors are shown.\n");
        }
}

static inline void test_init_with_(int *argc, char *argv[], const char *args, const char *help, int quiet)
{
        static struct option long_options[] = {
                {"help", no_argument, 0, 'h'},
                {"verbose", no_argument, 0, 'v'},
                {0, 0, 0, 0}
        };
        int verbose = 0;
        int c;
        int out = 1;
        while((c = getopt_long(*argc, argv, "hv", long_options, NULL)) != -1){
                switch(c){
                case 'h':
                        test_usage_(stdout, argv[0], args, help, quiet);
                        exit(EXIT_SUCCESS);
                case 'v':
                        verbose++;
                        break;
                default: /* getopt_long has printed what was wrong */
                        fputc('\n', stderr);
                        test_usage_(stderr, argv[0], args, help, quiet);
                        exit(2);
                }
        }
        for(int i = optind; i < *argc; i++){
                argv[out++] = argv[i];
        }
        argv[out] = NULL;
        *argc = out;
        if(verbose){
                setenv("TLD_VERBOSE", verbose >= 2 ? "2" : "1", 1);
        } else if(quiet && getenv("TLD_VERBOSE") == NULL){
                setenv("TLD_VERBOSE", "0", 1);
        }
}

/* Parse -h/-v; quiet unless -v or TLD_VERBOSE. */
static inline void test_init(int *argc, char *argv[], const char *args, const char *help)
{
        test_init_with_(argc, argv, args, help, 1);
}

/* Same, but without -v the TLD_VERBOSE default (1) is kept. */
static inline void test_init_keep(int *argc, char *argv[], const char *args, const char *help)
{
        test_init_with_(argc, argv, args, help, 0);
}

#endif
