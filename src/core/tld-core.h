/* core: return codes (OK/FAIL), the error-handling macros RUN, ASSERT and ERROR_MSG that
 * jump to a local ERROR label, timestamped log messages and rich terminal output on
 * stderr. No message changes errno, so code can log and then report it. Every other
 * module builds on these. */
#ifndef TLD_CORE_H
#define TLD_CORE_H

/* these headers are used by all bits of the library */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* TLD_API: the linkage of every public tld function and variable, extern (extern "C" in
 * C++). Define it before including tld.h to add attributes (visibility, dllexport). */
#ifndef TLD_API
#ifdef __cplusplus
#define TLD_API extern "C"
#else
#define TLD_API extern
#endif
#endif

/* TLD__STRINGIFY(x) turns the token x into a string literal without expanding it;
 * TLD__TOSTRING(x) expands x first, so TLD__TOSTRING(__LINE__) gives "42". */
#define TLD__STRINGIFY(x) #x
#define TLD__TOSTRING(x) TLD__STRINGIFY(x)

/* Return codes of every tld function that can fail: OK (0) on success, FAIL (1) on
 * failure. Compare with == OK; do not use the result as a boolean. */
#define OK              0
#define FAIL            1

/* Smaller / larger of a and b, with the usual arithmetic conversions (int and double
 * give double). Each argument is evaluated exactly once, so TLD_MAX(i++, n) is safe.
 * A GNU statement expression (gcc, clang): not for constant expressions such as a
 * file-scope array size. */
#define TLD_MIN(a,b)          TLD__MINMAX(<, a, b, __COUNTER__)
#define TLD_MAX(a,b)          TLD__MINMAX(>, a, b, __COUNTER__)
#define TLD__MINMAX(op, a, b, n) TLD__MINMAX_(op, a, b, n)
#define TLD__MINMAX_(op, a, b, n)                                       \
        __extension__({                                                 \
                __typeof__(a) tld__a##n = (a);                          \
                __typeof__(b) tld__b##n = (b);                          \
                tld__a##n op tld__b##n ? tld__a##n : tld__b##n;         \
        })

/* Every message macro checks at compile time that its format is a string literal:
 * "" fmt only compiles for a literal, and the check costs nothing at run time. A
 * compile error at TLD__LITERAL means text was passed as the format, which would
 * read or write random memory if it contained a %: print it with "%s" instead,
 * e.g. LOG_MSG("%s", text). */
#define TLD__LITERAL_FIRST(f, ...) f
#define TLD__LITERAL(...) ((void)sizeof("" TLD__LITERAL_FIRST(__VA_ARGS__, ~)))

/* Message types for tld__message and tld__message_at. */
#define TLD__MSG_LOG 0
#define TLD__MSG_WARNING 1
#define TLD__MSG_ERROR 2

/* Print a timestamped message of type MESSAGE_TYPE_* (printf-style, newline added) to
 * stderr. LOG and WARNING lines are dropped when TLD_VERBOSE=0; errors never are. */
TLD_API void tld__message(uint8_t type, const char *format, ...)
        __attribute__((format(printf, 2, 3)));

/* Like tld__message; an ERROR line also names file and line. Used by the macros. */
TLD_API void tld__message_at(uint8_t type, const char *file, int line, const char *format, ...)
        __attribute__((format(printf, 4, 5)));

/* LOG_MSG(fmt, ...) / WARNING_MSG(fmt, ...): print a timestamped LOG or WARNING line
 * (printf-style, newline added) to stderr and continue. Silent when TLD_VERBOSE=0. */
#define LOG_MSG(...) (TLD__LITERAL(__VA_ARGS__), tld__message(TLD__MSG_LOG, __VA_ARGS__))
#define WARNING_MSG(...) (TLD__LITERAL(__VA_ARGS__), tld__message(TLD__MSG_WARNING, __VA_ARGS__))

/* ERROR_MSG(fmt, ...): print a timestamped ERROR line with file and line to stderr
 * (always, whatever TLD_VERBOSE says) and goto ERROR. */
#define ERROR_MSG(...) do {                                     \
                TLD__LITERAL(__VA_ARGS__);                      \
                tld__message_at(TLD__MSG_ERROR, __FILE__, __LINE__, __VA_ARGS__); \
                goto ERROR;                                     \
        }while (0)

