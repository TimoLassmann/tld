/* tld_doc.h - live documentation for tld.h and tld_ai.h: a C program asks in plain
 * language ("how do I read a JSON string?") and gets the manual entries whose words match
 * the question, ranked, copied word for word. A deterministic lookup, nothing more: the
 * same question always gives the same answer, no model is involved, and the answer says
 * which of the question's words occur in the manuals and which do not, so the reader can
 * judge. Single header, builds on tld.h.
 *
 *   tld_doc_answer *a = NULL;
 *   RUN(tld_doc_ask("how do I read a JSON string?", NULL, &a));
 *   fputs(a->text, stdout);
 *   tld_doc_answer_free(a);
 *
 * Manual: docs/tld_doc.md, generated from tld_doc.h by tools/tlddoc.c.
 *
 * USAGE
 *   In exactly one .c file, before any other #include:
 *
 *       #define TLD_IMPLEMENTATION      // unless another file already compiles tld.h
 *       #define TLD_DOC_IMPLEMENTATION
 *       #include "tld_doc.h"            // includes "tld.h"
 *
 *   and plain #include "tld_doc.h" everywhere else. Build: cc -std=c11 prog.c -lpthread
 *   -lm. tld_doc.h does not need tld_ai.h or an Ollama server; its corpus covers the
 *   manuals of both tld.h and tld_ai.h.
 *
 * HOW A QUESTION IS ANSWERED
 *   A word search (tld_bpm, tld.h) over the corpus, one record per manual entry. The
 *   question's words (common English words dropped, plurals and endings such as -ing
 *   taken off) are matched where words of the manual start. Records are ranked by BM25
 *   (the standard ranking formula, with its usual parameters), a match in a name counting
 *   three times; an identifier ("tld_json_get_str", or with a typo "tld_jsn_get_str")
 *   puts its entry first. A word written exactly as a name of the manuals, case included
 *   ("OK", "RUN", "MMALLOC"), is an identifier too: it is searched even when shorter than
 *   3 letters and puts its entry first, so "OK parse json" ranks OK, FAIL first and "ok
 *   parse json" does not. A word of 6 or more letters found nowhere is searched again
 *   allowing an edit or two. The full rules are in the implementation.
 *
 * THE CORPUS
 *   One record per entry (### heading) of docs/tld.md and docs/tld_ai.md, holding the
 *   text the manual shows: the names, the header, the module and its one-line
 *   description, the declaration, the doc text and the code examples. It is generated
 *   into this file by tools/tlddoc.c, by the same command that writes the manuals
 *   (cmake --build build --target docs), and the ctest docs-current fails while it is
 *   out of date. It is static const data: nothing is allocated to use it.
 *
 * ERRORS
 *   As in tld.h: functions return OK or FAIL (or NULL) with an ERROR line on stderr.
 *
 * LICENCE: MIT, see the end of tld_doc.h.
 */

#ifndef TLD_DOC_H
#define TLD_DOC_H

#include "tld.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Version of tld_doc.h. */
#define TLD_DOC_VERSION "0.1.0"

/* ---- the corpus ---------------------------------------------------------- */

/* The kinds of a corpus record (tld_doc_entry.kind): an entry of a manual, or one of its
 * general sections. */
#define TLD_DOC_ENTRY 0
#define TLD_DOC_SECTION 1

/* One corpus record: one entry of the tld manuals, in static storage. names lists every
 * name under the entry's heading, in order, separated by ", "; header is "tld.h" or
 * "tld_ai.h"; module is the module (tld.h) or section (tld_ai.h) name and module_desc
 * its one-line description ("" when it has none); decl is the declaration block as the
 * manual shows it; doc is the doc text without its code examples, and example the code
 * examples (blocks separated by a blank line), NULL when the entry has none.
 *
 * A record of kind TLD_DOC_SECTION is a general section of a manual instead (Usage,
 * Errors, Memory, ...: the rules that hold for every function). Its names is the header
 * and the title, "tld.h: Errors"; module, module_desc and decl are ""; doc and example
 * are the section's text and code. */
typedef struct {
        const char *names;      /* "tld_json_parse, tld_json_parse_n" */
        const char *header;     /* "tld.h" or "tld_ai.h" */
        const char *module;     /* "json" */
        const char *module_desc;
        const char *decl;
        const char *doc;
        const char *example;    /* NULL when the entry has none */
        int kind;               /* TLD_DOC_ENTRY or TLD_DOC_SECTION */
} tld_doc_entry;

/* The number of records in the corpus. */
int tld_doc_n_entries(void);

/* Record i of the corpus, 0 <= i < tld_doc_n_entries(): the entries in manual order
 * (tld.h, then tld_ai.h), then the general sections in the same order; NULL with an
 * ERROR line when i is outside that range. */
const tld_doc_entry *tld_doc_entry_at(int i);

/* The corpus identifier, "HHHHHHHHHHHHHHHH-V": 16 lower-case hex digits of the 64-bit
 * FNV-1a hash of the records, then the AI_VERSION of the tld_ai.h it was generated from.
 * The hash runs over the records in order and, in each, over names, header, module,
 * module_desc, decl, doc and example: each field's bytes followed by a 0 byte, a NULL
 * field as the byte 1 followed by a 0 byte. Static storage. */
const char *tld_doc_corpus_id(void);

/* ---- asking -------------------------------------------------------------- */

/* One retrieved entry of an answer: the record and its score (BM25, plus 1000 for an
 * exact name match and 500 for a close one, so those come first). */
typedef struct {
        const tld_doc_entry *entry;
        double score;
} tld_doc_hit;

/* The answer to a question, from tld_doc_ask; free it with tld_doc_answer_free. hits are
 * the retrieved entries, best first (at most top_n, those that match at least one word);
 * text is the answer written for a coding agent (see tld_doc_ask). */
typedef struct {
        tld_doc_hit *hits;      /* retrieved entries, best first */
        int n_hits;
        char *text;             /* the answer for a coding agent */
} tld_doc_answer;

/* Options for tld_doc_ask; a field left 0 takes its default. */
typedef struct {
        int max_bytes;          /* budget for text; 0: 4096 */
        int top_n;              /* entries retrieved; 0: 8 */
} tld_doc_opts;

/* Answer question into *out (o may be NULL: all defaults). Returns FAIL only for bad
 * arguments (question or out NULL, max_bytes or top_n negative) or out of memory. At
 * most the first 16 distinct words of the question are searched; the head names the
 * words left out ("Not searched: ...").
 *
 * The text starts with a head: "tld_doc answer", "Question: " and the question on one
 * line (control bytes as spaces, bytes that are not UTF-8 as '?', cut after 200 bytes
 * with " ..."), then the facts of the search: "Words found in the manuals: ..." and
 * "Not found: ..." (with the nearest form of a word that was searched again allowing
 * edits), or that the question has no words to search for, or that no entry matches.
 * When entries follow, a line says they are ranked by word match and that everything
 * under ENTRY or SECTION is copied exactly from the manuals. Then, after a blank line
 * each, the entries, best first:
 *
 *     ENTRY 1: tld_json_parse, tld_json_parse_n
 *     Header: tld.h, module json (the module's description)
 *     Matched: words read, json, string        (or: name "x"; close to name "x"; near y)
 *     Declaration:
 *         (the declaration block, whole)
 *     Documentation:
 *         (the doc text)
 *     Example: none in the manual.             (or "Example:" and the code)
 *     See also: tld_json_obj_free              (names its doc text mentions)
 *
 * A general section (kind TLD_DOC_SECTION) is ranked with the entries and shown as
 * "SECTION 2: tld.h: Errors", without the Header, Declaration and See also lines.
 *
 * Everything indented under Declaration, Documentation and Example is copied from the
 * manual. The text is at most max_bytes bytes, except that the head always appears
 * whole (at most 2300 bytes, under 300 for a short question) and so does, when entries
 * are not shown, the line "Not shown (byte budget): ... and N more" (at most 43 bytes).
 * Entries come in rank order, each as complete as fits: a declaration is never cut; the
 * doc text is whole, or cut after its last sentence that fits and " ...", or "not shown
 * (byte budget)"; the example only with the whole doc text. The first entry whose
 * declaration does not fit, and every entry after it, is listed by name under "Not shown
 * (byte budget)", with as many names as fit and "... and N more". */
int tld_doc_ask(const char *question, const tld_doc_opts *o, tld_doc_answer **out);

/* Free an answer from tld_doc_ask; NULL is ignored. */
void tld_doc_answer_free(tld_doc_answer *a);

#ifdef __cplusplus
}
#endif

#endif /* TLD_DOC_H */

#ifdef TLD_DOC_IMPLEMENTATION
#ifndef TLD__DOC_IMPLEMENTED
#define TLD__DOC_IMPLEMENTED

/* ---- BEGIN CORPUS: generated by tools/tlddoc.c from tld.h and tld_ai.h; do not edit ---- */
#define TLD__DOC_N_ENTRIES 187
#define TLD__DOC_CORPUS_ID "3aead75db756d291-0.4.0"

