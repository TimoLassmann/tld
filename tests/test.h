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
 * (TLD_VERBOSE unset means 1) for programs whose warnings a caller checks.
 *
 * Expected errors. A check that makes the library fail on purpose gets its ERROR lines
 * between
 *
 *   test_expect_errors();          stderr goes to a temporary file from here on
 *   ... calls that must fail ...
 *   n = test_expect_errors_end();  stderr is back; n = the ERROR lines that were hidden
 *
 * The end hides the library's ERROR lines and prints everything else that was written
 * (a failed check's own message, a sanitizer report, LOG lines) to stderr. With -v it
 * adds one line with the number of hidden lines; with -vv it shows the lines too. When
 * the program exits, or a sanitizer stops it, before the end is reached, all of the
 * captured text is printed, so a failure never loses its messages: reach the end only
 * on the path where the checks passed. */
#ifndef TLD_TEST_H
#define TLD_TEST_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <dlfcn.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static FILE *test_err_file_;     /* the captured text */
static int test_err_saved_ = -1; /* the real stderr while a capture runs, else -1 */

/* Put stderr back and print all of the captured text. Runs at exit and when a sanitizer
 * stops the program; only dup2, close, lseek, read and write. */
static inline void test_err_dump_(void)
{
        char buf[4096];
        ssize_t n;
        int fd;
        if(test_err_saved_ < 0){
                return;
        }
        fd = fileno(test_err_file_);
        dup2(test_err_saved_, STDERR_FILENO);
        close(test_err_saved_);
        test_err_saved_ = -1;
        lseek(fd, 0, SEEK_SET);
        while((n = read(fd, buf, sizeof buf)) > 0){
                if(write(STDERR_FILENO, buf, (size_t)n) < 0){
                        break;
                }
        }
}

/* The level of TLD_VERBOSE as the library reads it (unset: 1). */
static inline int test_verbosity_(void)
{
        const char *v = getenv("TLD_VERBOSE");
        return v && v[0] >= '0' && v[0] <= '9' && v[1] == 0 ? v[0] - '0' : 1;
}

/* From here to test_expect_errors_end() the library's ERROR lines are expected. */
static inline void test_expect_errors(void)
{
        if(test_err_saved_ >= 0){
                return;
        }
        if(test_err_file_ == NULL){
                void (*set_death)(void (*)(void)) = NULL;
                void *self = dlopen(NULL, RTLD_LAZY);
                test_err_file_ = tmpfile();
                if(test_err_file_ == NULL){ /* no capture: the lines just show */
                        return;
                }
                atexit(test_err_dump_);
                if(self){ /* a sanitizer report ends the program without the exit handlers */
                        *(void **)&set_death = dlsym(self, "__sanitizer_set_death_callback");
                        if(set_death){
                                set_death(test_err_dump_);
                        }
                }
        }
        fflush(stderr);
        rewind(test_err_file_);
        if(ftruncate(fileno(test_err_file_), 0) != 0){
                return;
        }
        test_err_saved_ = dup(STDERR_FILENO);
        if(test_err_saved_ >= 0){
                dup2(fileno(test_err_file_), STDERR_FILENO);
        }
}

/* End of the expected errors: the number of ERROR lines that were hidden. */
static inline int test_expect_errors_end(void)
{
        char *line = NULL;
        size_t cap = 0;
        int n = 0;
        int level = test_verbosity_();
        if(test_err_saved_ < 0){
                return 0;
        }
        fflush(stderr);
        dup2(test_err_saved_, STDERR_FILENO);
        close(test_err_saved_);
        test_err_saved_ = -1;
        rewind(test_err_file_);
        while(getline(&line, &cap, test_err_file_) > 0){
                int expected = line[0] == '[' && strstr(line, "] :   ERROR : ") != NULL;
                n += expected;
                if(!expected || level >= 2){
                        fputs(line, stderr);
                }
        }
        free(line);
        if(n && level >= 1){
                fprintf(stderr, "   expected  %d ERROR line%s%s\n", n, n == 1 ? "" : "s",
                        level >= 2 ? "" : " hidden (-vv shows them)");
        }
        return n;
}

#endif