/* The string literal "Function \"x\" failed." with x macro-expanded; used by RUN. */
#define TLD__FAILED(x)  "Function \"" TLD__TOSTRING(x) "\" failed."

/* RUN(EXP): evaluate EXP (a call returning OK/FAIL); if it is not OK, print
 * Function "EXP" failed. as an ERROR line with file and line and goto ERROR. */
#define RUN(EXP) do {                                           \
                if((EXP) != OK){                                \
                        tld__message_at(TLD__MSG_ERROR, __FILE__, __LINE__, "%s", TLD__FAILED(EXP)); \
                        goto ERROR;                             \
                }                                               \
        } while (0)

/* ASSERT(TEST, fmt, ...): if TEST is false, print "Assertion failed: TEST" and the
 * printf-style message as ERROR lines (with file and line) and goto ERROR. The message
 * is required. Active in all builds; the enclosing function needs an ERROR: label. */
#define ASSERT(TEST,...) do {                                   \
                TLD__LITERAL(__VA_ARGS__);                      \
                if(!(TEST)) {                                   \
                        tld__message_at(TLD__MSG_ERROR, __FILE__, __LINE__, "Assertion failed: %s", #TEST); \
                        tld__message_at(TLD__MSG_ERROR, __FILE__, __LINE__, __VA_ARGS__); \
                        goto ERROR;                             \
                }                                               \
        } while (0)

/* The TLD_VERBOSE level: 0, 1 (the default, also for unset or unparsable values) or 2.
 * A single digit is accepted; the variable is read on every call. */
TLD_API int tld_verbosity(void);

/* 1 if stderr gets colour: it is a terminal, NO_COLOR is unset or empty and TERM is
 * not "dumb"; 0 otherwise. */
TLD_API int tld_term_has_color(void);

/* Rich terminal output. Everything goes to stderr, so stdout keeps only a program's
 * results. Colour and symbols are used only when stderr is a terminal and NO_COLOR is
 * not set. TLD_VERBOSE sets how much is shown: 0 errors only, 1 normal (default),
 * 2 also DEBUG_MSG lines.
 *
 *   int n = 40;
 *   SECTION_MSG("Aligning %d sequences", n);   // ── Aligning 40 sequences ────────
 *   KV_MSG("model", "%s", "inkling-2-mini");   //    model     inkling-2-mini
 *   OK_MSG("done in %.1f s", 1.9);             //  ✓ done in 1.9 s
 *   DEBUG_MSG("seed %u", 42u);                 //  · seed 42      (TLD_VERBOSE=2)
 *   for(int i = 0; i < n; i++){
 *           tld_progress(i + 1, n, "aligning");   // aligning [####....]  58% 23/40
 *   }
 */
#define SECTION_MSG(...) (TLD__LITERAL(__VA_ARGS__), tld__section(__VA_ARGS__))
#define KV_MSG(KEY, ...) (TLD__LITERAL(__VA_ARGS__), tld__kv(KEY, __VA_ARGS__))
#define OK_MSG(...) (TLD__LITERAL(__VA_ARGS__), tld__ok(__VA_ARGS__))
#define DEBUG_MSG(...) (TLD__LITERAL(__VA_ARGS__), tld__debug(__VA_ARGS__))

/* The functions behind SECTION_MSG, KV_MSG, OK_MSG and DEBUG_MSG (printf-style, a
 * newline is added). Use the macros. */
TLD_API void tld__section(const char *format, ...) __attribute__((format(printf, 1, 2)));
TLD_API void tld__kv(const char *key, const char *format, ...)
        __attribute__((format(printf, 2, 3)));
TLD_API void tld__ok(const char *format, ...) __attribute__((format(printf, 1, 2)));
TLD_API void tld__debug(const char *format, ...) __attribute__((format(printf, 1, 2)));

/* Progress of a loop: call with done = 1..total. On a terminal one line is redrawn
 * (only when the percentage changes); otherwise only the final line is printed. Not
 * meant to be called from several threads at once. */
TLD_API void tld_progress(long done, long total, const char *label);

#endif