static const tld_doc_entry tld__doc_corpus[TLD__DOC_N_ENTRIES] = {
        {       /* 0: tld.h, core */
                "TLD_API",
                "tld.h",
                "core",
                "Return codes (OK/FAIL), the error-handling macros RUN, ASSERT and ERROR_MSG that jump to a local ERROR label, timestamped log messages and rich terminal output on stderr. No message changes errno, so code can log and then report it. Every other module builds on these.",
                "#ifndef TLD_API\n"
                "#ifdef __cplusplus\n"
                "#define TLD_API extern \"C\"\n"
                "#else\n"
                "#define TLD_API extern\n"
                "#endif\n"
                "#endif",
                "TLD_API: the linkage of every public tld function and variable, extern (extern \"C\" in\n"
                "C++). Define it before including tld.h to add attributes (visibility, dllexport).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 1: tld.h, core */
                "OK, FAIL",
                "tld.h",
                "core",
                "Return codes (OK/FAIL), the error-handling macros RUN, ASSERT and ERROR_MSG that jump to a local ERROR label, timestamped log messages and rich terminal output on stderr. No message changes errno, so code can log and then report it. Every other module builds on these.",
                "#define OK              0\n"
                "#define FAIL            1",
                "Return codes of every tld function that can fail: OK (0) on success, FAIL (1) on\n"
                "failure. Compare with == OK; do not use the result as a boolean.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 2: tld.h, core */
                "TLD_MIN, TLD_MAX",
                "tld.h",
                "core",
                "Return codes (OK/FAIL), the error-handling macros RUN, ASSERT and ERROR_MSG that jump to a local ERROR label, timestamped log messages and rich terminal output on stderr. No message changes errno, so code can log and then report it. Every other module builds on these.",
                "#define TLD_MIN(a,b)          TLD__MINMAX(<, a, b, __COUNTER__)\n"
                "#define TLD_MAX(a,b)          TLD__MINMAX(>, a, b, __COUNTER__)",
                "Smaller / larger of a and b, with the usual arithmetic conversions (int and double\n"
                "give double). Each argument is evaluated exactly once, so TLD_MAX(i++, n) is safe.\n"
                "A GNU statement expression (gcc, clang): not for constant expressions such as a\n"
                "file-scope array size.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 3: tld.h, core */
                "LOG_MSG, WARNING_MSG",
                "tld.h",
                "core",
                "Return codes (OK/FAIL), the error-handling macros RUN, ASSERT and ERROR_MSG that jump to a local ERROR label, timestamped log messages and rich terminal output on stderr. No message changes errno, so code can log and then report it. Every other module builds on these.",
                "#define LOG_MSG(...) (TLD__LITERAL(__VA_ARGS__), tld__message(TLD__MSG_LOG, __VA_ARGS__))\n"
                "#define WARNING_MSG(...) (TLD__LITERAL(__VA_ARGS__), tld__message(TLD__MSG_WARNING, __VA_ARGS__))",
                "LOG_MSG(fmt, ...) / WARNING_MSG(fmt, ...): print a timestamped LOG or WARNING line\n"
                "(printf-style, newline added) to stderr and continue. Silent when TLD_VERBOSE=0.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 4: tld.h, core */
                "ERROR_MSG",
                "tld.h",
                "core",
                "Return codes (OK/FAIL), the error-handling macros RUN, ASSERT and ERROR_MSG that jump to a local ERROR label, timestamped log messages and rich terminal output on stderr. No message changes errno, so code can log and then report it. Every other module builds on these.",
                "#define ERROR_MSG(...) do {                                     \\\n"
                "                TLD__LITERAL(__VA_ARGS__);                      \\\n"
                "                tld__message_at(TLD__MSG_ERROR, __FILE__, __LINE__, __VA_ARGS__); \\\n"
                "                goto ERROR;                                     \\\n"
                "        }while (0)",
                "ERROR_MSG(fmt, ...): print a timestamped ERROR line with file and line to stderr\n"
                "(always, whatever TLD_VERBOSE says) and goto ERROR. The enclosing function needs an\n"
                "ERROR: label.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 5: tld.h, core */
                "RUN",
                "tld.h",
                "core",
                "Return codes (OK/FAIL), the error-handling macros RUN, ASSERT and ERROR_MSG that jump to a local ERROR label, timestamped log messages and rich terminal output on stderr. No message changes errno, so code can log and then report it. Every other module builds on these.",
                "#define RUN(EXP) do {                                           \\\n"
                "                if((EXP) != OK){                                \\\n"
                "                        tld__message_at(TLD__MSG_ERROR, __FILE__, __LINE__, \"%s\", TLD__FAILED(EXP)); \\\n"
                "                        goto ERROR;                             \\\n"
                "                }                                               \\\n"
                "        } while (0)",
                "RUN(EXP): evaluate EXP (a call returning OK/FAIL); if it is not OK, print\n"
                "Function \"EXP\" failed. as an ERROR line with file and line and goto ERROR. The\n"
                "enclosing function needs an ERROR: label.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 6: tld.h, core */
                "ASSERT",
                "tld.h",
                "core",
                "Return codes (OK/FAIL), the error-handling macros RUN, ASSERT and ERROR_MSG that jump to a local ERROR label, timestamped log messages and rich terminal output on stderr. No message changes errno, so code can log and then report it. Every other module builds on these.",
                "#define ASSERT(TEST,...) ...",
                "ASSERT(TEST, fmt, ...): if TEST is false, print \"Assertion failed: TEST\" and the\n"
                "printf-style message as ERROR lines (with file and line) and goto ERROR. The message\n"
                "is required. Active in all builds; the enclosing function needs an ERROR: label.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 7: tld.h, core */
                "tld_verbosity",
                "tld.h",
                "core",
                "Return codes (OK/FAIL), the error-handling macros RUN, ASSERT and ERROR_MSG that jump to a local ERROR label, timestamped log messages and rich terminal output on stderr. No message changes errno, so code can log and then report it. Every other module builds on these.",
                "int tld_verbosity(void);",
                "The TLD_VERBOSE level: 0, 1 (the default, also for unset or unparsable values) or 2.\n"
                "A single digit is accepted; the variable is read on every call.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 8: tld.h, core */
                "tld_term_has_color",
                "tld.h",
                "core",
                "Return codes (OK/FAIL), the error-handling macros RUN, ASSERT and ERROR_MSG that jump to a local ERROR label, timestamped log messages and rich terminal output on stderr. No message changes errno, so code can log and then report it. Every other module builds on these.",
                "int tld_term_has_color(void);",
                "1 if stderr gets colour: it is a terminal, NO_COLOR is unset or empty and TERM is\n"
                "not \"dumb\"; 0 otherwise.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 9: tld.h, core */
                "SECTION_MSG, KV_MSG, OK_MSG, DEBUG_MSG",
                "tld.h",
                "core",
                "Return codes (OK/FAIL), the error-handling macros RUN, ASSERT and ERROR_MSG that jump to a local ERROR label, timestamped log messages and rich terminal output on stderr. No message changes errno, so code can log and then report it. Every other module builds on these.",
                "#define SECTION_MSG(...) (TLD__LITERAL(__VA_ARGS__), tld__section(__VA_ARGS__))\n"
                "#define KV_MSG(KEY, ...) (TLD__LITERAL(__VA_ARGS__), tld__kv(KEY, __VA_ARGS__))\n"
                "#define OK_MSG(...) (TLD__LITERAL(__VA_ARGS__), tld__ok(__VA_ARGS__))\n"
                "#define DEBUG_MSG(...) (TLD__LITERAL(__VA_ARGS__), tld__debug(__VA_ARGS__))",
                "Rich terminal output. Everything goes to stderr, so stdout keeps only a program's\n"
                "results. Colour and symbols are used only when stderr is a terminal and NO_COLOR is\n"
                "not set. TLD_VERBOSE sets how much is shown: 0 errors only, 1 normal (default),\n"
                "2 also DEBUG_MSG lines.",
                "int n = 40;\n"
                "SECTION_MSG(\"Aligning %d sequences\", n);   // \342\224\200\342\224\200 Aligning 40 sequences \342\224\200\342\224\200\342\224\200\342\224\200\342\224\200\342\224\200\342\224\200\342\224\200\n"
                "KV_MSG(\"model\", \"%s\", \"inkling-2-mini\");   //    model     inkling-2-mini\n"
                "OK_MSG(\"done in %.1f s\", 1.9);             //  \342\234\223 done in 1.9 s\n"
                "DEBUG_MSG(\"seed %u\", 42u);                 //  \302\267 seed 42      (TLD_VERBOSE=2)\n"
                "for(int i = 0; i < n; i++){\n"
                "        tld_progress(i + 1, n, \"aligning\");   // aligning [####....]  58% 23/40\n"
                "}",
                TLD_DOC_ENTRY
        },
        {       /* 10: tld.h, core */
                "tld_progress",
                "tld.h",
                "core",
                "Return codes (OK/FAIL), the error-handling macros RUN, ASSERT and ERROR_MSG that jump to a local ERROR label, timestamped log messages and rich terminal output on stderr. No message changes errno, so code can log and then report it. Every other module builds on these.",
                "void tld_progress(long done, long total, const char *label);",
                "Progress of a loop: call with done = 1..total. On a terminal one line is redrawn\n"
                "(only when the percentage changes); otherwise only the final line is printed. Not\n"
                "meant to be called from several threads at once.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 11: tld.h, alloc */
                "MMALLOC",
                "tld.h",
                "alloc",
                "Checked allocation macros (MMALLOC, MREALLOC, MFREE) and galloc/gfree, which allocate zeroed, typed 1D arrays and 2D matrices that remember their dimensions. All allocation macros goto ERROR on failure.",
                "#define MMALLOC(p, size) ...",
                "MMALLOC(p, size): p = malloc(size), zero-filled. p must be NULL beforehand and size\n"
                "non-zero; either mistake, or running out of memory, prints an error and does goto\n"
                "ERROR (the enclosing function needs an ERROR: label). Free with MFREE.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 12: tld.h, alloc */
                "MMALLOC_ALIGNED",
                "tld.h",
                "alloc",
                "Checked allocation macros (MMALLOC, MREALLOC, MFREE) and galloc/gfree, which allocate zeroed, typed 1D arrays and 2D matrices that remember their dimensions. All allocation macros goto ERROR on failure.",
                "#define MMALLOC_ALIGNED(p, size, align) ...",
                "MMALLOC_ALIGNED: like MMALLOC, for a struct whose declaration asks for more alignment\n"
                "than malloc guarantees (16 bytes on 64-bit), e.g. __attribute__((aligned(64))). Using\n"
                "plain malloc for such a struct breaks the promise the declaration makes to the compiler\n"
                "(undefined behaviour; UBSan reports \"misaligned address ... requires 64 byte alignment\").\n"
                "Pass the struct's own alignment: MMALLOC_ALIGNED(p, sizeof(struct s), _Alignof(struct s)).\n"
                "C11 aligned_alloc requires the size to be a multiple of the alignment, so the size is\n"
                "rounded up. The block is freed with plain free(), so MFREE works unchanged. On failure\n"
                "an error is printed and goto ERROR; the enclosing function needs an ERROR: label.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 13: tld.h, alloc */
                "MREALLOC",
                "tld.h",
                "alloc",
                "Checked allocation macros (MMALLOC, MREALLOC, MFREE) and galloc/gfree, which allocate zeroed, typed 1D arrays and 2D matrices that remember their dimensions. All allocation macros goto ERROR on failure.",
                "#define MREALLOC(p, size) ...",
                "MREALLOC(p, size): resize p to size bytes (malloc when p is NULL). Bytes beyond the old\n"
                "size are not zeroed. On failure p is unchanged, an error is printed and goto ERROR;\n"
                "size 0 is an error. The enclosing function needs an ERROR: label.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 14: tld.h, alloc */
                "MFREE",
                "tld.h",
                "alloc",
                "Checked allocation macros (MMALLOC, MREALLOC, MFREE) and galloc/gfree, which allocate zeroed, typed 1D arrays and 2D matrices that remember their dimensions. All allocation macros goto ERROR on failure.",
                "#define MFREE(p)                                                        \\\n"
                "        do {                                                            \\\n"
                "                free(p);                                                \\\n"
                "                (p) = NULL;                                             \\\n"
                "        } while (0)",
                "MFREE(p): free(p) and set p to NULL. For memory from MMALLOC, MREALLOC or malloc (use\n"
                "gfree for galloc memory). p must be an lvalue; a NULL p is fine.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 15: tld.h, alloc */
                "galloc",
                "tld.h",
                "alloc",
                "Checked allocation macros (MMALLOC, MREALLOC, MFREE) and galloc/gfree, which allocate zeroed, typed 1D arrays and 2D matrices that remember their dimensions. All allocation macros goto ERROR on failure.",
                "#define galloc(...) TLD__GALLOC_ARGS(__VA_ARGS__)",
                "galloc(&a, n): a = zeroed array of n elements of a's element type.\n"
                "galloc(&m, n, k): m = zeroed n x k matrix, used as m[i][j]; the rows share one\n"
                "contiguous block. Any element type (numbers, pointers, structs) whose alignment malloc\n"
                "provides; a more strictly aligned type is a compile error. The pointer should be NULL (an existing block is not freed or resized). On\n"
                "failure an error is printed and goto ERROR. Free with gfree; tld_galloc_dim1/tld_galloc_dim2 give\n"
                "the dimensions back. The enclosing function needs an ERROR: label.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 16: tld.h, alloc */
                "gfree",
                "tld.h",
                "alloc",
                "Checked allocation macros (MMALLOC, MREALLOC, MFREE) and galloc/gfree, which allocate zeroed, typed 1D arrays and 2D matrices that remember their dimensions. All allocation macros goto ERROR on failure.",
                "#define gfree(a)                                                        \\\n"
                "        do {                                                            \\\n"
                "                tld__gfree(a);                                            \\\n"
                "                (a) = NULL;                                             \\\n"
                "        } while (0)",
                "gfree(p): free memory from galloc (1D or 2D) and set p to NULL, like MFREE, so a\n"
                "second gfree(p) on an error path is harmless. p must be an lvalue (a variable, field\n"
                "or element); a NULL p is fine. Never pass galloc memory to free() or MFREE.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 17: tld.h, alloc */
                "tld_galloc_dim1, tld_galloc_dim2",
                "tld.h",
                "alloc",
                "Checked allocation macros (MMALLOC, MREALLOC, MFREE) and galloc/gfree, which allocate zeroed, typed 1D arrays and 2D matrices that remember their dimensions. All allocation macros goto ERROR on failure.",
                "int tld_galloc_dim1(void *ptr, uint32_t *d);\n"
                "int tld_galloc_dim2(void *ptr, uint32_t *d);",
                "Dimensions of a galloc block: rows (n) into *d, and columns (k, 0 for a 1D array)\n"
                "into *d. FAIL if ptr is NULL. Only valid for memory from galloc.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 18: tld.h, string */
                "tld_strbuf",
                "tld.h",
                "string",
                "tld_strbuf, a growable NUL-terminated byte buffer (append, prepend, copy). Inspired by the rayfork str code by Sas Luca.",
                "typedef struct tld_strbuf {\n"
                "        uint8_t* str;\n"
                "        int32_t len;\n"
                "        int32_t alloc_len;\n"
                "} tld_strbuf;",
                "A growable byte buffer: len bytes at str, always followed by a NUL, in alloc_len\n"
                "bytes. TLD_STR(b) gives it as char *. Appending may move str, so do not keep that\n"
                "pointer across changes. Create with tld_strbuf_alloc, free with tld_strbuf_free.\n"
                "A buffer holds at most INT32_MAX - 1 bytes; an append that would pass that fails\n"
                "with an error and leaves the buffer unchanged.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 19: tld.h, string */
                "TLD_STR, TLD_STRLEN",
                "tld.h",
                "string",
                "tld_strbuf, a growable NUL-terminated byte buffer (append, prepend, copy). Inspired by the rayfork str code by Sas Luca.",
                "#define TLD_STR(X) ((char*)(X)->str)\n"
                "#define TLD_STRLEN(X) (X)->len",
                "TLD_STR(b): the contents of the tld_strbuf * b as char *; TLD_STRLEN(b): its length in\n"
                "bytes.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 20: tld.h, string */
                "tld_strbuf_alloc",
                "tld.h",
                "string",
                "tld_strbuf, a growable NUL-terminated byte buffer (append, prepend, copy). Inspired by the rayfork str code by Sas Luca.",
                "int tld_strbuf_alloc(tld_strbuf **buffer, int size);",
                "A new empty buffer in *buffer with size bytes reserved (0 means 1024; size < 0 is an\n"
                "error). It grows as needed. Free with tld_strbuf_free.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 21: tld.h, string */
                "tld_strbuf_free",
                "tld.h",
                "string",
                "tld_strbuf, a growable NUL-terminated byte buffer (append, prepend, copy). Inspired by the rayfork str code by Sas Luca.",
                "void tld_strbuf_free(tld_strbuf *b);",
                "Free a buffer; NULL is ignored.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 22: tld.h, string */
                "tld_strbuf_clear",
                "tld.h",
                "string",
                "tld_strbuf, a growable NUL-terminated byte buffer (append, prepend, copy). Inspired by the rayfork str code by Sas Luca.",
                "int tld_strbuf_clear(tld_strbuf *b);",
                "Empty the buffer (len 0); the memory is kept.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 23: tld.h, string */
                "tld_strbuf_copy",
                "tld.h",
                "string",
                "tld_strbuf, a growable NUL-terminated byte buffer (append, prepend, copy). Inspired by the rayfork str code by Sas Luca.",
                "int tld_strbuf_copy(tld_strbuf *t, tld_strbuf *s);",
                "Replace the contents of t with a copy of s; t grows as needed.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 24: tld.h, string */
                "tld_strbuf_append",
                "tld.h",
                "string",
                "tld_strbuf, a growable NUL-terminated byte buffer (append, prepend, copy). Inspired by the rayfork str code by Sas Luca.",
                "int tld_strbuf_append(tld_strbuf *b, const char *content);",
                "Append the NUL-terminated string content to b; b grows as needed.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 25: tld.h, string */
                "tld_strbuf_append_n",
                "tld.h",
                "string",
                "tld_strbuf, a growable NUL-terminated byte buffer (append, prepend, copy). Inspired by the rayfork str code by Sas Luca.",
                "int tld_strbuf_append_n(tld_strbuf *b, const char *s, size_t n);",
                "Append the n bytes at s to b (they may include NUL bytes; the buffer is still\n"
                "NUL-terminated after them). s may point into b, and may be NULL when n is 0.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 26: tld.h, string */
                "tld_strbuf_append_char",
                "tld.h",
                "string",
                "tld_strbuf, a growable NUL-terminated byte buffer (append, prepend, copy). Inspired by the rayfork str code by Sas Luca.",
                "int tld_strbuf_append_char(tld_strbuf *b, char c);",
                "Append the single character c to b.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 27: tld.h, string */
                "tld_strbuf_prepend",
                "tld.h",
                "string",
                "tld_strbuf, a growable NUL-terminated byte buffer (append, prepend, copy). Inspired by the rayfork str code by Sas Luca.",
                "int tld_strbuf_prepend(tld_strbuf *b, const char *content);",
                "Insert the NUL-terminated string content at the start of b.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 28: tld.h, path */
                "tld_path_exists, tld_path_is_dir",
                "tld.h",
                "path",
                "File and directory checks, absolute paths, file and directory names of a path, and capturing the command line.",
                "int tld_path_exists(const char *path);\n"
                "int tld_path_is_dir(const char *path);",
                "Predicates: 1 or 0, never a message (a NULL path prints an error and gives 0).\n"
                "tld_path_exists: 1 if path exists (a file, directory or anything else stat() sees).\n"
                "tld_path_is_dir: 1 if path is a directory (following symlinks).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 29: tld.h, path */
                "tld_path_mkdir",
                "tld.h",
                "path",
                "File and directory checks, absolute paths, file and directory names of a path, and capturing the command line.",
                "int tld_path_mkdir(const char *dir);",
                "Create the directory dir (mode 0700; its parent must exist). OK when it was created\n"
                "or already exists as a directory (silently); FAIL with an error otherwise, also when\n"
                "a file of that name exists.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 30: tld.h, path */
                "tld_path_full",
                "tld.h",
                "path",
                "File and directory checks, absolute paths, file and directory names of a path, and capturing the command line.",
                "int tld_path_full(tld_strbuf **full, const char *path);",
                "The absolute, symlink-free path of the existing file or directory path into *full.\n"
                "FAIL when it does not exist.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 31: tld.h, path */
                "tld_path_filename",
                "tld.h",
                "path",
                "File and directory checks, absolute paths, file and directory names of a path, and capturing the command line.",
                "int tld_path_filename(const char *path, tld_strbuf **out);",
                "The part of path after the last '/' into *out. FAIL when that part is empty (path is\n"
                "empty or ends in '/').",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 32: tld.h, path */
                "tld_path_dirname",
                "tld.h",
                "path",
                "File and directory checks, absolute paths, file and directory names of a path, and capturing the command line.",
                "int tld_path_dirname(const char *path, tld_strbuf **out);",
                "The part of path before the last '/' into *out (\"/\" for \"/name\"). FAIL when path\n"
                "contains no '/'.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 33: tld.h, path */
                "tld_path_read",
                "tld.h",
                "path",
                "File and directory checks, absolute paths, file and directory names of a path, and capturing the command line.",
                "int tld_path_read(const char *path, tld_strbuf **out);",
                "The whole contents of the file at path into *out; the bytes may include NUL. FAIL\n"
                "when it cannot be opened or read (the error names the file and the reason) or is\n"
                "larger than a buffer holds (INT32_MAX - 1 bytes).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 34: tld.h, path */
                "tld_cmdline_capture",
                "tld.h",
                "path",
                "File and directory checks, absolute paths, file and directory names of a path, and capturing the command line.",
                "int tld_cmdline_capture(tld_strbuf **cmdline, const int argc, char *const argv[]);",
                "argv[0] .. argv[argc - 1], each followed by a space, into *cmdline: for recording\n"
                "how a program was run.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 35: tld.h, timing */
                "TLD_START_TIMER, TLD_END_TIMER",
                "tld.h",
                "timing",
                "One global wall-clock stopwatch that logs the elapsed seconds on stderr.",
                "#define TLD_START_TIMER tld__timer(0);\n"
                "#define TLD_END_TIMER tld__timer(1);",
                "Complete statements (they include the semicolon): TLD_START_TIMER starts the\n"
                "stopwatch, TLD_END_TIMER prints the elapsed time (tld__timer(0) / (1)).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 36: tld.h, bitfield */
                "tld_bitfield",
                "tld.h",
                "bitfield",
                "Fixed-length bit sets with set, get and clear, OR, copy and (masked) Hamming distance.",
                "typedef struct {\n"
                "        uint64_t* m;\n"
                "        int32_t len;\n"
                "        int32_t alloc_len;\n"
                "} tld_bitfield;",
                "A set of len bits, stored in alloc_len 64-bit words m. Create with tld_bitfield_alloc,\n"
                "free with tld_bitfield_free.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 37: tld.h, bitfield */
                "tld_bitfield_alloc",
                "tld.h",
                "bitfield",
                "Fixed-length bit sets with set, get and clear, OR, copy and (masked) Hamming distance.",
                "int tld_bitfield_alloc(tld_bitfield **bit_f, int32_t size);",
                "A new bitfield of size bits, all 0, in *bit_f. Free with tld_bitfield_free.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 38: tld.h, bitfield */
                "tld_bitfield_free",
                "tld.h",
                "bitfield",
                "Fixed-length bit sets with set, get and clear, OR, copy and (masked) Hamming distance.",
                "void tld_bitfield_free(tld_bitfield *b);",
                "Free a bitfield; NULL is ignored.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 39: tld.h, bitfield */
                "tld_bitfield_reset",
                "tld.h",
                "bitfield",
                "Fixed-length bit sets with set, get and clear, OR, copy and (masked) Hamming distance.",
                "int tld_bitfield_reset(tld_bitfield *b);",
                "Clear every bit.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 40: tld.h, bitfield */
                "tld_bitfield_set, tld_bitfield_get, tld_bitfield_clear",
                "tld.h",
                "bitfield",
                "Fixed-length bit sets with set, get and clear, OR, copy and (masked) Hamming distance.",
                "int tld_bitfield_set(tld_bitfield *b, int32_t index);\n"
                "int tld_bitfield_get(tld_bitfield *b, int32_t index, uint8_t* bit);\n"
                "int tld_bitfield_clear(tld_bitfield *b, int32_t index);",
                "Set bit index to 1, read it into *bit (0 or 1), or clear it to 0. FAIL, without a\n"
                "message, when index is outside [0, len).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 41: tld.h, bitfield */
                "tld_bitfield_copy",
                "tld.h",
                "bitfield",
                "Fixed-length bit sets with set, get and clear, OR, copy and (masked) Hamming distance.",
                "int tld_bitfield_copy(tld_bitfield *dst, tld_bitfield *src);",
                "Copy the bits of src into dst; both must have the same length.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 42: tld.h, bitfield */
                "tld_bitfield_or",
                "tld.h",
                "bitfield",
                "Fixed-length bit sets with set, get and clear, OR, copy and (masked) Hamming distance.",
                "int tld_bitfield_or(tld_bitfield *b1, tld_bitfield *b2,tld_bitfield* ret);",
                "ret = b1 | b2, bit by bit; all three must have the same length.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 43: tld.h, bitfield */
                "tld_bitfield_hamming",
                "tld.h",
                "bitfield",
                "Fixed-length bit sets with set, get and clear, OR, copy and (masked) Hamming distance.",
                "int tld_bitfield_hamming(tld_bitfield *b1, tld_bitfield *b2,tld_bitfield *mask, int32_t *dist);",
                "The number of positions at which b1 and b2 differ into *dist, counting only positions\n"
                "set in mask (NULL: all). The bitfields (and mask) must have the same length.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 44: tld.h, hash */
                "tld_hash",
                "tld.h",
                "hash",
                "A string-to-string hash map (FNV-1a, chaining, grows automatically). Keys and values are copied in; pointers handed out are borrowed from the map. Not locked: one thread at a time per map.",
                "typedef struct tld_hash tld_hash;",
                "Opaque string-to-string map. Create with tld_hash_alloc (or let tld_hash_add create\n"
                "it), free with tld_hash_free.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 45: tld.h, hash */
                "tld_hash_alloc",
                "tld.h",
                "hash",
                "A string-to-string hash map (FNV-1a, chaining, grows automatically). Keys and values are copied in; pointers handed out are borrowed from the map. Not locked: one thread at a time per map.",
                "int tld_hash_alloc(tld_hash **hash, int size);",
                "Allocate an empty map in *hash with more than size buckets (a prime); it grows when\n"
                "the entries outnumber 3/4 of the buckets. Free with tld_hash_free.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 46: tld.h, hash */
                "tld_hash_free",
                "tld.h",
                "hash",
                "A string-to-string hash map (FNV-1a, chaining, grows automatically). Keys and values are copied in; pointers handed out are borrowed from the map. Not locked: one thread at a time per map.",
                "void tld_hash_free(tld_hash *h);",
                "Free the map and every key and value; NULL is ignored.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 47: tld.h, hash */
                "tld_hash_add",
                "tld.h",
                "hash",
                "A string-to-string hash map (FNV-1a, chaining, grows automatically). Keys and values are copied in; pointers handed out are borrowed from the map. Not locked: one thread at a time per map.",
                "int tld_hash_add(tld_hash **hash, const char *key, const char *val);",
                "Insert key -> val (both copied), replacing the value if key exists. When *hash is NULL\n"
                "a map is allocated first and stored in *hash.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 48: tld.h, hash */
                "tld_hash_get",
                "tld.h",
                "hash",
                "A string-to-string hash map (FNV-1a, chaining, grows automatically). Keys and values are copied in; pointers handed out are borrowed from the map. Not locked: one thread at a time per map.",
                "int tld_hash_get(tld_hash *h, const char *key, const char **val);",
                "Look up key: *val points to the stored value (borrowed and read-only: do not write\n"
                "to it or free it; valid until the entry changes or the map is freed). Returns FAIL, without a message, and sets *val to NULL when key\n"
                "is absent.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 49: tld.h, hash */
                "tld_hash_del",
                "tld.h",
                "hash",
                "A string-to-string hash map (FNV-1a, chaining, grows automatically). Keys and values are copied in; pointers handed out are borrowed from the map. Not locked: one thread at a time per map.",
                "int tld_hash_del(tld_hash *h, const char *key);",
                "Remove key. Returns FAIL, without a message, when key is absent.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 50: tld.h, hash */
                "tld_hash_rename",
                "tld.h",
                "hash",
                "A string-to-string hash map (FNV-1a, chaining, grows automatically). Keys and values are copied in; pointers handed out are borrowed from the map. Not locked: one thread at a time per map.",
                "int tld_hash_rename(tld_hash *h, const char *key, const char *new_key);",
                "Move the value stored under key to new_key (replacing any value there). Returns FAIL\n"
                "when key is absent.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 51: tld.h, hash */
                "tld_hash_iter_init",
                "tld.h",
                "hash",
                "A string-to-string hash map (FNV-1a, chaining, grows automatically). Keys and values are copied in; pointers handed out are borrowed from the map. Not locked: one thread at a time per map.",
                "int tld_hash_iter_init(tld_hash *h);",
                "Start iterating over the map (entries come in no particular order).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 52: tld.h, hash */
                "tld_hash_iter_next",
                "tld.h",
                "hash",
                "A string-to-string hash map (FNV-1a, chaining, grows automatically). Keys and values are copied in; pointers handed out are borrowed from the map. Not locked: one thread at a time per map.",
                "int tld_hash_iter_next(tld_hash* h, const char **key, const char **value);",
                "Next entry: *key and *value point into the map (borrowed and read-only). Returns FAIL, without a\n"
                "message, when there are no more entries. Do not add or delete while iterating.",
                "tld_hash *h = NULL;\n"
                "const char *k, *v;\n"
                "RUN(tld_hash_add(&h, \"a\", \"1\"));\n"
                "RUN(tld_hash_iter_init(h));\n"
                "while(tld_hash_iter_next(h, &k, &v) == OK){\n"
                "        printf(\"%s=%s\\n\", k, v);\n"
                "}\n"
                "tld_hash_free(h);",
                TLD_DOC_ENTRY
        },
        {       /* 53: tld.h, json types */
                "tld_json_type",
                "tld.h",
                "json types",
                "The JSON tree. An object (tld_json_obj) maps n keys to values, an array (tld_json_arr) holds n values, and each value (tld_json_val) is tagged with its tld_json_type. Containers own everything below them.",
                "typedef enum tld_json_type {\n"
                "        TLD_JSON_OBJ,\n"
                "        TLD_JSON_ARR,\n"
                "        TLD_JSON_STR,\n"
                "        TLD_JSON_DBL,\n"
                "        TLD_JSON_INT,\n"
                "        TLD_JSON_BOOL_TRUE,\n"
                "        TLD_JSON_BOOL_FALSE,\n"
                "        TLD_JSON_NULL\n"
                "} tld_json_type;",
                "Type tag of a tld_json_val. JSON null is TLD_JSON_NULL.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 54: tld.h, json types */
                "tld_json_val",
                "tld.h",
                "json types",
                "The JSON tree. An object (tld_json_obj) maps n keys to values, an array (tld_json_arr) holds n values, and each value (tld_json_val) is tagged with its tld_json_type. Containers own everything below them.",
                "typedef struct tld_json_val{\n"
                "        tld_json_type type;\n"
                "        union{\n"
                "                tld_json_obj* obj;\n"
                "                tld_json_arr* arr;\n"
                "                tld_strbuf* str;\n"
                "                double d_num;\n"
                "                int32_t i_num;\n"
                "                int8_t  b_num;\n"
                "        } value;\n"
                "} tld_json_val;",
                "One JSON value. type selects the member of value: obj, arr, str, d_num (TLD_JSON_DBL),\n"
                "i_num (TLD_JSON_INT) or b_num (the two BOOL types). Integers outside int32_t are stored\n"
                "as TLD_JSON_DBL.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 55: tld.h, json types */
                "tld_json_obj",
                "tld.h",
                "json types",
                "The JSON tree. An object (tld_json_obj) maps n keys to values, an array (tld_json_arr) holds n values, and each value (tld_json_val) is tagged with its tld_json_type. Containers own everything below them.",
                "typedef struct tld_json_obj{\n"
                "        tld_strbuf** key;\n"
                "        tld_json_val** v;\n"
                "        int n;\n"
                "        int n_alloc;\n"
                "        int top_level_arr;\n"
                "} tld_json_obj;",
                "A JSON object: key[i] maps to v[i] for i < n, in document order (duplicate keys are\n"
                "kept); n_alloc slots exist. Made by tld_json_parse, TLD_JSON_OBJECT or tld_json_obj_alloc;\n"
                "freed with tld_json_obj_free. top_level_arr is 1 in the object tld_json_parse makes for\n"
                "a top-level array: while it holds just that array, tld_json_dump writes the array.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 56: tld.h, json types */
                "tld_json_arr",
                "tld.h",
                "json types",
                "The JSON tree. An object (tld_json_obj) maps n keys to values, an array (tld_json_arr) holds n values, and each value (tld_json_val) is tagged with its tld_json_type. Containers own everything below them.",
                "typedef struct tld_json_arr {\n"
                "        tld_json_val** v;\n"
                "        int read_head;\n"
                "        int n;\n"
                "        int n_alloc;\n"
                "} tld_json_arr;",
                "A JSON array of n values v[0..n-1] (read_head is unused). Made by TLD_JSON_ARRAY or\n"
                "tld_json_arr_alloc; freed with tld_json_arr_free.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 57: tld.h, json types */
                "tld_json_decoration",
                "tld.h",
                "json types",
                "The JSON tree. An object (tld_json_obj) maps n keys to values, an array (tld_json_arr) holds n values, and each value (tld_json_val) is tagged with its tld_json_type. Containers own everything below them.",
                "typedef struct {\n"
                "        char* key;\n"
                "        void* value;\n"
                "        /* TLD__JVAL_AUTO guesses the type from the text (TLD_JAUTO); the values after\n"
                "           TLD__JVAL_ARR are the typed decorations TLD_JSTR, TLD_JNUM, TLD_JINT, TLD_JBOOL, TLD_JNULL. */\n"
                "        enum { TLD__JVAL_AUTO, TLD__JVAL_OBJ, TLD__JVAL_ARR, TLD__JVAL_STR, TLD__JVAL_NUM, TLD__JVAL_INT, TLD__JVAL_BOOL, TLD__JVAL_NULL } type;\n"
                "} tld_json_decoration;",
                "One entry for TLD_JSON_OBJECT / TLD_JSON_ARRAY, made by TLD_JAUTO, TLD_JOBJ, TLD_JARR, TLD_JSTR, TLD_JNUM, TLD_JINT, TLD_JBOOL or\n"
                "TLD_JNULL.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 58: tld.h, json memory */
                "tld_json_obj_alloc",
                "tld.h",
                "json memory",
                "Allocate, deep-copy and free JSON objects, arrays and values.",
                "int tld_json_obj_alloc(tld_json_obj **obj);",
                "A new empty object in *obj. Free with tld_json_obj_free.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 59: tld.h, json memory */
                "tld_json_obj_free",
                "tld.h",
                "json memory",
                "Allocate, deep-copy and free JSON objects, arrays and values.",
                "void tld_json_obj_free(tld_json_obj *n);",
                "Free an object with all its keys and values; NULL is ignored.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 60: tld.h, json memory */
                "tld_json_obj_copy",
                "tld.h",
                "json memory",
                "Allocate, deep-copy and free JSON objects, arrays and values.",
                "int tld_json_obj_copy(tld_json_obj **target, tld_json_obj *s);",
                "Deep copy of the object s into a new object in *target (free with tld_json_obj_free).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 61: tld.h, json memory */
                "tld_json_arr_alloc",
                "tld.h",
                "json memory",
                "Allocate, deep-copy and free JSON objects, arrays and values.",
                "int tld_json_arr_alloc(tld_json_arr **arr);",
                "A new empty array in *arr. Free with tld_json_arr_free.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 62: tld.h, json memory */
                "tld_json_arr_free",
                "tld.h",
                "json memory",
                "Allocate, deep-copy and free JSON objects, arrays and values.",
                "void tld_json_arr_free(tld_json_arr *n);",
                "Free an array with all its values; NULL is ignored.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 63: tld.h, json memory */
                "tld_json_arr_copy",
                "tld.h",
                "json memory",
                "Allocate, deep-copy and free JSON objects, arrays and values.",
                "int tld_json_arr_copy(tld_json_arr **t, tld_json_arr *s);",
                "Deep copy of the array s into a new array in *t (free with tld_json_arr_free).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 64: tld.h, json memory */
                "tld_json_val_alloc",
                "tld.h",
                "json memory",
                "Allocate, deep-copy and free JSON objects, arrays and values.",
                "int tld_json_val_alloc(tld_json_val **node);",
                "A new value of type TLD_JSON_NULL (null) in *node. Free with tld_json_val_free.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 65: tld.h, json memory */
                "tld_json_val_free",
                "tld.h",
                "json memory",
                "Allocate, deep-copy and free JSON objects, arrays and values.",
                "void tld_json_val_free(tld_json_val *n);",
                "Free a value and everything it owns; NULL is ignored.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 66: tld.h, json memory */
                "tld_json_val_copy",
                "tld.h",
                "json memory",
                "Allocate, deep-copy and free JSON objects, arrays and values.",
                "int tld_json_val_copy(tld_json_val *t, tld_json_val *s);",
                "Deep copy s into t. Whatever t held is released first, so t may change type; s may\n"
                "live inside t.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 67: tld.h, json */
                "tld_json_parse, tld_json_parse_n",
                "tld.h",
                "json",
                "Parse JSON text into a tree (tld_json_parse), build trees with TLD_JSON_OBJECT and TLD_JSON_ARRAY, read values by path (tld_json_get_*), search and update, and write JSON text (tld_json_dump).",
                "int tld_json_parse(const char *text, tld_json_obj **out);\n"
                "int tld_json_parse_n(const char *text, size_t len, tld_json_obj **out);",
                "Parse the NUL-terminated JSON text (RFC 8259, nesting up to 512 levels) into a new\n"
                "object in *out; free it with tld_json_obj_free. A top-level array becomes the single\n"
                "entry of an object, under the key \"Top Level Arr\" (tld_json_dump writes it back as the\n"
                "array). Strings must be well-formed UTF-8 without raw control characters; a lone\n"
                "surrogate escape (\\ud800) becomes U+FFFD. Numbers too large for a double are an error\n"
                "(underflow gives 0); integers outside int32_t are stored as doubles. Invalid JSON\n"
                "prints an error naming the byte offset and returns FAIL. tld_json_parse_n parses the len bytes\n"
                "at text instead (a NUL byte among them is invalid JSON). For a tld_strbuf b:\n"
                "tld_json_parse(TLD_STR(b), &o).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 68: tld.h, json */
                "tld_json_search",
                "tld.h",
                "json",
                "Parse JSON text into a tree (tld_json_parse), build trees with TLD_JSON_OBJECT and TLD_JSON_ARRAY, read values by path (tld_json_get_*), search and update, and write JSON text (tld_json_dump).",
                "int tld_json_search(tld_json_obj *n, char *key, tld_json_obj **ret);",
                "Collect deep copies of every value stored under key, at any depth, into a new object\n"
                "in *ret (entries named key, in document order; free with tld_json_obj_free). OK with\n"
                "an empty result when nothing matches.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 69: tld.h, json */
                "tld_json_get_str, tld_json_get_num, tld_json_get_int, tld_json_get_bool, tld_json_get_obj, tld_json_get_arr",
                "tld.h",
                "json",
                "Parse JSON text into a tree (tld_json_parse), build trees with TLD_JSON_OBJECT and TLD_JSON_ARRAY, read values by path (tld_json_get_*), search and update, and write JSON text (tld_json_dump).",
                "int tld_json_get_str(tld_json_obj *obj, const char *path, const char **out);\n"
                "int tld_json_get_num(tld_json_obj *obj, const char *path, double *out);\n"
                "int tld_json_get_int(tld_json_obj *obj, const char *path, int64_t *out);\n"
                "int tld_json_get_bool(tld_json_obj *obj, const char *path, int *out);\n"
                "int tld_json_get_obj(tld_json_obj *obj, const char *path, tld_json_obj **out);\n"
                "int tld_json_get_arr(tld_json_obj *obj, const char *path, tld_json_arr **out);",
                "Path getters. A path is dot-separated keys with [i] for array elements,\n"
                "e.g. \"message.content\", \"embeddings[0]\", \"models[2].name\". tld_json_parse\n"
                "stores a top-level array under the key \"Top Level Arr\"; address it as\n"
                "\"Top Level Arr[0]...\" or simply \"[0]...\". Keys containing '.' or '[' cannot\n"
                "be addressed. All return OK, or FAIL (leaving *out unchanged) when the path\n"
                "does not exist or the value has another type. Strings, objects and arrays\n"
                "are borrowed from the tree. get_num accepts integers; get_int accepts\n"
                "integral doubles inside the int64_t range. JSON null has type\n"
                "TLD_JSON_NULL and fails every getter. An empty path in get_obj returns\n"
                "obj itself. out may be NULL to test whether the path exists.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 70: tld.h, json */
                "tld_json_set_val",
                "tld.h",
                "json",
                "Parse JSON text into a tree (tld_json_parse), build trees with TLD_JSON_OBJECT and TLD_JSON_ARRAY, read values by path (tld_json_get_*), search and update, and write JSON text (tld_json_dump).",
                "int tld_json_set_val(tld_json_obj *n, const char *path, tld_json_val *v);",
                "Set the value at a path to a deep copy of v. key is a path in the syntax of the\n"
                "tld_json_get_* getters (\"name\", \"a.b\", \"list[2].id\", \"[0]\" for a parsed top-level\n"
                "array). Everything before the last step must exist. The last step may name a key\n"
                "that is missing (it is added at the end of its object) or the index one past the\n"
                "end of an array (v is appended). An existing value is replaced, even by one of\n"
                "another type, and v may be part of n. Returns FAIL for a missing parent, an index\n"
                "further past the end, or no memory. Keys containing '.' or '[' cannot be addressed.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 71: tld.h, json */
                "TLD_JAUTO, TLD_JOBJ, TLD_JARR",
                "tld.h",
                "json",
                "Parse JSON text into a tree (tld_json_parse), build trees with TLD_JSON_OBJECT and TLD_JSON_ARRAY, read values by path (tld_json_get_*), search and update, and write JSON text (tld_json_dump).",
                "#define TLD_JAUTO(k,v) ((tld_json_decoration){.key =(char*)k, .type = TLD__JVAL_AUTO, .value = (void*)(v)})\n"
                "#define TLD_JOBJ(k,v) ((tld_json_decoration){.key =(char*)k, .type = TLD__JVAL_OBJ, .value = (void*)(v)})\n"
                "#define TLD_JARR(k,v) ((tld_json_decoration){.key =(char*)k, .type = TLD__JVAL_ARR, .value = (void*)(v)})",
                "Entries for TLD_JSON_OBJECT / TLD_JSON_ARRAY. TLD_JAUTO(k, s): the string s, with its type guessed from\n"
                "the text: \"12\" becomes an integer, \"0.2\" a number, true/false (also True, TRUE, ...) a\n"
                "boolean, anything else a string. TLD_JOBJ(k, o) / TLD_JARR(k, a): a nested tld_json_obj * /\n"
                "tld_json_arr *, which the new container takes over (do not free it yourself), also\n"
                "when the build fails. A TLD_JAUTO or TLD_JSTR entry whose string is NULL is skipped; a TLD_JOBJ or TLD_JARR\n"
                "entry whose value is NULL makes the build fail, since a nested TLD_JSON_OBJECT / TLD_JSON_ARRAY\n"
                "returns NULL only when it failed. Arrays ignore the key; pass NULL.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 72: tld.h, json */
                "TLD_JSTR, TLD_JNUM, TLD_JINT, TLD_JBOOL, TLD_JNULL",
                "tld.h",
                "json",
                "Parse JSON text into a tree (tld_json_parse), build trees with TLD_JSON_OBJECT and TLD_JSON_ARRAY, read values by path (tld_json_get_*), search and update, and write JSON text (tld_json_dump).",
                "#define TLD_JSTR(k,v)  ((tld_json_decoration){.key =(char*)k, .type = TLD__JVAL_STR, .value = (void*)(v)})\n"
                "#define TLD_JNUM(k,v)  ((tld_json_decoration){.key =(char*)k, .type = TLD__JVAL_NUM,  .value = (void*)&(double){(v)}})\n"
                "#define TLD_JINT(k,v)  ((tld_json_decoration){.key =(char*)k, .type = TLD__JVAL_INT,  .value = (void*)&(long){(v)}})\n"
                "#define TLD_JBOOL(k,v) ((tld_json_decoration){.key =(char*)k, .type = TLD__JVAL_BOOL, .value = (void*)&(int){(v)}})\n"
                "#define TLD_JNULL(k)   ((tld_json_decoration){.key =(char*)k, .type = TLD__JVAL_NULL, .value = (void*)\"\"})",
                "Typed decorations. TLD_JAUTO above guesses the type from the text (\"0.2\" becomes\n"
                "a number); these do not. TLD_JSTR(k,s) is always a string, TLD_JNUM(k,d) a number\n"
                "(non-finite values are written as null), TLD_JINT(k,l) an integer (values\n"
                "outside int32_t are stored as double), TLD_JBOOL(k,b) true/false, TLD_JNULL(k) null.\n"
                "A NULL string in TLD_JSTR skips the entry, like TLD_JAUTO.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 73: tld.h, json */
                "TLD_JSON_OBJECT, TLD_JSON_ARRAY",
                "tld.h",
                "json",
                "Parse JSON text into a tree (tld_json_parse), build trees with TLD_JSON_OBJECT and TLD_JSON_ARRAY, read values by path (tld_json_get_*), search and update, and write JSON text (tld_json_dump).",
                "#define TLD_JSON_OBJECT(...) tld__json_obj_build(NULL, (tld_json_decoration[]){__VA_ARGS__}, \\\n"
                "                                            sizeof((tld_json_decoration[]){__VA_ARGS__})/sizeof(tld_json_decoration))\n"
                "#define TLD_JSON_ARRAY(...) tld__json_arr_build(NULL, (tld_json_decoration[]){__VA_ARGS__}, \\\n"
                "                                            sizeof((tld_json_decoration[]){__VA_ARGS__})/sizeof(tld_json_decoration))",
                "TLD_JSON_OBJECT(entry, ...): a new tld_json_obj * holding the entries in order; TLD_JSON_ARRAY(...)\n"
                "a new tld_json_arr *. NULL (with an error printed) when an entry cannot be added: out\n"
                "of memory, or a TLD_JOBJ/TLD_JARR entry holding NULL. The containers passed in with TLD_JOBJ/TLD_JARR are then\n"
                "freed too. Free the result with tld_json_obj_free / tld_json_arr_free.",
                "tld_json_obj *o = TLD_JSON_OBJECT(TLD_JSTR(\"name\", \"x\"), TLD_JINT(\"n\", 3),\n"
                "                           TLD_JARR(\"v\", TLD_JSON_ARRAY(TLD_JNUM(NULL, 0.5), TLD_JNUM(NULL, 2))));\n"
                "ASSERT(o != NULL, \"Could not build the object\");\n"
                "tld_json_obj_free(o);",
                TLD_DOC_ENTRY
        },
        {       /* 74: tld.h, json */
                "tld_json_obj_add, tld_json_arr_add",
                "tld.h",
                "json",
                "Parse JSON text into a tree (tld_json_parse), build trees with TLD_JSON_OBJECT and TLD_JSON_ARRAY, read values by path (tld_json_get_*), search and update, and write JSON text (tld_json_dump).",
                "int tld_json_obj_add(tld_json_obj **obj, tld_json_decoration entry);\n"
                "int tld_json_arr_add(tld_json_arr **arr, tld_json_decoration entry);",
                "Add one entry to *obj / one value to *arr, creating it when *obj / *arr is NULL:\n"
                "\n"
                "Entries are those of TLD_JSON_OBJECT: a TLD_JOBJ/TLD_JARR value passes to the container,\n"
                "also on failure (it is then freed), and a TLD_JAUTO/TLD_JSTR entry whose string is NULL\n"
                "is skipped (nothing is created). FAIL with an error when out of memory or for a\n"
                "TLD_JOBJ/TLD_JARR holding NULL; the container keeps what it had (a new one is freed,\n"
                "leaving *obj / *arr NULL).",
                "tld_json_obj *req = NULL;\n"
                "RUN(tld_json_obj_add(&req, TLD_JSTR(\"model\", \"inkling-2-mini\")));\n"
                "RUN(tld_json_obj_add(&req, TLD_JBOOL(\"stream\", 0)));\n"
                "tld_json_obj_free(req);",
                TLD_DOC_ENTRY
        },
        {       /* 75: tld.h, json */
                "TLD_JSON_COMPACT",
                "tld.h",
                "json",
                "Parse JSON text into a tree (tld_json_parse), build trees with TLD_JSON_OBJECT and TLD_JSON_ARRAY, read values by path (tld_json_get_*), search and update, and write JSON text (tld_json_dump).",
                "#define TLD_JSON_COMPACT (-1)",
                "The indent for compact JSON: tld_json_dump(n, b, TLD_JSON_COMPACT) writes no spaces or\n"
                "newlines outside strings.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 76: tld.h, json */
                "tld_json_dump, tld_json_dump_arr",
                "tld.h",
                "json",
                "Parse JSON text into a tree (tld_json_parse), build trees with TLD_JSON_OBJECT and TLD_JSON_ARRAY, read values by path (tld_json_get_*), search and update, and write JSON text (tld_json_dump).",
                "int tld_json_dump(tld_json_obj *n, tld_strbuf *b,int indent);\n"
                "int tld_json_dump_arr(tld_json_arr *n, tld_strbuf *b, int indent);",
                "Append n as JSON text to b (b is not cleared): with indent >= 0, 4 spaces of\n"
                "indentation per level on top of indent (pass 0); with TLD_JSON_COMPACT, no whitespace.\n"
                "Strings are escaped (a byte that is not well-formed UTF-8 is written as \\ufffd, so the\n"
                "text is always valid JSON), doubles written with the fewest of 15 to 17 significant\n"
                "digits that read back exactly, inf/nan as null. A parsed top-level array is written\n"
                "as the array (see top_level_arr). A NULL n appends nothing.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 77: tld.h, template */
                "tld_template_apply",
                "tld.h",
                "template",
                "Fill {{name}} placeholders and {{IF name}}...{{ELSE}}...{{ENDIF}} blocks in text from a tld_hash of names to values.",
                "int tld_template_apply(tld_strbuf *txt, tld_hash *map);",
                "Render the template in txt in place. {{name}} becomes map's value for name (nothing\n"
                "when absent); {{IF name}}A{{ELSE}}B{{ENDIF}} keeps A when name has a non-empty value\n"
                "and B otherwise (ELSE is optional; IFs nest up to 512 deep). The text between the\n"
                "braces is the key as written (no trimming, 1 to 63 characters). map must not be NULL.\n"
                "A malformed template (unclosed IF or tag, ELSE or ENDIF without IF, an empty {{}}, an\n"
                "IF with no name, IFs nested deeper than 512) prints the reason with its byte offset\n"
                "and returns FAIL with txt unchanged.",
                "tld_hash *m = NULL;\n"
                "tld_strbuf *txt = NULL;\n"
                "RUN(tld_hash_add(&m, \"who\", \"world\"));\n"
                "RUN(tld_strbuf_alloc(&txt, 64));\n"
                "RUN(tld_strbuf_append(txt, \"Hello {{who}}{{IF excl}}!{{ENDIF}}\"));\n"
                "RUN(tld_template_apply(txt, m));        // txt is now \"Hello world\"\n"
                "tld_strbuf_free(txt);\n"
                "tld_hash_free(m);",
                TLD_DOC_ENTRY
        },
        {       /* 78: tld.h, template */
                "tld_template_vars",
                "tld.h",
                "template",
                "Fill {{name}} placeholders and {{IF name}}...{{ELSE}}...{{ENDIF}} blocks in text from a tld_hash of names to values.",
                "int tld_template_vars(tld_strbuf *txt, tld_hash **hash);",
                "A new map in *hash (free with tld_hash_free) whose keys are the variable names used\n"
                "in txt, from {{name}} and {{IF name}}, each mapped to itself.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 79: tld.h, rng */
                "tld_rng",
                "tld.h",
                "rng",
                "The xoshiro256** random number generator (seedable, and splittable for threads) with uniform, integer, Gaussian and gamma draws, plus alias-table sampling from a discrete distribution. A generator is not locked: use one per thread.",
                "typedef struct tld_rng tld_rng;",
                "Opaque generator state. Create with tld_rng_alloc or tld_rng_split, free with\n"
                "tld_rng_free.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 80: tld.h, rng */
                "tld_rng_dist",
                "tld.h",
                "rng",
                "The xoshiro256** random number generator (seedable, and splittable for threads) with uniform, integer, Gaussian and gamma draws, plus alias-table sampling from a discrete distribution. A generator is not locked: use one per thread.",
                "struct tld_rng_dist;",
                "Opaque alias table for sampling from a discrete distribution (tld_rng_dist_*).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 81: tld.h, rng */
                "tld_rng_alloc",
                "tld.h",
                "rng",
                "The xoshiro256** random number generator (seedable, and splittable for threads) with uniform, integer, Gaussian and gamma draws, plus alias-table sampling from a discrete distribution. A generator is not locked: use one per thread.",
                "int tld_rng_alloc(struct tld_rng **rng, uint64_t seed);",
                "Allocate a generator in *rng seeded with seed; the same seed gives the same sequence.\n"
                "seed 0 picks a seed from /dev/urandom, mixed with the time, the process id and a\n"
                "counter, so every generator made with seed 0 has its own stream, also across jobs\n"
                "started at the same moment. Free with tld_rng_free.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 82: tld.h, rng */
                "tld_rng_split",
                "tld.h",
                "rng",
                "The xoshiro256** random number generator (seedable, and splittable for threads) with uniform, integer, Gaussian and gamma draws, plus alias-table sampling from a discrete distribution. A generator is not locked: use one per thread.",
                "int tld_rng_split(struct tld_rng** target, struct tld_rng* rng);",
                "Allocate a generator for another thread in *target: it continues rng's current\n"
                "stream, and rng itself jumps 2^128 draws ahead, so the two streams never overlap.\n"
                "Free with tld_rng_free.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 83: tld.h, rng */
                "tld_rng_set_seed",
                "tld.h",
                "rng",
                "The xoshiro256** random number generator (seedable, and splittable for threads) with uniform, integer, Gaussian and gamma draws, plus alias-table sampling from a discrete distribution. A generator is not locked: use one per thread.",
                "int tld_rng_set_seed(struct tld_rng *s, uint64_t seed);",
                "Reseed an existing generator (seed is used as given, 0 included) and drop any cached\n"
                "Gaussian deviate.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 84: tld.h, rng */
                "tld_rng_free",
                "tld.h",
                "rng",
                "The xoshiro256** random number generator (seedable, and splittable for threads) with uniform, integer, Gaussian and gamma draws, plus alias-table sampling from a discrete distribution. A generator is not locked: use one per thread.",
                "void tld_rng_free(struct tld_rng* rng);",
                "Free a generator; NULL is ignored.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 85: tld.h, rng */
                "tld_rng_double",
                "tld.h",
                "rng",
                "The xoshiro256** random number generator (seedable, and splittable for threads) with uniform, integer, Gaussian and gamma draws, plus alias-table sampling from a discrete distribution. A generator is not locked: use one per thread.",
                "double tld_rng_double(struct tld_rng* rng);",
                "Uniform double on the open interval (0, 1): never exactly 0 or 1. The double results\n"
                "below are NaN, and tld_rng_int / tld_rng_dist_sample give -1, for a NULL generator or\n"
                "table (with an error).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 86: tld.h, rng */
                "tld_rng_gaussian",
                "tld.h",
                "rng",
                "The xoshiro256** random number generator (seedable, and splittable for threads) with uniform, integer, Gaussian and gamma draws, plus alias-table sampling from a discrete distribution. A generator is not locked: use one per thread.",
                "double tld_rng_gaussian(struct tld_rng* rng, double mu, double sigma);",
                "Normal deviate with mean mu and standard deviation sigma (Marsaglia's polar method:\n"
                "values are made in pairs and every other call returns the cached second one, a cache\n"
                "shared with tld_rng_gamma).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 87: tld.h, rng */
                "tld_rng_gamma",
                "tld.h",
                "rng",
                "The xoshiro256** random number generator (seedable, and splittable for threads) with uniform, integer, Gaussian and gamma draws, plus alias-table sampling from a discrete distribution. A generator is not locked: use one per thread.",
                "double tld_rng_gamma(struct tld_rng* rng, double shape, double scale);",
                "Gamma deviate with the given shape (> 0, not checked) and scale; mean shape * scale.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 88: tld.h, rng */
                "tld_rng_int",
                "tld.h",
                "rng",
                "The xoshiro256** random number generator (seedable, and splittable for threads) with uniform, integer, Gaussian and gamma draws, plus alias-table sampling from a discrete distribution. A generator is not locked: use one per thread.",
                "int tld_rng_int(struct tld_rng* rng,int a);",
                "Uniform integer in [0, a), without modulo bias; 0 when a <= 1.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 89: tld.h, rng */
                "tld_rng_dist_alloc",
                "tld.h",
                "rng",
                "The xoshiro256** random number generator (seedable, and splittable for threads) with uniform, integer, Gaussian and gamma draws, plus alias-table sampling from a discrete distribution. A generator is not locked: use one per thread.",
                "int tld_rng_dist_alloc(struct tld_rng_dist** dist, double* w, int n);",
                "Build in *dist an alias table that samples index i in [0, n) with probability\n"
                "w[i] / sum(w). Weights must be >= 0 with a finite, positive sum. The table holds no\n"
                "generator: each sample draws from the one passed in, so one table can serve several\n"
                "threads, each with its own generator. Free with tld_rng_dist_free.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 90: tld.h, rng */
                "tld_rng_dist_sample",
                "tld.h",
                "rng",
                "The xoshiro256** random number generator (seedable, and splittable for threads) with uniform, integer, Gaussian and gamma draws, plus alias-table sampling from a discrete distribution. A generator is not locked: use one per thread.",
                "int tld_rng_dist_sample(struct tld_rng_dist* d, struct tld_rng* rng);",
                "One sample from the table, drawn with rng: an index in [0, n).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 91: tld.h, rng */
                "tld_rng_dist_free",
                "tld.h",
                "rng",
                "The xoshiro256** random number generator (seedable, and splittable for threads) with uniform, integer, Gaussian and gamma draws, plus alias-table sampling from a discrete distribution. A generator is not locked: use one per thread.",
                "void tld_rng_dist_free(struct tld_rng_dist* d);",
                "Free an alias table; NULL is ignored.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 92: tld.h, sort */
                "tld_sort_desc_char, tld_sort_asc_char, tld_sort_desc_int8_t, tld_sort_asc_int8_t, tld_sort_desc_uint8_t, tld_sort_asc_uint8_t, tld_sort_desc_int16_t, tld_sort_asc_int16_t, tld_sort_desc_uint16_t, tld_sort_asc_uint16_t, tld_sort_desc_int32_t, tld_sort_asc_int32_t, tld_sort_desc_uint32_t, tld_sort_asc_uint32_t, tld_sort_desc_int64_t, tld_sort_asc_int64_t, tld_sort_desc_uint64_t, tld_sort_asc_uint64_t, tld_sort_desc_float, tld_sort_asc_float, tld_sort_desc_double, tld_sort_asc_double",
                "tld.h",
                "sort",
                "In-place sorting of arrays of char, int8_t..int64_t, uint8_t..uint64_t, float and double, ascending or descending; TLD_SORT_ASC and TLD_SORT_DESC pick the function from the array's type.",
                "int tld_sort_desc_char(char *a, int len);\n"
                "int tld_sort_asc_char(char* a, int len);\n"
                "int tld_sort_desc_int8_t(int8_t *a, int len);\n"
                "int tld_sort_asc_int8_t(int8_t* a, int len);\n"
                "int tld_sort_desc_uint8_t(uint8_t *a, int len);\n"
                "int tld_sort_asc_uint8_t(uint8_t* a, int len);\n"
                "int tld_sort_desc_int16_t(int16_t *a, int len);\n"
                "int tld_sort_asc_int16_t(int16_t* a, int len);\n"
                "int tld_sort_desc_uint16_t(uint16_t *a, int len);\n"
                "int tld_sort_asc_uint16_t(uint16_t* a, int len);\n"
                "int tld_sort_desc_int32_t(int32_t *a, int len);\n"
                "int tld_sort_asc_int32_t(int32_t* a, int len);\n"
                "int tld_sort_desc_uint32_t(uint32_t *a, int len);\n"
                "int tld_sort_asc_uint32_t(uint32_t* a, int len);\n"
                "int tld_sort_desc_int64_t(int64_t *a, int len);\n"
                "int tld_sort_asc_int64_t(int64_t* a, int len);\n"
                "int tld_sort_desc_uint64_t(uint64_t *a, int len);\n"
                "int tld_sort_asc_uint64_t(uint64_t* a, int len);\n"
                "int tld_sort_desc_float(float *a, int len);\n"
                "int tld_sort_asc_float(float* a, int len);\n"
                "int tld_sort_desc_double(double *a, int len);\n"
                "int tld_sort_asc_double(double* a, int len);",
                "Sort len elements of a in place, descending (sort_desc_*) or ascending (sort_asc_*),\n"
                "with qsort (not stable). Always OK. Arrays holding NaN have no defined order.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 93: tld.h, sort */
                "TLD_SORT_DESC",
                "tld.h",
                "sort",
                "In-place sorting of arrays of char, int8_t..int64_t, uint8_t..uint64_t, float and double, ascending or descending; TLD_SORT_ASC and TLD_SORT_DESC pick the function from the array's type.",
                "#define TLD_SORT_DESC(A,N) ...",
                "TLD_SORT_DESC(a, len): sort_desc_<type>(a, len) for a's element type.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 94: tld.h, sort */
                "TLD_SORT_ASC",
                "tld.h",
                "sort",
                "In-place sorting of arrays of char, int8_t..int64_t, uint8_t..uint64_t, float and double, ascending or descending; TLD_SORT_ASC and TLD_SORT_DESC pick the function from the array's type.",
                "#define TLD_SORT_ASC(A,N) ...",
                "TLD_SORT_ASC(a, len): sort_asc_<type>(a, len) for a's element type.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 95: tld.h, shuffle */
                "tld_shuffle_char, tld_shuffle_int8_t, tld_shuffle_uint8_t, tld_shuffle_int16_t, tld_shuffle_uint16_t, tld_shuffle_int32_t, tld_shuffle_uint32_t, tld_shuffle_int64_t, tld_shuffle_uint64_t, tld_shuffle_float, tld_shuffle_double",
                "tld.h",
                "shuffle",
                "Uniform random permutation (Fisher-Yates) in place of arrays of char, int8_t..int64_t, uint8_t..uint64_t, float and double; TLD_SHUFFLE picks the function from the array's type.",
                "int tld_shuffle_char(char *a, int len, struct tld_rng* rng);\n"
                "int tld_shuffle_int8_t(int8_t *a, int len, struct tld_rng* rng);\n"
                "int tld_shuffle_uint8_t(uint8_t *a, int len, struct tld_rng* rng);\n"
                "int tld_shuffle_int16_t(int16_t *a, int len, struct tld_rng* rng);\n"
                "int tld_shuffle_uint16_t(uint16_t *a, int len, struct tld_rng* rng);\n"
                "int tld_shuffle_int32_t(int32_t *a, int len, struct tld_rng* rng);\n"
                "int tld_shuffle_uint32_t(uint32_t *a, int len, struct tld_rng* rng);\n"
                "int tld_shuffle_int64_t(int64_t *a, int len, struct tld_rng* rng);\n"
                "int tld_shuffle_uint64_t(uint64_t *a, int len, struct tld_rng* rng);\n"
                "int tld_shuffle_float(float *a, int len, struct tld_rng* rng);\n"
                "int tld_shuffle_double(double *a, int len, struct tld_rng* rng);",
                "Shuffle len elements of a in place, drawing from rng (see tld_rng_alloc). Always OK.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 96: tld.h, shuffle */
                "TLD_SHUFFLE",
                "tld.h",
                "shuffle",
                "Uniform random permutation (Fisher-Yates) in place of arrays of char, int8_t..int64_t, uint8_t..uint64_t, float and double; TLD_SHUFFLE picks the function from the array's type.",
                "#define TLD_SHUFFLE(A,N,R) ...",
                "TLD_SHUFFLE(a, len, rng): tld_shuffle_<type>(a, len, rng) for a's element type.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 97: tld.h, stats */
                "tld_shannon",
                "tld.h",
                "stats",
                "Descriptive statistics of double arrays (mean, variance, covariance, Pearson correlation, Shannon entropy, standardising), the sigmoid, the normal density and Gaussian kernel density estimates. Results come back through the last argument.",
                "int tld_shannon(double *vec, int n, double *entropy);",
                "Shannon entropy in bits of the distribution vec[i] / sum(vec) (n >= 1, every\n"
                "vec[i] >= 0); 0 when all values are 0.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 98: tld.h, stats */
                "tld_mean",
                "tld.h",
                "stats",
                "Descriptive statistics of double arrays (mean, variance, covariance, Pearson correlation, Shannon entropy, standardising), the sigmoid, the normal density and Gaussian kernel density estimates. Results come back through the last argument.",
                "int tld_mean(double *vec, int n, double *mean);",
                "Arithmetic mean of n >= 1 values.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 99: tld.h, stats */
                "tld_pop_variance, tld_sample_variance",
                "tld.h",
                "stats",
                "Descriptive statistics of double arrays (mean, variance, covariance, Pearson correlation, Shannon entropy, standardising), the sigmoid, the normal density and Gaussian kernel density estimates. Results come back through the last argument.",
                "int tld_pop_variance(double *vec, int n, double *variance);\n"
                "int tld_sample_variance(double *vec, int n, double *variance);",
                "Variance dividing by n (population, n >= 1) or by n - 1 (sample, n >= 2).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 100: tld.h, stats */
                "tld_standardise",
                "tld.h",
                "stats",
                "Descriptive statistics of double arrays (mean, variance, covariance, Pearson correlation, Shannon entropy, standardising), the sigmoid, the normal density and Gaussian kernel density estimates. Results come back through the last argument.",
                "int tld_standardise(double *vec, int n, double *mean, double *stdev);",
                "Standardise vec in place to (x - mean) / sd, with the sample standard deviation;\n"
                "the mean and sd used come back in *mean and *stdev. FAIL when sd is 0 or n < 2.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 101: tld.h, stats */
                "tld_sample_covariance, tld_pop_covariance",
                "tld.h",
                "stats",
                "Descriptive statistics of double arrays (mean, variance, covariance, Pearson correlation, Shannon entropy, standardising), the sigmoid, the normal density and Gaussian kernel density estimates. Results come back through the last argument.",
                "int tld_sample_covariance(double *vec_a, double *vec_b, int n, double *cov);\n"
                "int tld_pop_covariance(double *vec_a, double *vec_b, int n, double *cov);",
                "Covariance of two arrays of n values, dividing by n - 1 (sample, n >= 2) or by n\n"
                "(population).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 102: tld.h, stats */
                "tld_pearson_corr",
                "tld.h",
                "stats",
                "Descriptive statistics of double arrays (mean, variance, covariance, Pearson correlation, Shannon entropy, standardising), the sigmoid, the normal density and Gaussian kernel density estimates. Results come back through the last argument.",
                "int tld_pearson_corr(double *vec_a, double *vec_b, int n, double *corr);",
                "Pearson correlation of two arrays of n values, in [-1, 1]; 0 when either array is\n"
                "constant.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 103: tld.h, stats */
                "tld_sigmoid",
                "tld.h",
                "stats",
                "Descriptive statistics of double arrays (mean, variance, covariance, Pearson correlation, Shannon entropy, standardising), the sigmoid, the normal density and Gaussian kernel density estimates. Results come back through the last argument.",
                "int tld_sigmoid(double x, double *out);",
                "1 / (1 + exp(-x)) into *out, computed without overflow, for every x: NaN gives NaN,\n"
                "+inf 1 and -inf 0.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 104: tld.h, stats */
                "tld_normal_pdf",
                "tld.h",
                "stats",
                "Descriptive statistics of double arrays (mean, variance, covariance, Pearson correlation, Shannon entropy, standardising), the sigmoid, the normal density and Gaussian kernel density estimates. Results come back through the last argument.",
                "int tld_normal_pdf(double x, double mu, double sigma, double* p);",
                "Density at x of the normal distribution with mean mu and standard deviation\n"
                "sigma (> 0).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 105: tld.h, stats */
                "tld_kde_pdf",
                "tld.h",
                "stats",
                "Descriptive statistics of double arrays (mean, variance, covariance, Pearson correlation, Shannon entropy, standardising), the sigmoid, the normal density and Gaussian kernel density estimates. Results come back through the last argument.",
                "int tld_kde_pdf(double *data, int data_len, int n_steps, double ***out);",
                "Gaussian kernel density estimate of data[0..data_len-1], bandwidth sd * (3n/4)^(-1/5),\n"
                "evaluated at the centres of n_steps equal bins spanning [min, max] of the data. *out\n"
                "is a new n_steps x 2 galloc matrix, out[i][0] = x and out[i][1] = density; free with\n"
                "gfree.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 106: tld.h, stats */
                "tld_kde_count_pdf",
                "tld.h",
                "stats",
                "Descriptive statistics of double arrays (mean, variance, covariance, Pearson correlation, Shannon entropy, standardising), the sigmoid, the normal density and Gaussian kernel density estimates. Results come back through the last argument.",
                "int tld_kde_count_pdf(double *data,double* cnt, int data_len, double*** out);",
                "Weighted kernel density estimate: value data[i] seen cnt[i] (>= 0) times. The density\n"
                "is evaluated at each data[i]: *out is a new data_len x 2 galloc matrix, out[i][0] =\n"
                "data[i] and out[i][1] = density; free with gfree.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 107: tld.h, quantile */
                "tld_quantile_normalise",
                "tld.h",
                "quantile",
                "Quantile normalisation of the columns of a matrix.",
                "int tld_quantile_normalise(double** data, int n, int m);",
                "Quantile-normalise, in place, the m columns of the n x m matrix data (data[i][j] is\n"
                "row i, column j; e.g. from galloc(&data, n, m)). Afterwards every column has the same\n"
                "distribution: the mean over columns of the sorted values. Tied values in a column\n"
                "share the mean of their positions. NaN is an error.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 108: tld.h, auc */
                "tld_auc",
                "tld.h",
                "auc",
                "Area under the ROC curve, and the best score threshold, for 0/1 labels and real-valued scores.",
                "int tld_auc(double *Y, double *Y_hat, int n, double *t, double *auc);",
                "ROC analysis of n samples: Y[i] is the label (1.0 positive; any other value in [0, 1]\n"
                "negative), Y_hat[i] the score (higher means more likely positive; no NaN). Both\n"
                "classes must occur. *auc is the AUC (trapezoids, tied scores form one step) and *t\n"
                "the score nearest the (0, 1) corner of the curve: call Y_hat >= *t positive. Either\n"
                "output may be NULL when it is not wanted:",
                "double y[4] = {1, 1, 0, 0}, score[4] = {0.9, 0.4, 0.5, 0.1}, auc;\n"
                "RUN(tld_auc(y, score, 4, NULL, &auc));      // auc is 0.75",
                TLD_DOC_ENTRY
        },
        {       /* 109: tld.h, logsum */
                "tld_logsum_init",
                "tld.h",
                "logsum",
                "Arithmetic on probabilities stored as logarithms (log-sum-exp via a lookup table) and tolerant floating-point equality.",
                "void tld_logsum_init(void);",
                "Fill logsum's lookup table. Optional (logsum does it on first use); thread-safe.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 110: tld.h, logsum */
                "tld_logsum",
                "tld.h",
                "logsum",
                "Arithmetic on probabilities stored as logarithms (log-sum-exp via a lookup table) and tolerant floating-point equality.",
                "float tld_logsum(const float a,const float b);",
                "log(exp(a) + exp(b)) for log-space values, from a table with steps of 0.001. NaN if\n"
                "either is NaN; -INFINITY acts as log(0); returns max(a, b) when they differ by 15.7 or\n"
                "more.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 111: tld.h, logsum */
                "tld_nearly_equal_float, tld_nearly_equal_double",
                "tld.h",
                "logsum",
                "Arithmetic on probabilities stored as logarithms (log-sum-exp via a lookup table) and tolerant floating-point equality.",
                "int tld_nearly_equal_float(float a, float b);\n"
                "int tld_nearly_equal_double(double a, double b);",
                "1 if a and b are equal up to a relative difference of FLT_EPSILON / DBL_EPSILON (an\n"
                "absolute one when either is 0 or both are tiny), else 0.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 112: tld.h, logsum */
                "TLD_NEARLY_EQUAL",
                "tld.h",
                "logsum",
                "Arithmetic on probabilities stored as logarithms (log-sum-exp via a lookup table) and tolerant floating-point equality.",
                "#define TLD_NEARLY_EQUAL(X,Y) _Generic((X),                        \\\n"
                "                                float: tld_nearly_equal_float,  \\\n"
                "                                double: tld_nearly_equal_double \\\n"
                "                )(X,Y)",
                "TLD_NEARLY_EQUAL(X, Y): tld_nearly_equal_float or tld_nearly_equal_double, chosen by the type of X\n"
                "(float or double only).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 113: tld.h, kdtree */
                "tld_kdtree",
                "tld.h",
                "kdtree",
                "A k-d tree of double vectors with caller-chosen ids for nearest-neighbour search by Euclidean distance; points can be built in bulk or inserted one at a time. Not locked: changing a tree (alloc, build, insert, rebalance, reset) needs it to itself, but searches only read it and may run at the same time.",
                "typedef struct tld_kdtree tld_kdtree;",
                "Opaque tree; create with tld_kdtree_alloc, free with tld_kdtree_free.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 114: tld.h, kdtree */
                "tld_kdtree_alloc",
                "tld.h",
                "kdtree",
                "A k-d tree of double vectors with caller-chosen ids for nearest-neighbour search by Euclidean distance; points can be built in bulk or inserted one at a time. Not locked: changing a tree (alloc, build, insert, rebalance, reset) needs it to itself, but searches only read it and may run at the same time.",
                "int tld_kdtree_alloc(tld_kdtree **tree, int dim);",
                "Allocate in *tree an empty tree for points of dim (> 0) doubles. Storage grows as\n"
                "points are added. Free with tld_kdtree_free.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 115: tld.h, kdtree */
                "tld_kdtree_free",
                "tld.h",
                "kdtree",
                "A k-d tree of double vectors with caller-chosen ids for nearest-neighbour search by Euclidean distance; points can be built in bulk or inserted one at a time. Not locked: changing a tree (alloc, build, insert, rebalance, reset) needs it to itself, but searches only read it and may run at the same time.",
                "void tld_kdtree_free(tld_kdtree *t);",
                "Free the tree and all its points; NULL is ignored.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 116: tld.h, kdtree */
                "tld_kdtree_reset",
                "tld.h",
                "kdtree",
                "A k-d tree of double vectors with caller-chosen ids for nearest-neighbour search by Euclidean distance; points can be built in bulk or inserted one at a time. Not locked: changing a tree (alloc, build, insert, rebalance, reset) needs it to itself, but searches only read it and may run at the same time.",
                "int tld_kdtree_reset(tld_kdtree *t);",
                "Remove every point and keep the memory for reuse.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 117: tld.h, kdtree */
                "tld_kdtree_build",
                "tld.h",
                "kdtree",
                "A k-d tree of double vectors with caller-chosen ids for nearest-neighbour search by Euclidean distance; points can be built in bulk or inserted one at a time. Not locked: changing a tree (alloc, build, insert, rebalance, reset) needs it to itself, but searches only read it and may run at the same time.",
                "int tld_kdtree_build(tld_kdtree *t, double **x, uint32_t *ids, int n);",
                "Add n points x[i] (each of the tree's dim doubles) with ids[i] and rebuild the tree\n"
                "balanced. The points are copied; x and ids are not changed. On failure (no memory)\n"
                "the tree is unchanged.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 118: tld.h, kdtree */
                "tld_kdtree_insert",
                "tld.h",
                "kdtree",
                "A k-d tree of double vectors with caller-chosen ids for nearest-neighbour search by Euclidean distance; points can be built in bulk or inserted one at a time. Not locked: changing a tree (alloc, build, insert, rebalance, reset) needs it to itself, but searches only read it and may run at the same time.",
                "int tld_kdtree_insert(tld_kdtree *t, double *x, uint32_t id);",
                "Insert one point x (dim doubles) with id (copied). Every 10000 insertions the tree is\n"
                "rebuilt if its depth is more than 10 levels off the balanced depth. On failure (no\n"
                "memory) the tree is unchanged.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 119: tld.h, kdtree */
                "tld_kdtree_rebalance",
                "tld.h",
                "kdtree",
                "A k-d tree of double vectors with caller-chosen ids for nearest-neighbour search by Euclidean distance; points can be built in bulk or inserted one at a time. Not locked: changing a tree (alloc, build, insert, rebalance, reset) needs it to itself, but searches only read it and may run at the same time.",
                "int tld_kdtree_rebalance(tld_kdtree *t);",
                "Rebuild the tree balanced from the points it holds (allocates nothing).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 120: tld.h, kdtree */
                "tld_kdtree_nn_search",
                "tld.h",
                "kdtree",
                "A k-d tree of double vectors with caller-chosen ids for nearest-neighbour search by Euclidean distance; points can be built in bulk or inserted one at a time. Not locked: changing a tree (alloc, build, insert, rebalance, reset) needs it to itself, but searches only read it and may run at the same time.",
                "int tld_kdtree_nn_search(tld_kdtree *t, double *x, uint32_t *id);",
                "The id of the stored point nearest to x (dim doubles) into *id. FAIL (with an error)\n"
                "when the tree is empty.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 121: tld.h, kdtree */
                "tld_kdtree_bf_search",
                "tld.h",
                "kdtree",
                "A k-d tree of double vectors with caller-chosen ids for nearest-neighbour search by Euclidean distance; points can be built in bulk or inserted one at a time. Not locked: changing a tree (alloc, build, insert, rebalance, reset) needs it to itself, but searches only read it and may run at the same time.",
                "int tld_kdtree_bf_search(tld_kdtree *t, double *x, uint32_t *id);",
                "Brute force: the id of the stored point nearest to x, comparing every point; FAIL\n"
                "(with an error) when the tree is empty. For checking tld_kdtree_nn_search.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 122: tld.h, bpm */
                "tld_bpm",
                "tld.h",
                "bpm",
                "Approximate string search: where a pattern occurs in a text with at most k edits (substitutions, insertions, deletions), by Myers' bit-parallel algorithm, for patterns and texts of any length. AVX2 or NEON kernels are chosen at run time and give the same results as the plain ones. A compiled pattern is read-only: threads may share it.",
                "typedef struct tld_bpm tld_bpm;",
                "A compiled pattern (opaque). Create with tld_bpm_alloc, free with tld_bpm_free. A\n"
                "search only reads it, so several threads may search with one pattern at once.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 123: tld.h, bpm */
                "tld_bpm_hit",
                "tld.h",
                "bpm",
                "Approximate string search: where a pattern occurs in a text with at most k edits (substitutions, insertions, deletions), by Myers' bit-parallel algorithm, for patterns and texts of any length. AVX2 or NEON kernels are chosen at run time and give the same results as the plain ones. A compiled pattern is read-only: threads may share it.",
                "typedef struct {\n"
                "        int64_t start;\n"
                "        int64_t end;\n"
                "        int dist;\n"
                "} tld_bpm_hit;",
                "One match: text[start, end) aligns to the pattern with dist edits (its Levenshtein\n"
                "distance). start == end only when dist is the pattern length (every byte deleted).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 124: tld.h, bpm */
                "TLD_BPM_NOCASE, TLD_BPM_PLAIN",
                "tld.h",
                "bpm",
                "Approximate string search: where a pattern occurs in a text with at most k edits (substitutions, insertions, deletions), by Myers' bit-parallel algorithm, for patterns and texts of any length. AVX2 or NEON kernels are chosen at run time and give the same results as the plain ones. A compiled pattern is read-only: threads may share it.",
                "#define TLD_BPM_NOCASE 1\n"
                "#define TLD_BPM_PLAIN 2",
                "Flags for tld_bpm_alloc. TLD_BPM_NOCASE: an ASCII letter in the pattern matches\n"
                "either case in the text (other bytes match only themselves). TLD_BPM_PLAIN: never use\n"
                "the AVX2 or NEON kernels; results are the same, so this is for tests and timing.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 125: tld.h, bpm */
                "tld_bpm_alloc",
                "tld.h",
                "bpm",
                "Approximate string search: where a pattern occurs in a text with at most k edits (substitutions, insertions, deletions), by Myers' bit-parallel algorithm, for patterns and texts of any length. AVX2 or NEON kernels are chosen at run time and give the same results as the plain ones. A compiled pattern is read-only: threads may share it.",
                "int tld_bpm_alloc(tld_bpm **bpm, const char *pattern, int len, int flags);",
                "Compile the len (> 0) bytes at pattern into *bpm; the bytes may be anything, including\n"
                "0, and are copied. flags is 0 or TLD_BPM_NOCASE and TLD_BPM_PLAIN or'ed together. The\n"
                "pattern takes about 16 bytes per distinct byte for every 64 bytes of length (a\n"
                "10000-byte DNA pattern about 13 KB). Free with tld_bpm_free.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 126: tld.h, bpm */
                "tld_bpm_free",
                "tld.h",
                "bpm",
                "Approximate string search: where a pattern occurs in a text with at most k edits (substitutions, insertions, deletions), by Myers' bit-parallel algorithm, for patterns and texts of any length. AVX2 or NEON kernels are chosen at run time and give the same results as the plain ones. A compiled pattern is read-only: threads may share it.",
                "void tld_bpm_free(tld_bpm *b);",
                "Free a compiled pattern; NULL is ignored.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 127: tld.h, bpm */
                "tld_bpm_best",
                "tld.h",
                "bpm",
                "Approximate string search: where a pattern occurs in a text with at most k edits (substitutions, insertions, deletions), by Myers' bit-parallel algorithm, for patterns and texts of any length. AVX2 or NEON kernels are chosen at run time and give the same results as the plain ones. A compiled pattern is read-only: threads may share it.",
                "int tld_bpm_best(const tld_bpm *b, const char *text, int64_t len, tld_bpm_hit *hit);",
                "The best match of the pattern in the len (>= 0) bytes at text: the smallest distance;\n"
                "of the matches with that distance the first one tld_bpm_find would return for k = that\n"
                "distance. An empty text gives {0, 0, pattern length}. Time grows with\n"
                "len * (pattern length / 64), plus a pass over at most twice the pattern length.",
                "tld_bpm *b = NULL;\n"
                "tld_bpm_hit h;\n"
                "const char text[] = \"a call to tld_json_get_string\";\n"
                "RUN(tld_bpm_alloc(&b, \"tld_jsn_get_str\", 15, 0));\n"
                "RUN(tld_bpm_best(b, text, (int64_t)(sizeof text - 1), &h));\n"
                "ASSERT(h.dist == 1 && h.start == 10 && h.end == 26, \"got %d at [%lld, %lld)\", h.dist,\n"
                "       (long long)h.start, (long long)h.end);\n"
                "tld_bpm_free(b);",
                TLD_DOC_ENTRY
        },
        {       /* 128: tld.h, bpm */
                "tld_bpm_find",
                "tld.h",
                "bpm",
                "Approximate string search: where a pattern occurs in a text with at most k edits (substitutions, insertions, deletions), by Myers' bit-parallel algorithm, for patterns and texts of any length. AVX2 or NEON kernels are chosen at run time and give the same results as the plain ones. A compiled pattern is read-only: threads may share it.",
                "int tld_bpm_find(const tld_bpm *b, const char *text, int64_t len, int k, tld_bpm_hit **hits, int64_t *n_hits);",
                "Every match with at most k (>= 0) edits in the len (>= 0) bytes at text, in order, none\n"
                "overlapping another: *hits (which must be NULL) gets an array of *n_hits matches, freed\n"
                "with MFREE, or stays NULL when there are none. A match ends where the distance is\n"
                "smallest locally and starts where the longest stretch of text with that distance does;\n"
                "where two such matches overlap, the one with the smaller distance is kept (then the\n"
                "one that starts first, then the longer one), deciding from left to right. k above the\n"
                "pattern length is taken as the pattern length. An empty text has no matches.",
                "tld_bpm *b = NULL;\n"
                "tld_bpm_hit *hits = NULL;\n"
                "int64_t n = 0;\n"
                "const char text[] = \"colour, color or colr\";\n"
                "RUN(tld_bpm_alloc(&b, \"color\", 5, 0));\n"
                "RUN(tld_bpm_find(b, text, (int64_t)(sizeof text - 1), 1, &hits, &n));\n"
                "ASSERT(n == 3, \"got %lld matches\", (long long)n);\n"
                "ASSERT(hits[1].start == 8 && hits[1].end == 13 && hits[1].dist == 0, \"not color\");\n"
                "ASSERT(hits[2].start == 17 && hits[2].end == 21 && hits[2].dist == 1, \"not colr\");\n"
                "MFREE(hits);\n"
                "tld_bpm_free(b);",
                TLD_DOC_ENTRY
        },
        {       /* 129: tld.h, bpm */
                "tld_bpm_distance",
                "tld.h",
                "bpm",
                "Approximate string search: where a pattern occurs in a text with at most k edits (substitutions, insertions, deletions), by Myers' bit-parallel algorithm, for patterns and texts of any length. AVX2 or NEON kernels are chosen at run time and give the same results as the plain ones. A compiled pattern is read-only: threads may share it.",
                "int tld_bpm_distance(const tld_bpm *b, const char *text, int64_t len, int *dist);",
                "The Levenshtein distance between the pattern and all len (>= 0) bytes at text, into\n"
                "*dist: the fewest substitutions, insertions and deletions that turn one into the other\n"
                "(with TLD_BPM_NOCASE, letters that differ only in case are equal). An empty text gives\n"
                "the pattern length. len is at most INT32_MAX minus the pattern length. To compare one\n"
                "string with many, compile it once and call this for each of the others; for a pattern\n"
                "of at most 64 bytes the call allocates nothing.",
                "tld_bpm *b = NULL;\n"
                "int d = -1;\n"
                "RUN(tld_bpm_alloc(&b, \"kitten\", 6, 0));\n"
                "RUN(tld_bpm_distance(b, \"sitting\", 7, &d));\n"
                "ASSERT(d == 3, \"got %d\", d);\n"
                "tld_bpm_free(b);",
                TLD_DOC_ENTRY
        },
        {       /* 130: tld.h, bpm */
                "tld_bpm_kernel",
                "tld.h",
                "bpm",
                "Approximate string search: where a pattern occurs in a text with at most k edits (substitutions, insertions, deletions), by Myers' bit-parallel algorithm, for patterns and texts of any length. AVX2 or NEON kernels are chosen at run time and give the same results as the plain ones. A compiled pattern is read-only: threads may share it.",
                "const char *tld_bpm_kernel(const tld_bpm *b);",
                "The kernel searches with b run: \"word\" (pattern of at most 64 bytes), \"block\" (longer\n"
                "patterns), \"avx2\" or \"neon\" (long patterns, when the CPU has the instructions and\n"
                "TLD_BPM_PLAIN was not given). The string is static; NULL b gives NULL.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 131: tld.h, tpool */
                "tld_thread_pool",
                "tld.h",
                "tpool",
                "A pthread pool. Jobs func(pool, data, start, end, thread_id) are queued with tld_thread_pool_add and run in FIFO order by a fixed set of worker threads; optional time limit and a program-wide stop (tld_thread_pool_stop_all).",
                "typedef struct tld_thread_pool tld_thread_pool;",
                "Opaque pool; create with tld_thread_pool_alloc, free with tld_thread_pool_free.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 132: tld.h, tpool */
                "tld_thread_pool_stop_all",
                "tld.h",
                "tpool",
                "A pthread pool. Jobs func(pool, data, start, end, thread_id) are queued with tld_thread_pool_add and run in FIFO order by a fixed set of worker threads; optional time limit and a program-wide stop (tld_thread_pool_stop_all).",
                "void tld_thread_pool_stop_all(void);",
                "Stop every pool, running and future, for the rest of the program: workers stop\n"
                "between jobs and drop queued jobs, and tld_thread_pool_add refuses new ones. The\n"
                "library installs no signal handlers; this only sets an atomic flag, so a program may\n"
                "call it from its own handler (e.g. on SIGINT).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 133: tld.h, tpool */
                "tld_thread_pool_alloc",
                "tld.h",
                "tpool",
                "A pthread pool. Jobs func(pool, data, start, end, thread_id) are queued with tld_thread_pool_add and run in FIFO order by a fixed set of worker threads; optional time limit and a program-wide stop (tld_thread_pool_stop_all).",
                "int  tld_thread_pool_alloc(tld_thread_pool **pool, double max_time, int n_threads);",
                "Start a pool of n_threads workers (at least 2) in *pool. max_time is a limit in\n"
                "minutes since creation, checked after each job: once passed, the pool stops and\n"
                "queued jobs are dropped. 0 means no limit.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 134: tld.h, tpool */
                "tld_thread_pool_add",
                "tld.h",
                "tpool",
                "A pthread pool. Jobs func(pool, data, start, end, thread_id) are queued with tld_thread_pool_add and run in FIFO order by a fixed set of worker threads; optional time limit and a program-wide stop (tld_thread_pool_stop_all).",
                "int tld_thread_pool_add(tld_thread_pool *p, void (*func_ptr)(tld_thread_pool*,void *,int64_t, int64_t,int), void *data, int64_t start, int64_t end);",
                "Queue a job: a worker calls func_ptr(p, data, start, end, thread_id), thread_id in\n"
                "[0, n_threads). start and end are passed through (e.g. a slice of an index range).\n"
                "Thread-safe; may be called from inside a job. Once the pool has stopped (time limit\n"
                "or tld_thread_pool_stop_all) the job is refused: FAIL without a message, and data is still\n"
                "the caller's to free.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 135: tld.h, tpool */
                "tld_thread_pool_wait",
                "tld.h",
                "tpool",
                "A pthread pool. Jobs func(pool, data, start, end, thread_id) are queued with tld_thread_pool_add and run in FIFO order by a fixed set of worker threads; optional time limit and a program-wide stop (tld_thread_pool_stop_all).",
                "int tld_thread_pool_wait(tld_thread_pool *p);",
                "Block until the queue is empty and no job is running (after a stop: until every\n"
                "worker has exited). OK when every job added to the pool has run; FAIL, without a\n"
                "message, when a stop refused or abandoned jobs (they did not run, so whatever they\n"
                "would have done or freed did not happen). A running pool can take more jobs\n"
                "afterwards. NULL is ignored (OK).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 136: tld.h, tpool */
                "tld_thread_pool_free",
                "tld.h",
                "tpool",
                "A pthread pool. Jobs func(pool, data, start, end, thread_id) are queued with tld_thread_pool_add and run in FIFO order by a fixed set of worker threads; optional time limit and a program-wide stop (tld_thread_pool_stop_all).",
                "void tld_thread_pool_free(tld_thread_pool *p);",
                "Run every job still queued, then stop and join the workers and free the pool. NULL\n"
                "is ignored.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 137: tld_ai.h, general */
                "AI_VERSION",
                "tld_ai.h",
                "general",
                "",
                "#define AI_VERSION \"0.4.0\"",
                "Version of tld_ai.h.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 138: tld_ai.h, status codes (ai_status) */
                "AI_OK, AI_ENOSERVER, AI_EHTTP, AI_EPARSE, AI_ESCHEMA, AI_ENOMODEL, AI_EIO, AI_ENOMEM, AI_EARG",
                "tld_ai.h",
                "status codes (ai_status)",
                "",
                "enum {\n"
                "    AI_OK = 0,\n"
                "    AI_ENOSERVER, /* no Ollama server answers at TLD_AI_HOST                       */\n"
                "    AI_EHTTP,     /* https host, timeout, truncated reply, HTTP error status    */\n"
                "    AI_EPARSE,    /* the reply is not valid JSON or lacks the expected fields   */\n"
                "    AI_ESCHEMA,   /* valid JSON that does not fit the struct, labels or range   */\n"
                "    AI_ENOMODEL,  /* the model is not installed on the server                   */\n"
                "    AI_EIO,       /* cache, review or image file could not be read or written   */\n"
                "    AI_ENOMEM,    /* out of memory                                              */\n"
                "    AI_EARG       /* invalid argument (NULL format, n <= 0, ...)                */\n"
                "};",
                "Status codes: ai_status() returns the one for this thread's last call.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 139: tld_ai.h, options */
                "ai_f, ai_i, AI_F, AI_I",
                "tld_ai.h",
                "options",
                "",
                "typedef struct { int set; double v; } ai_f;   /* optional double: unset is not 0 */\n"
                "typedef struct { int set; long   v; } ai_i;   /* optional integer */\n"
                "#define AI_F(x) ((ai_f){1, (x)})\n"
                "#define AI_I(x) ((ai_i){1, (x)})",
                "Optional numbers for ai_opts and ai_config: .set = 1 marks a value as given, so an\n"
                "unset field differs from 0. Write AI_F(0.2) and AI_I(7).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 140: tld_ai.h, options */
                "ai_opts",
                "tld_ai.h",
                "options",
                "",
                "typedef struct {\n"
                "    const char *model;            /* overrides TLD_AI_LLM / TLD_AI_DECIDER / TLD_AI_EMBEDDER */\n"
                "    const char *system;           /* system prompt (LLM calls) */\n"
                "    ai_f temperature, top_p, min_p;\n"
                "    ai_i top_k, seed, num_ctx, num_predict;\n"
                "    const char *const *stop;      /* NULL-terminated stop sequences */\n"
                "    const char *think;            /* \"true\", \"false\", \"low\", \"medium\", \"high\" */\n"
                "    const char *keep_alive;       /* \"10m\", \"0\", ... */\n"
                "    const char *const *images;    /* NULL-terminated image file paths */\n"
                "    const char *extra_options;    /* raw JSON object merged into \"options\" */\n"
                "    ai_i dimensions;              /* embeddings: output dimensions, if the model allows */\n"
                "    int no_truncate;              /* embeddings: fail instead of truncating long input */\n"
                "    int retries;                  /* extra attempts on malformed output (0 = default 2) */\n"
                "    int fresh;                    /* 1 = bypass the cache for this call */\n"
                "    ai_i max_retries;             /* extra attempts, 0 allowed; overrides .retries */\n"
                "} ai_opts;",
                "Per-call options for the *_with calls; every field is optional (0 / NULL / unset).\n"
                "Text calls (ai_text, ai_fill) use all fields except dimensions and no_truncate, with\n"
                "unset fields taken from ai_defaults(.opts). Decisions (ai_yesno, ai_choose, ai_rate)\n"
                "use only model, images and fresh. Embeddings use model, dimensions, no_truncate,\n"
                "fresh and the \"options\" fields (temperature .. num_predict, stop, extra_options),\n"
                "think and keep_alive. retries and max_retries apply to ai_fill.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 141: tld_ai.h, options */
                "AI_OPTS",
                "tld_ai.h",
                "options",
                "",
                "#define AI_OPTS(...) (&(const ai_opts){__VA_ARGS__})",
                "AI_OPTS(.temperature = AI_F(0), .seed = AI_I(7)) -> const ai_opts *, pointing to a\n"
                "compound literal that lives until the end of the enclosing block.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 142: tld_ai.h, program-set defaults */
                "ai_config, AI_CONFIG",
                "tld_ai.h",
                "program-set defaults",
                "",
                "typedef struct {\n"
                "    const char *host;      /* TLD_AI_HOST      e.g. \"http://192.168.1.20:11434\" (http:// only) */\n"
                "    const char *llm;       /* TLD_AI_LLM       text model: ai_text, ai_fill                  */\n"
                "    const char *decider;   /* TLD_AI_DECIDER   decision model: ai_yesno, ai_choose, ai_rate  */\n"
                "    const char *embedder;  /* TLD_AI_EMBEDDER  embedding model: ai_embed, ai_embed_all       */\n"
                "    const char *db;        /* TLD_AI_DB        cache file, or \"off\"                          */\n"
                "    const char *reviews;   /* TLD_AI_REVIEWS   review directory                              */\n"
                "    ai_i num_ctx;          /* TLD_AI_NUM_CTX   context length for LLM and embedding calls    */\n"
                "    ai_i timeout;          /* TLD_AI_TIMEOUT   seconds per request                           */\n"
                "    ai_i fresh;            /* TLD_AI_FRESH     1 = ignore the cache                          */\n"
                "    ai_i embed_dimensions; /* default output dimensions for embeddings, if the model allows */\n"
                "    const ai_opts *opts;   /* default options for text calls (system, temperature, seed,\n"
                "                              stop, think, keep_alive, retries, ...); decisions take only\n"
                "                              .fresh from it, embeddings .dimensions, .no_truncate, .fresh */\n"
                "} ai_config;\n"
                "#define AI_CONFIG(...) (&(const ai_config){__VA_ARGS__})",
                "Program-wide defaults for ai_defaults, written AI_CONFIG(.llm = \"...\", ...). Precedence,\n"
                "per setting: per-call options > environment > ai_defaults() > built-in. The\n"
                "environment is read once, at the first call into tld_ai.h.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 143: tld_ai.h, program-set defaults */
                "ai_defaults",
                "tld_ai.h",
                "program-set defaults",
                "",
                "int ai_defaults(const ai_config *c);",
                "Set program-wide defaults: the fields c sets replace the built-in defaults; NULL resets\n"
                "to the built-ins. Each call replaces the previous call's settings; strings and .opts are\n"
                "copied. 0 on success, -1 with ai_status()/ai_error(); an invalid call (host not\n"
                "http://, empty name, number <= 0) changes nothing. Calls in flight keep their settings.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 144: tld_ai.h, plain calls */
                "ai_text, ai_text_with",
                "tld_ai.h",
                "plain calls",
                "",
                "const char *ai_text(const char *fmt, ...) __attribute__((format(printf, 1, 2)));\n"
                "const char *ai_text_with(const ai_opts *o, const char *fmt, ...) __attribute__((format(printf, 2, 3)));",
                "Text from the LLM (TLD_AI_LLM, or o->model) for the printf-style prompt fmt. The\n"
                "string is library memory, valid until ai_reset() (never free it); NULL on failure.\n"
                "ai_text_with takes per-call options (o may be NULL).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 145: tld_ai.h, plain calls */
                "ai_yesno, ai_yesno_with",
                "tld_ai.h",
                "plain calls",
                "",
                "double      ai_yesno(const char *question, const char *state);\n"
                "double      ai_yesno_with(const ai_opts *o, const char *question, const char *state);",
                "Probability in [0, 1] that the answer to question about state is yes, from the\n"
                "decision model (TLD_AI_DECIDER, or o->model); -1 on failure. A state that starts\n"
                "with '{' and parses as JSON is sent as a JSON object, anything else as text.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 146: tld_ai.h, embeddings */
                "ai_vec, ai_vecs",
                "tld_ai.h",
                "embeddings",
                "",
                "typedef struct { int dim; const float *x; } ai_vec;              /* x == NULL on failure */\n"
                "typedef struct { int n, dim; const float *x; } ai_vecs;          /* row i at x + i*dim */",
                "An embedding of dim floats at x (x == NULL on failure), and n embeddings with row i at\n"
                "x + i*dim. The floats are library memory, valid until ai_reset(); never free them.\n"
                "Vectors from different embedding models are not comparable.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 147: tld_ai.h, embeddings */
                "ai_embed, ai_embed_with",
                "tld_ai.h",
                "embeddings",
                "",
                "ai_vec  ai_embed(const char *text);\n"
                "ai_vec  ai_embed_with(const ai_opts *o, const char *text);",
                "The embedding of text from the embedding model (TLD_AI_EMBEDDER, or o->model); .x is\n"
                "NULL on failure. Long input is truncated unless o->no_truncate is set.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 148: tld_ai.h, embeddings */
                "ai_embed_all, ai_embed_all_with",
                "tld_ai.h",
                "embeddings",
                "",
                "ai_vecs ai_embed_all(const char *const *texts, int n);           /* one request, n rows */\n"
                "ai_vecs ai_embed_all_with(const ai_opts *o, const char *const *texts, int n);",
                "Embeddings of texts[0..n-1] in one request, as n rows; .x is NULL on failure.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 149: tld_ai.h, embeddings */
                "ai_row",
                "tld_ai.h",
                "embeddings",
                "",
                "#define ai_row(vs, i) ((ai_vec){(vs).dim, (vs).x + (size_t)(i) * (vs).dim})",
                "Row i of the ai_vecs vs as an ai_vec (no copy, no bounds check).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 150: tld_ai.h, embeddings */
                "ai_cosine",
                "tld_ai.h",
                "embeddings",
                "",
                "double ai_cosine(ai_vec a, ai_vec b);                            /* -2 if dims differ */",
                "Cosine similarity of a and b, in [-1, 1]; 0 if either is all zeros; -2 if the\n"
                "dimensions differ or either .x is NULL.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 151: tld_ai.h, embeddings */
                "ai_nearest",
                "tld_ai.h",
                "embeddings",
                "",
                "int    ai_nearest(ai_vec q, ai_vecs m, int k, int *idx, double *score);",
                "The k rows of m most similar to q (cosine), best first: row indices into idx and\n"
                "similarities into score, two arrays of at least k elements. Returns how many were\n"
                "written (fewer than k when m has fewer rows; 0 on bad input).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 152: tld_ai.h, typed structs filled by an LLM */
                "ai_kind, ai_field, ai_type, ai_label, ai_labels",
                "tld_ai.h",
                "typed structs filled by an LLM",
                "",
                "typedef enum { AI_T_str, AI_T_num, AI_T_int, AI_T_bool } ai_kind;\n"
                "typedef struct { const char *name; ai_kind kind; size_t off; const char *desc; } ai_field;\n"
                "typedef struct { const char *name; size_t size; const ai_field *f; int n; } ai_type;\n"
                "typedef struct { const char *name; const char *desc; } ai_label;\n"
                "typedef struct { const char *name; const char *question; const ai_label *labels; int n; } ai_labels;",
                "Descriptions of a record and of labels: a member's type (ai_kind), one member\n"
                "(ai_field), a record (ai_type), one choice or scale label (ai_label), and a question\n"
                "with its labels (ai_labels). AI_STRUCT, AI_CHOICE and AI_SCALE make them at compile\n"
                "time; a program that learns its schemas at run time (from a file, say) fills them\n"
                "itself and passes them to ai_fill_dyn, ai_choose_dyn and ai_rate_dyn. size and off\n"
                "place members in a C struct and are used only by ai_fill; ai_fill_dyn ignores them.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 153: tld_ai.h, typed structs filled by an LLM */
                "AI_STRUCT",
                "tld_ai.h",
                "typed structs filled by an LLM",
                "",
                "#define AI_STRUCT(name, FIELDS) ...",
                "AI_STRUCT(name, FIELDS) defines struct name for ai_fill. FIELDS is an X-macro of\n"
                "X(kind, member, \"description\") entries; kind is str (const char *), num (double), int\n"
                "(long) or bool (int). The descriptions reach the model in a JSON schema.",
                "#define FIX(X) X(str, title, \"One-line summary\") X(num, risk, \"Risk 0 to 1\")\n"
                "AI_STRUCT(fix, FIX)      // struct fix { const char *title; double risk; };",
                TLD_DOC_ENTRY
        },
        {       /* 154: tld_ai.h, decisions: pick one label */
                "AI_CHOICE",
                "tld_ai.h",
                "decisions: pick one label",
                "",
                "#define AI_CHOICE(name, question, OPTIONS) ...",
                "AI_CHOICE(name, question, OPTIONS) defines enum name, one constant per label in order,\n"
                "and struct name_answer { int ok; enum name value; double confidence; double p[]; }\n"
                "(p: one probability per label), for ai_choose. OPTIONS is an X-macro of\n"
                "X(label, \"description\") entries. Labels become bare enum constants, so two choices\n"
                "cannot share a label (the compiler reports the clash).",
                "#define TEAM(X) X(bug, \"Software defects\") X(billing, \"Payments and refunds\")\n"
                "AI_CHOICE(team, \"Which team should handle this?\", TEAM)",
                TLD_DOC_ENTRY
        },
        {       /* 155: tld_ai.h, decisions: place on an ordered scale */
                "AI_SCALE",
                "tld_ai.h",
                "decisions: place on an ordered scale",
                "",
                "#define AI_SCALE(name, question, LEVELS) ...",
                "AI_SCALE(name, question, LEVELS) defines enum name, one constant per level from lowest\n"
                "to highest, and struct name_answer { int ok; enum name level; double score; double\n"
                "confidence; }, for ai_rate. LEVELS is an X-macro of X(level, \"description\") entries.\n"
                "score is the model's probability-weighted position, 0 (lowest level) to n-1; level is\n"
                "score rounded to the nearest level.\n"
                "\n"
                "How many levels a scale may have depends on the decision model, and models do not\n"
                "advertise it: autotrust-jev (the default) needs exactly 6. A count the model rejects\n"
                "fails with AI_EHTTP and the model's own message in ai_error(), e.g. 'question\n"
                "\"urgency\": score criteria must contain exactly 6 levels'.",
                "#define URGENCY(X) X(none, \"No action\") X(someday, \"Within a month\")              \\\n"
                "                   X(routine, \"Within a week\") X(soon, \"This week\")               \\\n"
                "                   X(urgent, \"Today\") X(critical, \"Many customers, now\")\n"
                "AI_SCALE(urgency, \"How urgent is this?\", URGENCY)",
                TLD_DOC_ENTRY
        },
        {       /* 156: tld_ai.h, the calls a program makes */
                "ai_fill, ai_fill_with",
                "tld_ai.h",
                "the calls a program makes",
                "",
                "#define ai_fill(out, T, ...) \\\n"
                "    (TLD__LITERAL(__VA_ARGS__), ai__fill(AI__CHECK(out, T), T##__type(), NULL, __VA_ARGS__))\n"
                "#define ai_fill_with(o, out, T, ...) \\\n"
                "    (TLD__LITERAL(__VA_ARGS__), ai__fill(AI__CHECK(out, T), T##__type(), (o), __VA_ARGS__))",
                "ai_fill(&s, T, fmt, ...): ask the LLM to fill s, a struct T made by AI_STRUCT, for the\n"
                "printf-style task fmt. The struct's JSON schema is appended to the prompt; a reply\n"
                "that does not fit is retried (o->retries, default 2 extra attempts). 0 on success, -1\n"
                "on failure with s zeroed. Strings in s are library memory until ai_reset(); never free\n"
                "them. ai_fill_with(o, &s, T, fmt, ...) takes per-call options.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 157: tld_ai.h, the calls a program makes */
                "ai_choose, ai_choose_with",
                "tld_ai.h",
                "the calls a program makes",
                "",
                "#define ai_choose(T, state)             T##__choose((state), NULL)\n"
                "#define ai_choose_with(o, T, state)     T##__choose((state), (o))",
                "ai_choose(T, state): ask the decision model which label of the choice T (AI_CHOICE)\n"
                "fits state; returns struct T_answer (struct team_answer for T = team): .ok (0 on\n"
                "failure), .value, .confidence and .p[label]. A state that starts with '{' and parses as JSON is sent as JSON.\n"
                "ai_choose_with(o, T, state) takes options (model, images, fresh).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 158: tld_ai.h, the calls a program makes */
                "ai_rate, ai_rate_with",
                "tld_ai.h",
                "the calls a program makes",
                "",
                "#define ai_rate(T, state)               T##__rate((state), NULL)\n"
                "#define ai_rate_with(o, T, state)       T##__rate((state), (o))",
                "ai_rate(T, state): place state on the scale T (AI_SCALE); returns struct T_answer:\n"
                ".ok (0 on failure), .score (the model's score), .level (score rounded to the nearest\n"
                "level and clamped to the scale) and .confidence. ai_rate_with(o, T, state) takes\n"
                "options (model, images, fresh).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 159: tld_ai.h, descriptions built at run time */
                "ai_fill_dyn, ai_fill_dyn_with",
                "tld_ai.h",
                "descriptions built at run time",
                "",
                "const char *ai_fill_dyn(const ai_type *t, const char *fmt, ...) __attribute__((format(printf, 2, 3)));\n"
                "const char *ai_fill_dyn_with(const ai_opts *o, const ai_type *t, const char *fmt, ...) __attribute__((format(printf, 3, 4)));",
                "ai_fill_dyn(t, fmt, ...): ai_fill for a record described at run time. t is an ai_type\n"
                "the program filled (size and each off are ignored); the result is the reply, checked\n"
                "against t, as compact JSON text: one object with exactly t's members, in t's order,\n"
                "typed as declared (str a string, num a number, int an integer, bool true/false).\n"
                "Library memory until ai_reset(); NULL on failure. The text can be passed on as the\n"
                "state of a decision, which sends it as JSON. Retries as ai_fill does.\n"
                "\n"
                "Checked before anything is sent (AI_EARG): t->name and every member name are\n"
                "non-empty and made of letters, digits, '_' and '-'; t->n >= 1; member names are\n"
                "distinct; every kind is one of ai_kind.\n"
                "\n"
                "ai_fill_dyn_with(o, t, fmt, ...) takes per-call options, as ai_fill_with.",
                "ai_field f[] = { {.name = \"title\", .kind = AI_T_str, .desc = \"One-line summary\"},\n"
                "                 {.name = \"risk\",  .kind = AI_T_num, .desc = \"Risk 0 to 1\"} };\n"
                "ai_type t = { .name = \"fix\", .f = f, .n = 2 };\n"
                "const char *j = ai_fill_dyn(&t, \"Propose a fix for:\\n%s\", \"The app crashes on start\");\n"
                "// j: {\"title\":\"...\",\"risk\":0.2}, or NULL",
                TLD_DOC_ENTRY
        },
        {       /* 160: tld_ai.h, descriptions built at run time */
                "ai_pick",
                "tld_ai.h",
                "descriptions built at run time",
                "",
                "typedef struct { int ok; int value; double confidence; const double *p; } ai_pick;",
                "The answer of ai_choose_dyn: .ok (0 on failure, when the rest is 0 and p NULL), the\n"
                "index of the chosen label in .value, .confidence, and one probability per label in p\n"
                "(n doubles, library memory until ai_reset()).",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 161: tld_ai.h, descriptions built at run time */
                "ai_rating",
                "tld_ai.h",
                "descriptions built at run time",
                "",
                "typedef struct { int ok; int level; double score; double confidence; } ai_rating;",
                "The answer of ai_rate_dyn: .ok (0 on failure), .score (0 for the first label to n-1\n"
                "for the last), .level (score rounded to the nearest level and clamped) and\n"
                ".confidence.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 162: tld_ai.h, descriptions built at run time */
                "ai_choose_dyn, ai_choose_dyn_with, ai_rate_dyn, ai_rate_dyn_with",
                "tld_ai.h",
                "descriptions built at run time",
                "",
                "ai_pick   ai_choose_dyn(const ai_labels *c, const char *state);\n"
                "ai_pick   ai_choose_dyn_with(const ai_opts *o, const ai_labels *c, const char *state);\n"
                "ai_rating ai_rate_dyn(const ai_labels *c, const char *state);\n"
                "ai_rating ai_rate_dyn_with(const ai_opts *o, const ai_labels *c, const char *state);",
                "ai_choose_dyn(c, state): ai_choose for labels described at run time; c->name names the\n"
                "decision, c->question is asked, and c->labels[0..n-1] are the choices.\n"
                "ai_rate_dyn(c, state): ai_rate likewise, the labels ordered lowest to highest (the\n"
                "default decision model needs exactly 6; see AI_SCALE).\n"
                "\n"
                "Checked before anything is sent (AI_EARG): c->name and every label name are non-empty\n"
                "and made of letters, digits, '_' and '-'; label names are distinct; c->question and\n"
                "every description are non-NULL; c->n >= 2.\n"
                "\n"
                "The _with forms take options (model, images, fresh), as ai_choose_with.",
                "ai_label L[] = { {\"bug\", \"A software defect\"}, {\"billing\", \"Payments and refunds\"} };\n"
                "ai_labels team = { .name = \"team\", .question = \"Which team?\", .labels = L, .n = 2 };\n"
                "ai_pick a = ai_choose_dyn(&team, \"The app crashes on start\");\n"
                "if (a.ok) printf(\"%s %.2f\\n\", L[a.value].name, a.confidence);",
                TLD_DOC_ENTRY
        },
        {       /* 163: tld_ai.h, asking a person */
                "ai_review",
                "tld_ai.h",
                "asking a person",
                "",
                "const char *ai_review(const char *what, const char *payload);",
                "Ask a person (see Human review above). The first call writes TLD_AI_REVIEWS/<key>.json, the\n"
                "key a hash of what and payload, and returns NULL with ai_status() == AI_OK; so does\n"
                "every call while the review is open. Once a person has set \"status\" to \"done\" and\n"
                "\"answer\" to a string, the call returns that answer (library memory). NULL with\n"
                "another status on errors.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 164: tld_ai.h, errors and the last call */
                "ai_error",
                "tld_ai.h",
                "errors and the last call",
                "",
                "const char *ai_error(void);",
                "The message about this thread's last call: why it failed, or empty after a success\n"
                "(ai_review leaves a note while a review is open). Valid until the thread's next call.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 165: tld_ai.h, errors and the last call */
                "ai_status",
                "tld_ai.h",
                "errors and the last call",
                "",
                "int         ai_status(void);",
                "The status code (AI_OK, AI_ENOSERVER, ...) of this thread's last call.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 166: tld_ai.h, errors and the last call */
                "ai_info",
                "tld_ai.h",
                "errors and the last call",
                "",
                "typedef struct {\n"
                "    const char *thinking;         /* model's thinking text, if requested */\n"
                "    long   prompt_tokens, output_tokens;\n"
                "    double seconds;               /* wall time of the HTTP call, 0 if cached */\n"
                "    int    from_cache;\n"
                "    const char *model;            /* model that answered */\n"
                "    const char *host;             /* server that was asked */\n"
                "} ai_info;",
                "Details of a request to the server, from ai_last(). Strings are library memory.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 167: tld_ai.h, errors and the last call */
                "ai_last",
                "tld_ai.h",
                "errors and the last call",
                "",
                "const ai_info *ai_last(void);",
                "Details of the last request this thread sent (or tried to send) to the server.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 168: tld_ai.h, library memory */
                "ai_fmt",
                "tld_ai.h",
                "library memory",
                "",
                "const char *ai_fmt(const char *fmt, ...) __attribute__((format(printf, 1, 2)));",
                "sprintf into library memory: the formatted string, valid until ai_reset() (never free\n"
                "it); NULL on failure.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 169: tld_ai.h, library memory */
                "ai_reset",
                "tld_ai.h",
                "library memory",
                "",
                "void        ai_reset(void);",
                "Free every string and vector the library has returned, in all threads. Call it only\n"
                "when no thread still uses them; the library keeps working afterwards.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 170: tld_ai.h, parallel work */
                "ai_parallel",
                "tld_ai.h",
                "parallel work",
                "",
                "void ai_parallel(int n, int threads, void (*fn)(int i, void *ctx), void *ctx);",
                "Run fn(i, ctx) for i = 0..n-1 on `threads` threads and wait for all. The calling\n"
                "thread works too; the calls run concurrently, in no fixed order, and may themselves\n"
                "call tld_ai.h.",
                NULL,
                TLD_DOC_ENTRY
        },
        {       /* 171: tld.h, Usage */
                "tld.h: Usage",
                "tld.h",
                "",
                "",
                "",
                "In exactly one .c file of your program, before any other #include:\n"
                "\n"
                "and plain #include \"tld.h\" everywhere else. C11 plus POSIX (pthreads, stat,\n"
                "realpath); macOS and Linux. Build: cc -std=c11 prog.c -lm -lpthread.",
                "#define TLD_IMPLEMENTATION\n"
                "#include \"tld.h\"",
                TLD_DOC_SECTION
        },
        {       /* 172: tld.h, Errors */
                "tld.h: Errors",
                "tld.h",
                "",
                "",
                "",
                "A function that can fail returns int: OK (0) on success, FAIL (1) on failure, with\n"
                "its results in pointer arguments; one that returns a pointer returns NULL on\n"
                "failure. Failures print an ERROR line (time, message, file and line) to stderr;\n"
                "lookups that just find nothing (tld_hash_get, tld_json_get_*, ...) return FAIL\n"
                "silently. Yes/no questions (tld_path_exists, tld_path_is_dir, tld_nearly_equal_*,\n"
                "tld_term_has_color) return 1 or 0, and a few functions return a plain value; each\n"
                "one says so. Nothing exits or aborts.\n"
                "\n"
                "A NULL pointer argument is an error unless the function's comment allows it: an\n"
                "ERROR line and the failure value (FAIL, NULL, 0 for a 1/0 answer, NaN for a double\n"
                "result, -1 for an index). The free functions ignore NULL, like free().\n"
                "\n"
                "RUN(call), ASSERT(test, fmt, ...), ERROR_MSG(fmt, ...) and the allocation macros\n"
                "MMALLOC, MREALLOC, MMALLOC_ALIGNED and galloc jump to a label named ERROR in the\n"
                "calling function on failure. The usual shape of a function:\n"
                "\n"
                "Initialise every pointer to NULL: MMALLOC refuses a non-NULL pointer, and the\n"
                "cleanup after ERROR: may run before an allocation happened.\n"
                "\n"
                "LOG_MSG, WARNING_MSG, ERROR_MSG, ASSERT and the rich-output calls take a printf\n"
                "format, checked by the compiler. Print text from users or files with \"%s\", never as\n"
                "the format itself: LOG_MSG(\"%s\", line), not LOG_MSG(line).",
                "int load(const char *path, tld_strbuf **out)\n"
                "{\n"
                "        tld_strbuf *b = NULL;\n"
                "        ASSERT(path != NULL, \"No path\");\n"
                "        RUN(tld_strbuf_alloc(&b, 256));\n"
                "        RUN(tld_strbuf_append(b, path));\n"
                "        *out = b;\n"
                "        return OK;\n"
                "ERROR:\n"
                "        tld_strbuf_free(b);\n"
                "        return FAIL;\n"
                "}",
                TLD_DOC_SECTION
        },
        {       /* 173: tld.h, Memory */
                "tld.h: Memory",
                "tld.h",
                "",
                "",
                "",
                "Objects come from an allocator (tld_strbuf_alloc(&b, ...), tld_hash_alloc,\n"
                "tld_kdtree_alloc, tld_rng_alloc, tld_json_parse, TLD_JSON_OBJECT, ...) and belong\n"
                "to the caller, who frees them with the matching free function (tld_strbuf_free,\n"
                "tld_hash_free, tld_kdtree_free, tld_rng_free, tld_json_obj_free, ...); every free\n"
                "function accepts NULL.\n"
                "galloc memory is freed with gfree, MMALLOC memory with MFREE, never with free();\n"
                "both set the pointer to NULL, so freeing again on an error path is harmless.\n"
                "Pointers handed out by an object (tld_hash_get, tld_json_get_str, ...) are borrowed\n"
                "and const: valid until that object changes or is freed; never write through them\n"
                "or free them. Sizes are int-based: a string buffer holds at most INT32_MAX - 1 bytes\n"
                "and refuses to grow past that.",
                NULL,
                TLD_DOC_SECTION
        },
        {       /* 174: tld.h, Threads */
                "tld.h: Threads",
                "tld.h",
                "",
                "",
                "",
                "Objects have no locks: use each one from one thread at a time. The tld_thread_pool_*\n"
                "calls and tld_logsum_init are thread-safe; tld_progress and the TLD_START_TIMER\n"
                "timer keep global state.",
                NULL,
                TLD_DOC_SECTION
        },
        {       /* 175: tld.h, Environment */
                "tld.h: Environment",
                "tld.h",
                "",
                "",
                "",
                "TLD_VERBOSE  0 errors only, 1 normal (default), 2 also DEBUG_MSG lines\n"
                "NO_COLOR     set and non-empty: no colour on stderr (likewise TERM=dumb)",
                NULL,
                TLD_DOC_SECTION
        },
        {       /* 176: tld.h, Limitations */
                "tld.h: Limitations",
                "tld.h",
                "",
                "",
                "",
                "JSON integers that fit in 32 bits are stored exactly; larger ones are stored as\n"
                "double, exact only up to 2^53 (9007199254740992), so bigger values lose precision.\n"
                "JSON parsing is strict (RFC 8259): True, NaN, leading zeros and trailing commas are\n"
                "rejected. JSON keys containing '.' or '[' cannot be addressed by path.\n"
                "tld_progress keeps one bar at a time and is not for concurrent use from threads.\n"
                "POSIX only (macOS, Linux, other Unix); not Windows except through WSL.",
                NULL,
                TLD_DOC_SECTION
        },
        {       /* 177: tld_ai.h, Usage */
                "tld_ai.h: Usage",
                "tld_ai.h",
                "",
                "",
                "",
                "In exactly one .c file, before any other #include:\n"
                "\n"
                "and plain #include \"tld_ai.h\" everywhere else. The program must compile\n"
                "TLD_IMPLEMENTATION in some file (tld_ai.h uses tld's JSON, strings and hash map).\n"
                "Build: cc -std=c11 prog.c -lpthread -lm. Nothing else: no libcurl, no SQLite, no\n"
                "cJSON (HTTP, JSON, cache and reviews are done here and in tld.h).\n"
                "\n"
                "C11 plus POSIX (sockets, poll, flock, pthreads); macOS and Linux. In the\n"
                "AI_IMPLEMENTATION file tld_ai.h asks for POSIX.1-2008 itself (it defines\n"
                "_POSIX_C_SOURCE, _DEFAULT_SOURCE and _DARWIN_C_SOURCE when they are not set), so\n"
                "nothing needs -D flags. That works when tld_ai.h comes before every system header in\n"
                "that file; if a system header came first, tld_ai.h supplies the few declarations a\n"
                "strict -std=c11 build hides.",
                "#define TLD_IMPLEMENTATION      // unless another file already compiles tld.h\n"
                "#define AI_IMPLEMENTATION\n"
                "#include \"tld_ai.h\"                 // includes \"tld.h\" itself if TLD_H is not defined",
                TLD_DOC_SECTION
        },
        {       /* 178: tld_ai.h, Environment */
                "tld_ai.h: Environment",
                "tld_ai.h",
                "",
                "",
                "",
                "All optional; read once, at the first call into tld_ai.h, so a later setenv() has\n"
                "no effect.\n"
                "\n"
                "TLD_AI_HOST      Ollama base URL                      default http://localhost:11434\n"
                "                 (plain http only; https:// fails with AI_EHTTP)\n"
                "TLD_AI_LLM       default LLM                          default inkling-2-mini:latest\n"
                "TLD_AI_DECIDER   default decision model               default autotrust-jev:latest\n"
                "TLD_AI_EMBEDDER  default embedding model              default embeddinggemma-2:latest\n"
                "TLD_AI_NUM_CTX   context length sent with every LLM and embedding call that does not\n"
                "                 set .num_ctx                         default: not sent (server decides)\n"
                "TLD_AI_TIMEOUT   seconds per HTTP request             default 300\n"
                "TLD_AI_FRESH     1 = ignore the cache                 default 0\n"
                "TLD_AI_DB        cache file (JSON lines)              default: no cache; \"off\" turns\n"
                "                 off a cache set with ai_defaults(.db = ...)\n"
                "TLD_AI_REVIEWS   directory for ai_review files        default ai-reviews\n"
                "\n"
                "TLD_AI_FRESH accepts 1/0, true/false, yes/no, on/off (any case); TLD_AI_NUM_CTX and\n"
                "TLD_AI_TIMEOUT must be positive integers. A value that does not parse is ignored with a\n"
                "warning.\n"
                "\n"
                "TLD_AI_TIMEOUT bounds a whole request: loading the model and generating every token.\n"
                "On a slow machine, or with a thinking model that writes long answers, raise it (for\n"
                "example TLD_AI_TIMEOUT=1800). Two waits stay short whatever it says: opening the\n"
                "connection gives up after 5 s, and listing the server's models after 30 s.",
                NULL,
                TLD_DOC_SECTION
        },
        {       /* 179: tld_ai.h, Defaults set in code */
                "tld_ai.h: Defaults set in code",
                "tld_ai.h",
                "",
                "",
                "",
                "Each field that is set (a non-NULL string, an ai_i with .set, a non-NULL .opts)\n"
                "replaces the built-in default; an environment variable, if set, still wins, and\n"
                "per-call options win over both. Each call replaces the previous one's settings;\n"
                "ai_defaults(NULL) resets to the built-ins. Strings and .opts are copied. Invalid\n"
                "input (a host that is not http://, an empty model name, a number <= 0) changes\n"
                "nothing and returns -1. db is read when the cache is first opened, so a later\n"
                "change has no effect on that run; reviews is read by each ai_review call. .opts is\n"
                "merged field by field under each call's options, for text calls only (its .model\n"
                "is ignored); embeddings take .embed_dimensions as their default dimensions. Calls\n"
                "in flight keep the settings they started with. Vectors from different embedders\n"
                "are not comparable: programs that store vectors should store the embedder's name\n"
                "next to them.",
                "ai_defaults(AI_CONFIG(.host = \"http://192.168.1.20:11434\", .llm = \"inkling-2-mini\",\n"
                "                      .decider = \"autotrust-jev\", .embedder = \"embeddinggemma-2\",\n"
                "                      .opts = AI_OPTS(.temperature = AI_F(0.2))));",
                TLD_DOC_SECTION
        },
        {       /* 180: tld_ai.h, Errors */
                "tld_ai.h: Errors",
                "tld_ai.h",
                "",
                "",
                "",
                "Failure values: NULL, -1, .x == NULL or .ok == 0, as documented per call. Nothing\n"
                "exits or aborts. ai_status() gives this thread's last code (AI_OK, AI_ENOSERVER,\n"
                "AI_EHTTP, ...) and ai_error() the message. Every call that needs the server probes\n"
                "GET /api/version first; with no server every such call fails fast with\n"
                "AI_ENOSERVER, and the probe is retried at most every 5 s. Failures print nothing;\n"
                "warnings (an ignored environment value, a cache line that cannot be read or\n"
                "written, too many kept results) and the review notices are tld.h WARNING and LOG\n"
                "lines on stderr, so TLD_VERBOSE=0 silences them.",
                NULL,
                TLD_DOC_SECTION
        },
        {       /* 181: tld_ai.h, Server memory */
                "tld_ai.h: Server memory",
                "tld_ai.h",
                "",
                "",
                "",
                "On a machine that can hold only one model, start Ollama with\n"
                "OLLAMA_MAX_LOADED_MODELS=1; the server then swaps models safely between requests.",
                NULL,
                TLD_DOC_SECTION
        },
        {       /* 182: tld_ai.h, Cloud models */
                "tld_ai.h: Cloud models",
                "tld_ai.h",
                "",
                "",
                "",
                "Opt-in only. A cloud model is one whose /api/tags entry has \"remote_host\"; it is\n"
                "reached through the local server like any other (tld_ai.h does no HTTPS). The defaults\n"
                "are local models, so prompts leave the machine only when a cloud model is named in\n"
                "TLD_AI_LLM, TLD_AI_DECIDER, TLD_AI_EMBEDDER or .model. A server that is not signed in answers\n"
                "with an HTTP error, which comes back as AI_EHTTP with the server's message.",
                NULL,
                TLD_DOC_SECTION
        },
        {       /* 183: tld_ai.h, Cache and replay */
                "tld_ai.h: Cache and replay",
                "tld_ai.h",
                "",
                "",
                "",
                "Every answered request is appended to TLD_AI_DB as one JSON line\n"
                "{key, endpoint, model, digest, request, response, seconds, created}, under\n"
                "flock(LOCK_EX), so several processes can share the file. The key is FNV-1a 64 of\n"
                "endpoint, model digest and request body, plus \"#n\" for the n-th identical request\n"
                "in the process; a rerun therefore replays finished calls in order, and a retry is\n"
                "a new call. n advances only when an answer is obtained, so a transient failure\n"
                "does not shift the replay. The digest stops an updated model from replaying old\n"
                "answers. TLD_AI_FRESH=1 or .fresh = 1 skips the lookup (the answer is still stored).",
                NULL,
                TLD_DOC_SECTION
        },
        {       /* 184: tld_ai.h, Human review */
                "tld_ai.h: Human review",
                "tld_ai.h",
                "",
                "",
                "",
                "ai_review(what, payload) writes TLD_AI_REVIEWS/<key>.json =\n"
                "{\"what\":..., \"payload\":..., \"status\":\"open\", \"answer\":null} and returns NULL (with\n"
                "ai_status() == AI_OK) while the review is open. A person edits the file, sets\n"
                "\"status\" to \"done\" and \"answer\" to a string; the next run returns that answer.",
                NULL,
                TLD_DOC_SECTION
        },
        {       /* 185: tld_ai.h, Memory */
                "tld_ai.h: Memory",
                "tld_ai.h",
                "",
                "",
                "",
                "Strings, vectors and the strings in an AI_STRUCT belong to the library: they are\n"
                "const, so the compiler warns when one is passed to free(). They stay valid until\n"
                "ai_reset() or exit, which means memory grows with every call. A program that calls\n"
                "in a loop should call ai_reset() each round, once it has used or copied that round's\n"
                "results. When 100000 values are kept without an ai_reset(), the library prints a\n"
                "warning to stderr.",
                NULL,
                TLD_DOC_SECTION
        },
        {       /* 186: tld_ai.h, Threads */
                "tld_ai.h: Threads",
                "tld_ai.h",
                "",
                "",
                "",
                "Every function may be called from any thread. Call ai_reset() only when no thread\n"
                "still uses returned values. Error text, status and ai_last() are per thread.",
                NULL,
                TLD_DOC_SECTION
        }
};
/* ---- END CORPUS ---- */

