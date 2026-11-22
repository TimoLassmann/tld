#ifndef TLD_CORE_H
#define TLD_CORE_H

/* these headers are used by all bits of the library */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef tld_external
#ifdef __cplusplus
#define tld_external extern "C"
#else
#define tld_external extern
#endif
#endif

#ifndef tld_internal
#define tld_internal static
#endif

/* Define generally useful macros  */

#define tld_stringify(x) #x
#define tld_tostring(x) tld_stringify(x)


#define OK              0
#define FAIL            1

/* Need to revisit  */
#define MACRO_MIN(a,b)          (((a)<(b))?(a):(b))
#define MACRO_MAX(a,b)          (((a)>(b))?(a):(b))



#define ASSERT(TEST,...) do {                                   \
                if(!(TEST)) {                                   \
                        tld_message_at(MESSAGE_TYPE_ERROR, __FILE__, __LINE__, "Assertion failed: %s", #TEST); \
                        tld_message_at(MESSAGE_TYPE_ERROR, __FILE__, __LINE__, __VA_ARGS__); \
                        goto ERROR;                             \
                }                                               \
        } while (0)

/* Messages  */
#define MESSAGE_TYPE_MSG 0
#define MESSAGE_TYPE_WARNING 1
#define MESSAGE_TYPE_ERROR 2


#define LOG_MSG(...) (tld_message(MESSAGE_TYPE_MSG, __VA_ARGS__ ))
#define WARNING_MSG(...) (tld_message(MESSAGE_TYPE_WARNING, __VA_ARGS__ ))
#define ERROR_MSG(...) do {                                     \
                tld_message_at(MESSAGE_TYPE_ERROR, __FILE__, __LINE__, __VA_ARGS__); \
                goto ERROR;                                     \
        }while (0)


/* Checking for function return  */
#define ADDFAILED(x)  "Function \"" tld_tostring(x) "\" failed."
#define RUN(EXP) do {                                           \
                if((EXP) != OK){                                \
                        tld_message_at(MESSAGE_TYPE_ERROR, __FILE__, __LINE__, "%s", ADDFAILED(EXP)); \
                        goto ERROR;                             \
                }                                               \
        } while (0)


tld_external void tld_message(uint8_t type, const char *format, ...);
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

tld_external int tld_verbosity(void);       /* TLD_VERBOSE: 0, 1 (default) or 2 */
tld_external int tld_term_color(void);      /* 1 if stderr gets colour */
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
