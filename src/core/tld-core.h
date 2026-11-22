/* core: return codes (OK/FAIL), the error-handling macros RUN, ASSERT and ERROR_MSG that
 * jump to a local ERROR label, timestamped log messages and rich terminal output on
 * stderr. Every other module builds on these. */
#ifndef TLD_CORE_H
#define TLD_CORE_H

/* these headers are used by all bits of the library */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* Linkage of every public tld function: extern, or extern "C" in C++. Define it before
 * including tld.h to override. */
#ifndef tld_external
#ifdef __cplusplus
#define tld_external extern "C"
#else
#define tld_external extern
#endif
#endif

/* Marks functions private to one implementation file (static). Define it before
 * including tld.h to override. */
#ifndef tld_internal
#define tld_internal static
#endif

/* Define generally useful macros  */

/* tld_stringify(x) turns the token x into a string literal without expanding it;
 * tld_tostring(x) expands x first, so tld_tostring(__LINE__) gives "42". */
#define tld_stringify(x) #x
#define tld_tostring(x) tld_stringify(x)


/* Return codes of every tld function that can fail: OK (0) on success, FAIL (1) on
 * failure. Compare with == OK; do not use the result as a boolean. */
#define OK              0
#define FAIL            1

/* Smaller / larger of a and b. Arguments are evaluated twice: no side effects. */
#define MACRO_MIN(a,b)          (((a)<(b))?(a):(b))
#define MACRO_MAX(a,b)          (((a)>(b))?(a):(b))



/* ASSERT(TEST, fmt, ...): if TEST is false, print "Assertion failed: TEST" and the
 * printf-style message as ERROR lines (with file and line) and goto ERROR. The message
 * is required. Active in all builds; the enclosing function needs an ERROR: label. */
#define ASSERT(TEST,...) do {                                   \
                if(!(TEST)) {                                   \
                        tld_message_at(MESSAGE_TYPE_ERROR, __FILE__, __LINE__, "Assertion failed: %s", #TEST); \
                        tld_message_at(MESSAGE_TYPE_ERROR, __FILE__, __LINE__, __VA_ARGS__); \
                        goto ERROR;                             \
                }                                               \
        } while (0)

/* Message types for tld_message and tld_message_at. */
#define MESSAGE_TYPE_MSG 0
#define MESSAGE_TYPE_WARNING 1
#define MESSAGE_TYPE_ERROR 2


/* LOG_MSG(fmt, ...) / WARNING_MSG(fmt, ...): print a timestamped LOG or WARNING line
 * (printf-style, newline added) to stderr and continue. Silent when TLD_VERBOSE=0. */
#define LOG_MSG(...) (tld_message(MESSAGE_TYPE_MSG, __VA_ARGS__ ))
#define WARNING_MSG(...) (tld_message(MESSAGE_TYPE_WARNING, __VA_ARGS__ ))

/* ERROR_MSG(fmt, ...): print a timestamped ERROR line with file and line to stderr
 * (always, whatever TLD_VERBOSE says) and goto ERROR. */
#define ERROR_MSG(...) do {                                     \
                tld_message_at(MESSAGE_TYPE_ERROR, __FILE__, __LINE__, __VA_ARGS__); \
                goto ERROR;                                     \
        }while (0)


/* The string literal "Function \"x\" failed." with x macro-expanded; used by RUN. */
#define ADDFAILED(x)  "Function \"" tld_tostring(x) "\" failed."

/* RUN(EXP): evaluate EXP (a call returning OK/FAIL); if it is not OK, print
 * Function "EXP" failed. as an ERROR line with file and line and goto ERROR. */
#define RUN(EXP) do {                                           \
                if((EXP) != OK){                                \
                        tld_message_at(MESSAGE_TYPE_ERROR, __FILE__, __LINE__, "%s", ADDFAILED(EXP)); \
                        goto ERROR;                             \
                }                                               \
        } while (0)


/* Print a timestamped message of type MESSAGE_TYPE_* (printf-style, newline added) to
 * stderr. LOG and WARNING lines are dropped when TLD_VERBOSE=0; errors never are. */
tld_external void tld_message(uint8_t type, const char *format, ...);

/* Like tld_message; an ERROR line also names file and line. Used by the macros. */
tld_external void tld_message_at(uint8_t type, const char *file, int line, const char *format, ...)
        __attribute__((format(printf, 4, 5)));

/* Rich terminal output. Everything goes to stderr, so stdout keeps only a program's
 * results. Colour and symbols are used only when stderr is a terminal and NO_COLOR is
 * not set. TLD_VERBOSE sets how much is shown: 0 errors only, 1 normal (default),
 * 2 also DEBUG_MSG lines.
 *
 *   SECTION_MSG("Aligning %d sequences", n);   ── Aligning 40 sequences ──────────
 *   KV_MSG("model", "%s", name);                  model     inkling-2-mini:latest
 *   OK_MSG("done in %.1f s", t);                ✓ done in 1.9 s
 *   DEBUG_MSG("seed %u", seed);                 · seed 42          (TLD_VERBOSE=2)
 *   tld_progress(i + 1, n, "aligning");        aligning [#######.....]  58% 23/40
 */
#define SECTION_MSG(...) tld_section(__VA_ARGS__)
#define KV_MSG(KEY, ...) tld_kv(KEY, __VA_ARGS__)
#define OK_MSG(...) tld_ok(__VA_ARGS__)
#define DEBUG_MSG(...) tld_debug(__VA_ARGS__)

/* The TLD_VERBOSE level: 0, 1 (the default, also for unset or unparsable values) or 2.
 * A single digit is accepted; the variable is read on every call. */
tld_external int tld_verbosity(void);

/* 1 if stderr gets colour: it is a terminal, NO_COLOR is unset or empty and TERM is
 * not "dumb"; 0 otherwise. */
tld_external int tld_term_color(void);

/* The functions behind SECTION_MSG, KV_MSG, OK_MSG and DEBUG_MSG (printf-style, a
 * newline is added). Use the macros. */
tld_external void tld_section(const char *format, ...) __attribute__((format(printf, 1, 2)));
tld_external void tld_kv(const char *key, const char *format, ...)
        __attribute__((format(printf, 2, 3)));
tld_external void tld_ok(const char *format, ...) __attribute__((format(printf, 1, 2)));
tld_external void tld_debug(const char *format, ...) __attribute__((format(printf, 1, 2)));

/* Progress of a loop: call with done = 1..total. On a terminal one line is redrawn
 * (only when the percentage changes); otherwise only the final line is printed. Not
 * meant to be called from several threads at once. */
tld_external void tld_progress(long done, long total, const char *label);

#endif