int tld_doc_n_entries(void)
{
        return TLD__DOC_N_ENTRIES;
}

const tld_doc_entry *tld_doc_entry_at(int i)
{
        ASSERT(i >= 0 && i < TLD__DOC_N_ENTRIES, "Corpus entry %d is outside 0..%d", i,
               TLD__DOC_N_ENTRIES - 1);
        return &tld__doc_corpus[i];
ERROR:
        return NULL;
}

const char *tld_doc_corpus_id(void)
{
        return TLD__DOC_CORPUS_ID;
}

/* ---- the word search ----------------------------------------------------
 *
 * The rules:
 *
 *   1. Words. The question is split into words of ASCII letters, digits and '_'; every
 *      other byte separates words. At most TLD__DOC_MAX_WORDS distinct words are used,
 *      each cut to TLD__DOC_MAX_WORD bytes; the head of the text names the first
 *      TLD__DOC_MAX_IGNORED of the words left out. A word is an identifier when it contains
 *      '_' or is exactly (with case) one of the corpus names ("MMALLOC", "galloc").
 *      Other words are lower-cased; a word in tld__doc_stopwords, or of fewer than 3
 *      bytes, is dropped.
 *   2. Stems. A plain word is searched as its stem: one suffix is taken off, "ation",
 *      "ing", "ies", "ed" or "es" when at least 4 bytes remain, or "s" when at least 3
 *      remain and it does not follow 's', 'u' or 'i'; then a final "e" when at least 4
 *      bytes remain ("parse" -> "pars", "allocation" -> "alloc", "tags" -> "tag",
 *      "class", "status" and "file" stay). An identifier is searched whole.
 *   3. Matches. The stem is compiled with tld_bpm (TLD_BPM_NOCASE) and searched with
 *      tld_bpm_find in each field of each record: the names; the declaration with the
 *      doc text; the example; and two module fields, the module name and the module
 *      description. Every record of a module shares the module fields, so they say how
 *      well the module fits, not which of its records does: they are scored without
 *      length normalisation (b = 0), so they move a module's records together and never
 *      reorder them. A match counts only where a word of the text starts (after a
 *      byte that is not a letter or digit, so "json" matches in "tld_json_parse" and
 *      "pars" in "parsed", but "paris" not in "comparison"); the number of such matches
 *      is the word's frequency in that field.
 *   4. Near matches. A word with no exact or name match anywhere is searched again with
 *      k = 1 edit (k = 2 from 12 bytes): first its stem, when the stem has at least 6
 *      bytes ("alocations" -> "alocation" finds "allocation"); then, when that finds
 *      nothing, the word as written (lower case), when it has at least 6 bytes and differs
 *      from its stem (a misspelled "alocation" stems to "aloc"). The matches starting
 *      within one byte of a word start then count as its frequencies, and the head shows
 *      the nearest form: the first word (letters, digits and '_') of the closest match,
 *      with its own edit distance; a match whose first word is more than k edits away
 *      gives no form.
 *   5. Names. An identifier equal to a name of a record (ignoring case when it contains
 *      '_') is a name match there; otherwise one within d edits of a name, d * 6 <= its
 *      length and d <= 3 (tld_bpm_distance), is a close-name match.
 *   6. Ranking: BM25F (Robertson and Zaragoza 2009) with k1 = 1.2 and b = 0.75. A
 *      field's length is its number of words (word starts), normalised by the average
 *      over the records (b = 0 for the module fields, rule 3); the names field weighs
 *      TLD__DOC_W_NAMES (3), the others 1. idf = log(1 + (N - df + 0.5) / (df + 0.5)),
 *      df = the records the word matches. A name
 *      match adds 1000 and a close-name match 500 to the score. Hits are the top_n
 *      records with a score above 0, best first (ties: manual order).
 */

#define TLD__DOC_MAX_WORDS 16           /* distinct words searched per question */
#define TLD__DOC_MAX_WORD 32            /* bytes of a word that are searched */
#define TLD__DOC_MAX_QUESTION 200       /* bytes of the question the text repeats */
#define TLD__DOC_MAX_IGNORED 8          /* words not searched that the head names */
#define TLD__DOC_MAX_SEE 8              /* names a See also line gives */
#define TLD__DOC_N_FIELDS 5
#define TLD__DOC_W_NAMES 3.0
#define TLD__DOC_K1 1.2
#define TLD__DOC_B 0.75

static const char *const tld__doc_stopwords[] = {
        "a", "about", "after", "all", "also", "am", "an", "and", "any", "are", "as", "at",
        "be", "been", "being", "but", "by", "can", "could", "did", "do", "does", "doing",
        "for", "from", "had", "has", "have", "having", "he", "her", "here", "him", "his",
        "how", "i", "if", "in", "into", "is", "it", "its", "just", "let", "lets", "like",
        "me", "might", "must", "my", "myself", "need", "of", "on", "or", "our", "out",
        "please", "she", "should", "so", "some", "than", "that", "the", "their", "them",
        "then", "there", "these", "they", "this", "those", "to", "too", "use", "using",
        "very", "via", "want", "was", "way", "we", "were", "what", "when", "where",
        "which", "while", "who", "why", "will", "with", "would", "you", "your", NULL
};

/* One searched word of a question. */
typedef struct {
        char word[TLD__DOC_MAX_WORD + 1];       /* as searched for names (case kept) */
        char low[TLD__DOC_MAX_WORD + 1];        /* lower case */
        char stem[TLD__DOC_MAX_WORD + 1];       /* the pattern of the word search */
        char near[TLD__DOC_MAX_WORD + 5];       /* the nearest form (rule 4), "" if none */
        int near_dist;
        int ident;
        int df;
        int found;                              /* an exact or name match somewhere */
} tld__doc_word;

/* How word j matched record r. */
typedef struct {
        unsigned short tf[TLD__DOC_N_FIELDS];
        unsigned char name;             /* 2 name match, 1 close-name match, 0 none */
        unsigned char near;             /* the frequencies are near matches (rule 4) */
} tld__doc_match;

/* A question and its search results. */
typedef struct {
        char question[TLD__DOC_MAX_QUESTION + 5];       /* as the text repeats it */
        tld__doc_word w[TLD__DOC_MAX_WORDS];
        int n_words;
        char ignored[TLD__DOC_MAX_IGNORED][TLD__DOC_MAX_WORD + 1];      /* not searched */
        int n_ignored;
        int more_ignored;                       /* more words than ignored holds */
        tld__doc_match m[TLD__DOC_N_ENTRIES][TLD__DOC_MAX_WORDS];
        double len[TLD__DOC_N_ENTRIES][TLD__DOC_N_FIELDS];      /* words per field */
        double score[TLD__DOC_N_ENTRIES];
} tld__doc_query;

static int tld__doc_alnum(int c)
{
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

static int tld__doc_lower(int c)
{
        return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
}

static int tld__doc_eq_nocase(const char *a, size_t na, const char *b, size_t nb)
{
        if(na != nb){
                return 0;
        }
        for(size_t i = 0; i < na; i++){
                if(tld__doc_lower((unsigned char)a[i]) != tld__doc_lower((unsigned char)b[i])){
                        return 0;
                }
        }
        return 1;
}

static int tld__doc_appendf(tld_strbuf *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static int tld__doc_appendf(tld_strbuf *b, const char *fmt, ...)
{
        char small[256];
        char *big = NULL;
        va_list ap;
        int n;
        va_start(ap, fmt);
        n = vsnprintf(small, sizeof small, fmt, ap);
        va_end(ap);
        ASSERT(n >= 0, "Could not format text");
        if((size_t)n < sizeof small){
                RUN(tld_strbuf_append_n(b, small, (size_t)n));
                return OK;
        }
        MMALLOC(big, (size_t)n + 1);
        va_start(ap, fmt);
        vsnprintf(big, (size_t)n + 1, fmt, ap);
        va_end(ap);
        RUN(tld_strbuf_append_n(b, big, (size_t)n));
        MFREE(big);
        return OK;
ERROR:
        MFREE(big);
        return FAIL;
}

/* The length of the well-formed UTF-8 sequence (RFC 3629) that starts the n > 0 bytes
 * at s, 0 when they do not start with one: no overlong form, no UTF-16 surrogate
 * (U+D800..U+DFFF), nothing above U+10FFFF. */
static size_t tld__doc_utf8_len(const unsigned char *s, size_t n)
{
        unsigned char c = s[0];
        unsigned char lo = 0x80;
        unsigned char hi = 0xbf;
        size_t need = c >= 0xf0 && c <= 0xf4 ? 4 : c >= 0xe0 && c <= 0xef ? 3 : c >= 0xc2 && c <= 0xdf ? 2 : 0;
        if(need == 0 || n < need){
                return 0;
        }
        /* the second byte's range: it rules out the overlong forms, the surrogates and
           what lies above U+10FFFF */
        if(c == 0xe0){
                lo = 0xa0;
        }else if(c == 0xed){
                hi = 0x9f;
        }else if(c == 0xf0){
                lo = 0x90;
        }else if(c == 0xf4){
                hi = 0x8f;
        }
        if(s[1] < lo || s[1] > hi){
                return 0;
        }
        for(size_t k = 2; k < need; k++){
                if((s[k] & 0xc0) != 0x80){
                        return 0;
                }
        }
        return need;
}

/* Copy the n bytes at s into d, for one line of text: control bytes become spaces, a
 * byte that is not part of well-formed UTF-8 becomes '?', and text longer than max
 * bytes is cut at a character boundary after at most max bytes and ends in " ...". d
 * must hold max + 5 bytes. */
static void tld__doc_clean(char *d, size_t max, const char *s, size_t n)
{
        size_t o = 0;
        size_t i = 0;
        while(i < n){
                unsigned char c = (unsigned char)s[i];
                size_t len = 1;
                if(c >= 0x80){
                        len = tld__doc_utf8_len((const unsigned char *)s + i, n - i);
                }
                if(o + (len ? len : 1) > max){
                        memcpy(d + o, " ...", 4);
                        o += 4;
                        break;
                }
                if(len == 0){
                        d[o++] = '?';
                        i++;
                }else if(len == 1){
                        d[o++] = (c < 0x20 || c == 0x7f) ? ' ' : (char)c;
                        i++;
                }else{
                        memcpy(d + o, s + i, len);
                        o += len;
                        i += len;
                }
        }
        d[o] = 0;
}

/* Part 0 or 1 of field f of record e (rule 3): 0 the names, 1 the declaration and the
 * doc text (both describe the entry), 2 the example, 3 the module name, 4 the module
 * description. */
static const char *tld__doc_field(const tld_doc_entry *e, int f, int part)
{
        switch(f){
        case 0: return part ? NULL : e->names;
        case 1: return part ? e->doc : e->decl;
        case 2: return part ? NULL : e->example;
        case 3: return part ? NULL : e->module;
        default: return part ? NULL : e->module_desc;
        }
}

/* The number of words (word starts) of s, 0 for NULL. */
static int tld__doc_n_words(const char *s)
{
        int n = 0;
        for(size_t i = 0; s && s[i]; i++){
                n += tld__doc_alnum((unsigned char)s[i]) && (i == 0 || !tld__doc_alnum((unsigned char)s[i - 1]));
        }
        return n;
}

/* Is the word w (n bytes) a name of record e? With nocase, case is ignored. */
static int tld__doc_is_name(const tld_doc_entry *e, const char *w, size_t n, int nocase)
{
        const char *s = e->names;
        while(*s){
                const char *c = strstr(s, ", ");
                size_t len = c ? (size_t)(c - s) : strlen(s);
                if(nocase ? tld__doc_eq_nocase(s, len, w, n) : (len == n && memcmp(s, w, n) == 0)){
                        return 1;
                }
                s += len + (c ? 2 : 0);
        }
        return 0;
}

/* The smallest edit distance from the compiled word b to a name of record e. */
static int tld__doc_name_distance(const tld_bpm *b, const tld_doc_entry *e, int *dist)
{
        const char *s = e->names;
        *dist = INT32_MAX;
        while(*s){
                const char *c = strstr(s, ", ");
                size_t len = c ? (size_t)(c - s) : strlen(s);
                int d = 0;
                RUN(tld_bpm_distance(b, s, (int64_t)len, &d));
                if(d < *dist){
                        *dist = d;
                }
                s += len + (c ? 2 : 0);
        }
        return OK;
ERROR:
        return FAIL;
}

static int tld__doc_stopword(const char *w)
{
        for(int i = 0; tld__doc_stopwords[i]; i++){
                if(strcmp(tld__doc_stopwords[i], w) == 0){
                        return 1;
                }
        }
        return 0;
}

static void tld__doc_stem(const char *w, char *stem)
{
        static const char *const suffix[] = {"ation", "ing", "ies", "ed", "es", "s", NULL};
        size_t n = strlen(w);
        memcpy(stem, w, n + 1);
        for(int i = 0; suffix[i]; i++){
                size_t k = strlen(suffix[i]);
                if(n >= k + (k == 1 ? 3 : 4) && strcmp(stem + n - k, suffix[i]) == 0 &&
                   !(k == 1 && (stem[n - 2] == 's' || stem[n - 2] == 'u' || stem[n - 2] == 'i'))){
                        n -= k;
                        stem[n] = 0;
                        break;
                }
        }
        if(n >= 5 && stem[n - 1] == 'e'){
                stem[--n] = 0;
        }
}

/* Split the question into q->w (rules 1 and 2). The words that would be searched but
 * come after the first TLD__DOC_MAX_WORDS go to q->ignored (the first
 * TLD__DOC_MAX_IGNORED of them; q->more_ignored is set when there are more). */
static void tld__doc_words(const char *question, tld__doc_query *q)
{
        const char *s = question;
        while(*s && !q->more_ignored){
                const char *t;
                size_t n;
                int has_us = 0;
                int has_alnum = 0;
                int dup = 0;
                int ident = 0;
                tld__doc_word x;
                while(*s && !(tld__doc_alnum((unsigned char)*s) || *s == '_')){
                        s++;
                }
                t = s;
                while(*s && (tld__doc_alnum((unsigned char)*s) || *s == '_')){
                        has_us |= *s == '_';
                        has_alnum |= tld__doc_alnum((unsigned char)*s);
                        s++;
                }
                n = (size_t)(s - t);
                if(n == 0 || !has_alnum){
                        continue;
                }
                if(n > TLD__DOC_MAX_WORD){
                        n = TLD__DOC_MAX_WORD;
                }
                ident = has_us;
                for(int r = 0; !ident && r < TLD__DOC_N_ENTRIES; r++){
                        ident = tld__doc_is_name(&tld__doc_corpus[r], t, n, 0);
                }
                memset(&x, 0, sizeof x);
                memcpy(x.word, t, n);
                for(size_t i = 0; i < n; i++){
                        x.low[i] = (char)tld__doc_lower((unsigned char)x.word[i]);
                }
                if(!ident && (n < 3 || tld__doc_stopword(x.low))){
                        continue;
                }
                for(int j = 0; j < q->n_words; j++){
                        if(strcmp(q->w[j].low, x.low) != 0){
                                continue;
                        }
                        dup = 1;
                        /* an identifier spelling wins over a plain word of the same
                           lower-case form, so the answer does not depend on which comes
                           first */
                        if(ident && !q->w[j].ident){
                                memcpy(q->w[j].word, x.word, sizeof x.word);
                                memcpy(q->w[j].stem, x.low, sizeof x.low);
                                q->w[j].ident = 1;
                        }
                }
                for(int j = 0; j < q->n_ignored; j++){
                        dup |= tld__doc_eq_nocase(q->ignored[j], strlen(q->ignored[j]), x.low, n);
                }
                if(dup){
                        continue;
                }
                if(q->n_words == TLD__DOC_MAX_WORDS){
                        if(q->n_ignored == TLD__DOC_MAX_IGNORED){
                                q->more_ignored = 1;
                        }else{
                                memcpy(q->ignored[q->n_ignored++], x.word, n + 1);
                        }
                        continue;
                }
                x.ident = ident;
                if(ident){
                        memcpy(x.stem, x.low, n + 1);
                }else{
                        tld__doc_stem(x.low, x.stem);
                }
                q->w[q->n_words++] = x;
        }
}

/* The nearest form of word w (rule 4) from a near match of b at t[st, end) with dist
 * edits: the first word (letters, digits and '_') of the span. A match can run over the
 * ", " between names or into the next word; then the word is compared again and kept only
 * when it is at most k edits from b, with that distance. It replaces w's form when it is
 * closer. */
static int tld__doc_near_form(tld__doc_word *w, const tld_bpm *b, const char *t, int64_t st, int64_t end,
                              int dist, int k)
{
        int64_t a = st;
        int64_t z;
        while(a < end && !(tld__doc_alnum((unsigned char)t[a]) || t[a] == '_')){
                a++;
        }
        z = a;
        while(z < end && (tld__doc_alnum((unsigned char)t[z]) || t[z] == '_')){
                z++;
        }
        if(z == a){
                return OK;
        }
        if(z < end || a > st){
                /* the word is not the whole span: its own distance (a word continuing past
                   the span, as "Complete" for a match "Comple", stays cut at the span) */
                RUN(tld_bpm_distance(b, t + a, z - a, &dist));
                if(dist > k){
                        return OK;
                }
        }
        if(dist < w->near_dist){
                w->near_dist = dist;
                tld__doc_clean(w->near, TLD__DOC_MAX_WORD, t + a, (size_t)(z - a));
        }
        return OK;
ERROR:
        return FAIL;
}

/* Count the matches of b (with up to k edits) of word j in every field of every record
 * (rules 3 and 4): with k = 0 those at a word start, with k > 0 the near ones starting
 * within one byte of a word start. Returns the number of records matched in *df. */
static int tld__doc_count(tld__doc_query *q, int j, const tld_bpm *b, int k, int *df)
{
        tld__doc_word *w = &q->w[j];
        tld_bpm_hit *hits = NULL;
        int64_t nh = 0;
        *df = 0;
        for(int r = 0; r < TLD__DOC_N_ENTRIES; r++){
                const tld_doc_entry *e = &tld__doc_corpus[r];
                int any = 0;
                for(int f = 0; f < TLD__DOC_N_FIELDS; f++){
                        unsigned cnt = 0;
                        for(int part = 0; part < 2; part++){
                                const char *t = tld__doc_field(e, f, part);
                                if(t == NULL || *t == 0){
                                        continue;
                                }
                                RUN(tld_bpm_find(b, t, (int64_t)strlen(t), k, &hits, &nh));
                                for(int64_t x = 0; x < nh; x++){
                                        int64_t st = hits[x].start;
                                        int at_start = st == 0 || !tld__doc_alnum((unsigned char)t[st - 1]);
                                        int near_start = at_start || st == 1 ||
                                                (st >= 2 && !tld__doc_alnum((unsigned char)t[st - 2]));
                                        if(k == 0 ? !at_start : !near_start){
                                                continue;
                                        }
                                        cnt++;
                                        if(k > 0 && hits[x].dist < w->near_dist){
                                                RUN(tld__doc_near_form(w, b, t, st, hits[x].end, hits[x].dist,
                                                                       k));
                                        }
                                }
                                MFREE(hits);
                                nh = 0;
                        }
                        cnt = cnt > 65535 ? 65535 : cnt;
                        q->m[r][j].tf[f] = (unsigned short)cnt;
                        q->m[r][j].near = k > 0;
                        any |= cnt > 0;
                }
                *df += any;
        }
        return OK;
ERROR:
        MFREE(hits);
        return FAIL;
}

/* Search word j of q in every record (rules 3 to 5). */
static int tld__doc_search_word(tld__doc_query *q, int j)
{
        tld__doc_word *w = &q->w[j];
        tld_bpm *b = NULL;
        size_t len = strlen(w->low);
        size_t sl = strlen(w->stem);
        int df = 0;
        w->near_dist = INT32_MAX;
        RUN(tld_bpm_alloc(&b, w->stem, (int)sl, TLD_BPM_NOCASE));
        RUN(tld__doc_count(q, j, b, 0, &df));
        w->found = df > 0;
        if(w->ident){
                size_t n = strlen(w->word);
                for(int r = 0; r < TLD__DOC_N_ENTRIES; r++){
                        const tld_doc_entry *e = &tld__doc_corpus[r];
                        int d = 0;
                        if(tld__doc_is_name(e, w->word, n, strchr(w->word, '_') != NULL)){
                                q->m[r][j].name = 2;
                                w->found = 1;
                                continue;
                        }
                        RUN(tld__doc_name_distance(b, e, &d));
                        if(d <= 3 && (size_t)d * 6 <= n){
                                q->m[r][j].name = 1;
                        }
                }
        }
        /* rule 4: the stem first, then the word as written (a misspelled word can stem to
           something short and garbled: "alocation" -> "aloc") */
        if(!w->found && sl >= 6){
                RUN(tld__doc_count(q, j, b, sl >= 12 ? 2 : 1, &df));
        }
        if(!w->found && df == 0 && len >= 6 && strcmp(w->low, w->stem) != 0){
                tld_bpm_free(b);
                b = NULL;
                RUN(tld_bpm_alloc(&b, w->low, (int)len, TLD_BPM_NOCASE));
                RUN(tld__doc_count(q, j, b, len >= 12 ? 2 : 1, &df));
        }
        w->df = df;
        tld_bpm_free(b);
        return OK;
ERROR:
        tld_bpm_free(b);
        return FAIL;
}

/* The search for question: q (the words, matches and scores) and, in a (zeroed), up to
 * top_n hits (rule 6). */
static int tld__doc_search(const char *question, int top_n, tld__doc_query *q, tld_doc_answer *a)
{
        static const double weight[TLD__DOC_N_FIELDS] = {TLD__DOC_W_NAMES, 1.0, 1.0, 1.0, 1.0};
        static const double bf[TLD__DOC_N_FIELDS] = {TLD__DOC_B, TLD__DOC_B, TLD__DOC_B, 0.0, 0.0};
        double avg[TLD__DOC_N_FIELDS] = {0};
        const double N = TLD__DOC_N_ENTRIES;
        int n = 0;
        tld__doc_clean(q->question, TLD__DOC_MAX_QUESTION, question, strlen(question));
        tld__doc_words(question, q);
        for(int r = 0; r < TLD__DOC_N_ENTRIES; r++){
                for(int f = 0; f < TLD__DOC_N_FIELDS; f++){
                        const tld_doc_entry *e = &tld__doc_corpus[r];
                        q->len[r][f] = tld__doc_n_words(tld__doc_field(e, f, 0)) +
                                tld__doc_n_words(tld__doc_field(e, f, 1));
                        avg[f] += q->len[r][f] / N;
                }
        }
        for(int j = 0; j < q->n_words; j++){
                RUN(tld__doc_search_word(q, j));
        }
        for(int r = 0; r < TLD__DOC_N_ENTRIES; r++){
                for(int j = 0; j < q->n_words; j++){
                        const tld__doc_match *m = &q->m[r][j];
                        double df = q->w[j].df;
                        double tf = 0.0;
                        for(int f = 0; f < TLD__DOC_N_FIELDS; f++){
                                double norm = avg[f] > 0.0 ? 1.0 - bf[f] + bf[f] * q->len[r][f] / avg[f] : 1.0;
                                tf += weight[f] * m->tf[f] / norm;
                        }
                        if(tf > 0.0){
                                q->score[r] += log(1.0 + (N - df + 0.5) / (df + 0.5)) * tf / (TLD__DOC_K1 + tf);
                        }
                        q->score[r] += m->name == 2 ? 1000.0 : m->name == 1 ? 500.0 : 0.0;
                }
                n += q->score[r] > 0.0;
        }
        n = n < top_n ? n : top_n;
        if(n > 0){
                MMALLOC(a->hits, sizeof(tld_doc_hit) * (size_t)n);
        }
        for(int i = 0; i < n; i++){
                int pick = -1;
                for(int r = 0; r < TLD__DOC_N_ENTRIES; r++){
                        int taken = 0;
                        if(q->score[r] <= 0.0){
                                continue;
                        }
                        for(int x = 0; x < i && !taken; x++){
                                taken = a->hits[x].entry == &tld__doc_corpus[r];
                        }
                        if(!taken && (pick < 0 || q->score[r] > q->score[pick])){
                                pick = r;
                        }
                }
                a->hits[i].entry = &tld__doc_corpus[pick];
                a->hits[i].score = q->score[pick];
        }
        a->n_hits = n;
        return OK;
ERROR:
        return FAIL;
}

/* ---- the answer text ---------------------------------------------------- */

/* Append text with every line indented by 4 spaces (empty lines stay empty) and a
 * final newline. */
static int tld__doc_indent(tld_strbuf *b, const char *s, size_t n)
{
        size_t i = 0;
        while(i < n){
                const char *nl = memchr(s + i, '\n', n - i);
                size_t len = nl ? (size_t)(nl - (s + i)) : n - i;
                if(len){
                        RUN(tld_strbuf_append(b, "    "));
                        RUN(tld_strbuf_append_n(b, s + i, len));
                }
                RUN(tld_strbuf_append_char(b, '\n'));
                i += len + 1;
        }
        return OK;
ERROR:
        return FAIL;
}

/* The length tld__doc_indent gives the n bytes at s. */
static long tld__doc_indent_len(const char *s, size_t n)
{
        long len = 0;
        size_t i = 0;
        while(i < n){
                const char *nl = memchr(s + i, '\n', n - i);
                size_t l = nl ? (size_t)(nl - (s + i)) : n - i;
                len += (long)l + (l ? 4 : 0) + 1;
                i += l + 1;
        }
        return len;
}

/* The names of e for a list: all of them up to 60 bytes, else those that fit in 56
 * and ", ...". */
static int tld__doc_short_names(tld_strbuf *b, const tld_doc_entry *e)
{
        size_t n = strlen(e->names);
        size_t cut = 0;
        if(n <= 60){
                return tld_strbuf_append(b, e->names);
        }
        for(size_t i = 0; i + 2 <= 56; i++){
                if(e->names[i] == ',' && e->names[i + 1] == ' '){
                        cut = i;
                }
        }
        RUN(tld_strbuf_append_n(b, e->names, cut));
        RUN(tld_strbuf_append(b, ", ..."));
        return OK;
ERROR:
        return FAIL;
}

/* Does a sentence end at the '.' s[i] (of n bytes)? At the end of the text, or when
 * spaces or newlines and then a capital letter follow. */
static int tld__doc_sentence_end(const char *s, size_t i, size_t n)
{
        size_t j = i + 1;
        if(j >= n){
                return 1;
        }
        if(s[j] != ' ' && s[j] != '\n'){
                return 0;
        }
        while(j < n && (s[j] == ' ' || s[j] == '\n')){
                j++;
        }
        return j >= n || (s[j] >= 'A' && s[j] <= 'Z');
}

/* The longest head tld__doc_head can write: 2213 bytes (tld_doc_ask documents at most
 * 2300, which the assertion below holds). The title and Question line: 25 + 1 bytes and
 * the question, at most TLD__DOC_MAX_QUESTION + 4 with " ..." (tld__doc_clean writes one
 * byte for each byte it replaces). The Words line is longest with no word found: 27 + 17 (" none. Not found:")
 * + 2 bytes, and per word ", " (one byte less for the first), the word (at most
 * TLD__DOC_MAX_WORD) and " (nearest: \"\", 2 edits)" (23, one digit as k <= 2) around a
 * nearest form of at most TLD__DOC_MAX_WORD + 4. The Not searched line: 13 + 9 (" and
 * more") + 64 bytes, and per word ", " and the word. The closing line: at most 93. */
#define TLD__DOC_MAX_HEAD (25 + TLD__DOC_MAX_QUESTION + 4 + 1 + \
        27 + 17 + 2 + TLD__DOC_MAX_WORDS * (2 + TLD__DOC_MAX_WORD + 23 + TLD__DOC_MAX_WORD + 4) - 1 + \
        13 + 9 + 64 + TLD__DOC_MAX_IGNORED * (2 + TLD__DOC_MAX_WORD) - 1 + 93)
_Static_assert(TLD__DOC_MAX_HEAD <= 2300, "the head can be longer than the 2300 bytes tld_doc_ask documents");

/* The head: title, question, the facts of the search, the words not searched. */
static int tld__doc_head(tld_strbuf *b, const tld_doc_answer *a, const tld__doc_query *q)
{
        int found = 0;
        RUN(tld__doc_appendf(b, "tld_doc answer\nQuestion: %s\n", q->question));
        for(int j = 0; j < q->n_words; j++){
                found += q->w[j].found;
        }
        if(q->n_words == 0){
                RUN(tld_strbuf_append(b, "The question has no words to search for (only common words, or "
                                      "words shorter than 3 letters).\n"));
        }else{
                RUN(tld_strbuf_append(b, "Words found in the manuals:"));
                for(int j = 0, k = 0; j < q->n_words; j++){
                        if(q->w[j].found){
                                RUN(tld__doc_appendf(b, "%s %s", k++ ? "," : "", q->w[j].word));
                        }
                }
                RUN(tld_strbuf_append(b, found ? ". Not found:" : " none. Not found:"));
                for(int j = 0, k = 0; j < q->n_words; j++){
                        const tld__doc_word *w = &q->w[j];
                        if(w->found){
                                continue;
                        }
                        RUN(tld__doc_appendf(b, "%s %s", k++ ? "," : "", w->word));
                        if(w->near[0]){
                                RUN(tld__doc_appendf(b, " (nearest: \"%s\", %d edit%s)", w->near, w->near_dist,
                                                     w->near_dist == 1 ? "" : "s"));
                        }
                }
                RUN(tld_strbuf_append(b, found == q->n_words ? " none.\n" : ".\n"));
        }
        if(q->n_ignored){
                RUN(tld_strbuf_append(b, "Not searched:"));
                for(int j = 0; j < q->n_ignored; j++){
                        RUN(tld__doc_appendf(b, "%s %s", j ? "," : "", q->ignored[j]));
                }
                RUN(tld__doc_appendf(b, "%s (only the first %d distinct words of a question are searched).\n",
                                     q->more_ignored ? " and more" : "", TLD__DOC_MAX_WORDS));
        }
        if(a->n_hits){
                RUN(tld_strbuf_append(b, "Entries ranked by word match. Everything under ENTRY or SECTION is "
                                      "copied exactly from the tld manuals.\n"));
        }else if(q->n_words){
                RUN(tld_strbuf_append(b, "No entry of the tld manuals matches these words.\n"));
        }
        return OK;
ERROR:
        return FAIL;
}

/* The "Matched:" line of record r. */
static int tld__doc_match_line(tld_strbuf *b, const tld__doc_query *q, int r)
{
        int k = 0;
        RUN(tld_strbuf_append(b, "Matched:"));
        for(int j = 0; j < q->n_words; j++){
                if(q->m[r][j].name){
                        RUN(tld__doc_appendf(b, "%s %s \"%s\"", k++ ? ";" : "",
                                             q->m[r][j].name == 2 ? "name" : "close to name", q->w[j].word));
                }
        }
        for(int near = 0; near < 2; near++){
                for(int j = 0, n = 0; j < q->n_words; j++){
                        const tld__doc_match *m = &q->m[r][j];
                        int any = 0;
                        for(int f = 0; f < TLD__DOC_N_FIELDS; f++){
                                any |= m->tf[f] > 0;
                        }
                        if(m->name || !any || m->near != near){
                                continue;
                        }
                        RUN(tld__doc_appendf(b, "%s%s", n++ ? ", " : k++ ? (near ? "; near " : "; words ") :
                                             (near ? " near " : " words "), q->w[j].word));
                }
        }
        RUN(tld_strbuf_append(b, "\n"));
        return OK;
ERROR:
        return FAIL;
}

/* The "See also" line of e: the names of other records its doc text mentions (as whole
 * identifiers), at most TLD__DOC_MAX_SEE and "... and N more"; nothing when there are
 * none. Only the doc text: there a name is a deliberate reference, while the examples
 * name the idioms (RUN, ASSERT, ...) everywhere. */
static int tld__doc_see_also(tld_strbuf *b, const tld_doc_entry *e)
{
        const tld_doc_entry *seen[TLD__DOC_N_ENTRIES];
        int n_seen = 0;
        {
                const char *s = e->doc;
                size_t i = 0;
                while(s && s[i]){
                        size_t st;
                        if(!(tld__doc_alnum((unsigned char)s[i]) || s[i] == '_')){
                                i++;
                                continue;
                        }
                        st = i;
                        while(s[i] && (tld__doc_alnum((unsigned char)s[i]) || s[i] == '_')){
                                i++;
                        }
                        for(int r = 0; r < TLD__DOC_N_ENTRIES; r++){
                                const tld_doc_entry *o = &tld__doc_corpus[r];
                                int dup = o == e;
                                for(int x = 0; x < n_seen && !dup; x++){
                                        dup = seen[x] == o;
                                }
                                if(dup || !tld__doc_is_name(o, s + st, i - st, 0)){
                                        continue;
                                }
                                if(n_seen < TLD__DOC_MAX_SEE){
                                        RUN(tld__doc_appendf(b, "%s%.*s", n_seen ? ", " : "See also: ", (int)(i - st),
                                                             s + st));
                                }
                                seen[n_seen++] = o;
                                break;
                        }
                }
        }
        if(n_seen > TLD__DOC_MAX_SEE){
                RUN(tld__doc_appendf(b, ", ... and %d more", n_seen - TLD__DOC_MAX_SEE));
        }
        if(n_seen){
                RUN(tld_strbuf_append(b, "\n"));
        }
        return OK;
ERROR:
        return FAIL;
}

/* Append to b the block of hit h (numbered rank) in at most limit bytes, as complete as
 * fits: the mandatory part (names, header with the module description when desc is
 * set, matched line, declaration, short
 * Documentation and Example lines, See also), then the whole doc text or as many of its
 * first sentences as fit, then the example when the doc text is whole. *shown is 0 (and
 * b unchanged) when the mandatory part does not fit. */
static int tld__doc_entry(tld_strbuf *b, const tld_doc_answer *a, const tld__doc_query *q, int h, int rank,
                          int desc, long limit, int *shown)
{
        static const char doc_min[] = "Documentation: not shown (byte budget).\n";
        static const char ex_none[] = "Example: none in the manual.\n";
        static const char ex_min[] = "Example: not shown (byte budget).\n";
        const tld_doc_entry *e = a->hits[h].entry;
        int r = (int)(e - tld__doc_corpus);
        int ai = strcmp(e->header, "tld_ai.h") == 0;
        tld_strbuf *top = NULL;
        tld_strbuf *see = NULL;
        tld_strbuf *doc = NULL;
        tld_strbuf *ex = NULL;
        long need;
        long rem;
        int doc_full = 0;
        *shown = 0;
        RUN(tld_strbuf_alloc(&top, 1024));
        RUN(tld_strbuf_alloc(&see, 256));
        RUN(tld_strbuf_alloc(&doc, 1024));
        RUN(tld_strbuf_alloc(&ex, 1024));
        if(e->kind == TLD_DOC_SECTION){
                RUN(tld__doc_appendf(top, "\nSECTION %d: %s\n", rank, e->names));
                RUN(tld__doc_match_line(top, q, r));
        }else{
                RUN(tld__doc_appendf(top, "\nENTRY %d: %s\nHeader: %s, %s %s", rank, e->names, e->header,
                                     ai ? "section" : "module", e->module));
                if(e->module_desc[0] && desc){
                        RUN(tld__doc_appendf(top, " (%s)", e->module_desc));
                }
                RUN(tld_strbuf_append(top, "\n"));
                RUN(tld__doc_match_line(top, q, r));
                RUN(tld_strbuf_append(top, "Declaration:\n"));
                RUN(tld__doc_indent(top, e->decl, strlen(e->decl)));
        }
        if(e->kind != TLD_DOC_SECTION){ /* a section names many entries, all in its text */
                RUN(tld__doc_see_also(see, e));
        }
        need = TLD_STRLEN(top) + TLD_STRLEN(see) + (long)strlen(doc_min) + (long)strlen(e->example ? ex_min : ex_none);
        if(need > limit){
                goto DONE;
        }
        rem = limit - need;
        /* the doc text: whole, or its first sentences and " ..." */
        RUN(tld_strbuf_append(doc, "Documentation:\n"));
        RUN(tld__doc_indent(doc, e->doc, strlen(e->doc)));
        if(TLD_STRLEN(doc) - (long)strlen(doc_min) <= rem){
                doc_full = 1;
        }else{
                size_t n = strlen(e->doc);
                size_t cut = 0;
                for(size_t i = 0; i < n; i++){
                        if(e->doc[i] == '.' && tld__doc_sentence_end(e->doc, i, n)){
                                /* "Documentation:\n", the indented sentences, " ..." */
                                long len = 15 + tld__doc_indent_len(e->doc, i + 1) + 4;
                                if(len - (long)strlen(doc_min) > rem){
                                        break;
                                }
                                cut = i + 1;
                        }
                }
                RUN(tld_strbuf_clear(doc));
                if(cut){
                        tld_strbuf *t = NULL;
                        RUN(tld_strbuf_alloc(&t, (int)cut + 8));
                        if(tld_strbuf_append_n(t, e->doc, cut) != OK || tld_strbuf_append(t, " ...") != OK ||
                           tld_strbuf_append(doc, "Documentation:\n") != OK ||
                           tld__doc_indent(doc, TLD_STR(t), (size_t)TLD_STRLEN(t)) != OK){
                                tld_strbuf_free(t);
                                ERROR_MSG("Could not cut the doc text");
                        }
                        tld_strbuf_free(t);
                }else{
                        RUN(tld_strbuf_append(doc, doc_min));
                }
        }
        rem -= TLD_STRLEN(doc) - (long)strlen(doc_min);
        /* the example */
        if(e->example){
                RUN(tld_strbuf_append(ex, "Example:\n"));
                RUN(tld__doc_indent(ex, e->example, strlen(e->example)));
                if(!doc_full || TLD_STRLEN(ex) - (long)strlen(ex_min) > rem){
                        RUN(tld_strbuf_clear(ex));
                        RUN(tld_strbuf_append(ex, ex_min));
                }
        }else{
                RUN(tld_strbuf_append(ex, ex_none));
        }
        RUN(tld_strbuf_append(b, TLD_STR(top)));
        RUN(tld_strbuf_append(b, TLD_STR(doc)));
        RUN(tld_strbuf_append(b, TLD_STR(ex)));
        RUN(tld_strbuf_append(b, TLD_STR(see)));
        *shown = 1;
DONE:
        tld_strbuf_free(top);
        tld_strbuf_free(see);
        tld_strbuf_free(doc);
        tld_strbuf_free(ex);
        return OK;
ERROR:
        tld_strbuf_free(top);
        tld_strbuf_free(see);
        tld_strbuf_free(doc);
        tld_strbuf_free(ex);
        return FAIL;
}

/* Append the "Not shown (byte budget)" line for hits from .. n_hits - 1 in at most
 * limit bytes: as many names as fit and "... and N more"; at least "Not shown (byte
 * budget): ... and N more" even beyond limit. Nothing when from == n_hits. */
static int tld__doc_footer(tld_strbuf *b, const tld_doc_answer *a, int from, long limit)
{
        tld_strbuf *s = NULL;
        tld_strbuf *name = NULL;
        int done = 0;
        int n = a->n_hits - from;
        if(n <= 0){
                return OK;
        }
        RUN(tld_strbuf_alloc(&s, 256));
        RUN(tld_strbuf_alloc(&name, 128));
        RUN(tld_strbuf_append(s, "\nNot shown (byte budget): "));
        for(int i = from; i < a->n_hits; i++){
                long tail;
                RUN(tld_strbuf_clear(name));
                RUN(tld_strbuf_append(name, done ? "; " : ""));
                RUN(tld__doc_short_names(name, a->hits[i].entry));
                /* room for "; ... and N more" and the newline */
                tail = done + 1 < n ? snprintf(NULL, 0, "; ... and %d more\n", n - done - 1) : 1;
                if(TLD_STRLEN(s) + TLD_STRLEN(name) + tail > limit){
                        break;
                }
                RUN(tld_strbuf_append(s, TLD_STR(name)));
                done++;
        }
        if(done < n){
                RUN(tld__doc_appendf(s, "%s... and %d more", done ? "; " : "", n - done));
        }
        RUN(tld_strbuf_append(s, "\n"));
        RUN(tld_strbuf_append(b, TLD_STR(s)));
        tld_strbuf_free(s);
        tld_strbuf_free(name);
        return OK;
ERROR:
        tld_strbuf_free(s);
        tld_strbuf_free(name);
        return FAIL;
}

/* Write a->text from a and q within max_bytes (see tld_doc_ask): the head, the hits in
 * rank order, each as complete as fits in what is left after the Not shown line the
 * hits after it would need, until one does not fit; then the Not shown line. The module
 * description goes with the first entry of its module. */
static int tld__doc_render(tld_doc_answer *a, const tld__doc_query *q, int max_bytes)
{
        tld_strbuf *b = NULL;
        tld_strbuf *f = NULL;
        int from = 0;
        RUN(tld_strbuf_alloc(&b, (max_bytes < 8192 ? max_bytes : 8192) + 64));
        RUN(tld_strbuf_alloc(&f, 1024));
        RUN(tld__doc_head(b, a, q));
        for(; from < a->n_hits; from++){
                const tld_doc_entry *e = a->hits[from].entry;
                int shown = 0;
                int desc = 1;
                for(int x = 0; x < from; x++){
                        desc &= !(strcmp(a->hits[x].entry->module, e->module) == 0 &&
                                  strcmp(a->hits[x].entry->header, e->header) == 0);
                }
                RUN(tld_strbuf_clear(f));
                RUN(tld__doc_footer(f, a, from + 1, (long)INT32_MAX));
                RUN(tld__doc_entry(b, a, q, from, from + 1, desc, (long)max_bytes - TLD_STRLEN(b) - TLD_STRLEN(f),
                                   &shown));
                if(!shown){
                        break;
                }
        }
        RUN(tld_strbuf_clear(f));
        RUN(tld__doc_footer(f, a, from, (long)max_bytes - TLD_STRLEN(b)));
        RUN(tld_strbuf_append(b, TLD_STR(f)));
        MMALLOC(a->text, (size_t)TLD_STRLEN(b) + 1);
        memcpy(a->text, TLD_STR(b), (size_t)TLD_STRLEN(b) + 1);
        tld_strbuf_free(f);
        tld_strbuf_free(b);
        return OK;
ERROR:
        tld_strbuf_free(f);
        tld_strbuf_free(b);
        return FAIL;
}

int tld_doc_ask(const char *question, const tld_doc_opts *o, tld_doc_answer **out)
{
        tld_doc_answer *a = NULL;
        tld__doc_query *q = NULL;
        int max_bytes = 4096;
        int top_n = 8;
        ASSERT(out != NULL, "No place for the answer (out is NULL)");
        *out = NULL;
        ASSERT(question != NULL, "No question (question is NULL)");
        if(o){
                ASSERT(o->max_bytes >= 0, "max_bytes must not be negative (got %d)", o->max_bytes);
                ASSERT(o->top_n >= 0, "top_n must not be negative (got %d)", o->top_n);
                max_bytes = o->max_bytes ? o->max_bytes : max_bytes;
                top_n = o->top_n ? o->top_n : top_n;
        }
        MMALLOC(a, sizeof *a);
        MMALLOC(q, sizeof *q);
        RUN(tld__doc_search(question, top_n, q, a));
        RUN(tld__doc_render(a, q, max_bytes));
        MFREE(q);
        *out = a;
        return OK;
ERROR:
        MFREE(q);
        tld_doc_answer_free(a);
        return FAIL;
}

void tld_doc_answer_free(tld_doc_answer *a)
{
        if(a){
                MFREE(a->hits);
                MFREE(a->text);
                MFREE(a);
        }
}

/* The implementation's own macros end here (TLD__DOC_IMPLEMENTED stays: it guards
 * against a second inclusion); tests/names/check.sh checks this. */
#undef TLD__DOC_N_ENTRIES
#undef TLD__DOC_CORPUS_ID
#undef TLD__DOC_MAX_WORDS
#undef TLD__DOC_MAX_WORD
#undef TLD__DOC_MAX_QUESTION
#undef TLD__DOC_MAX_IGNORED
#undef TLD__DOC_MAX_SEE
#undef TLD__DOC_MAX_HEAD
#undef TLD__DOC_N_FIELDS
#undef TLD__DOC_W_NAMES
#undef TLD__DOC_K1
#undef TLD__DOC_B

#endif /* TLD__DOC_IMPLEMENTED */
#endif /* TLD_DOC_IMPLEMENTATION */

/*
MIT License

Copyright (c) 2026 Timo Lassmann

Permission is hereby granted, free of charge, to any person obtaining a copy of this
software and associated documentation files (the "Software"), to deal in the Software
without restriction, including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/
