/* tld_ai.h - local LLMs, decision models and embeddings through the Ollama server on this
 * machine, called as ordinary C functions. Single header, builds on tld.h. Thread-safe.
 *
 *   char *s = ai_text("Summarise:\n%s", doc);
 *   double p = ai_yesno("Is a refund being requested?", ticket);
 *   struct team_answer t = ai_choose(team, ticket);
 *   ai_fill(&fix, fix, "Propose a fix for:\n%s", ticket);
 *   ai_vec v = ai_embed(abstract);
 *
 * Manual: docs/tld_ai.md, generated from tld_ai.h by tools/tlddoc.c.
 *
 * USAGE
 *   In exactly one .c file, before any other #include:
 *
 *       #define TLD_IMPLEMENTATION      // unless another file already compiles tld.h
 *       #define AI_IMPLEMENTATION
 *       #include "tld_ai.h"                 // includes "tld.h" itself if TLD_H is not defined
 *
 *   and plain #include "tld_ai.h" everywhere else. The program must compile
 *   TLD_IMPLEMENTATION in some file (tld_ai.h uses tld's JSON, strings and hash map).
 *   Build: cc -std=c11 prog.c -lpthread -lm. Nothing else: no libcurl, no SQLite, no
 *   cJSON (HTTP, JSON, cache and reviews are done here and in tld.h).
 *
 *   C11 plus POSIX (sockets, poll, flock, pthreads); macOS and Linux. In the
 *   AI_IMPLEMENTATION file tld_ai.h asks for POSIX.1-2008 itself (it defines
 *   _POSIX_C_SOURCE, _DEFAULT_SOURCE and _DARWIN_C_SOURCE when they are not set), so
 *   nothing needs -D flags. That works when tld_ai.h comes before every system header in
 *   that file; if a system header came first, tld_ai.h supplies the few declarations a
 *   strict -std=c11 build hides.
 *
 * ENVIRONMENT
 *   All optional; read once, at the first call into tld_ai.h, so a later setenv() has
 *   no effect.
 *
 *   TLD_AI_HOST      Ollama base URL                      default http://localhost:11434
 *                    (plain http only; https:// fails with AI_EHTTP)
 *   TLD_AI_LLM       default LLM                          default inkling-2-mini:latest
 *   TLD_AI_DECIDER   default decision model               default autotrust-jev:latest
 *   TLD_AI_EMBEDDER  default embedding model              default embeddinggemma-2:latest
 *   TLD_AI_NUM_CTX   context length sent with every LLM and embedding call that does not
 *                    set .num_ctx                         default: not sent (server decides)
 *   TLD_AI_TIMEOUT   seconds per HTTP request             default 300
 *   TLD_AI_FRESH     1 = ignore the cache                 default 0
 *   TLD_AI_DB        cache file (JSON lines)              default: no cache; "off" turns
 *                    off a cache set with ai_defaults(.db = ...)
 *   TLD_AI_REVIEWS   directory for ai_review files        default ai-reviews
 *
 *   TLD_AI_FRESH accepts 1/0, true/false, yes/no, on/off (any case); TLD_AI_NUM_CTX and
 *   TLD_AI_TIMEOUT must be positive integers. A value that does not parse is ignored with a
 *   warning.
 *
 * DEFAULTS SET IN CODE
 *   ai_defaults(AI_CONFIG(.host = "http://192.168.1.20:11434", .llm = "inkling-2-mini",
 *                         .decider = "autotrust-jev", .embedder = "embeddinggemma-2",
 *                         .opts = AI_OPTS(.temperature = AI_F(0.2))));
 *
 *   Each field that is set (a non-NULL string, an ai_i with .set, a non-NULL .opts)
 *   replaces the built-in default; an environment variable, if set, still wins, and
 *   per-call options win over both. Each call replaces the previous one's settings;
 *   ai_defaults(NULL) resets to the built-ins. Strings and .opts are copied. Invalid
 *   input (a host that is not http://, an empty model name, a number <= 0) changes
 *   nothing and returns -1. db is read when the cache is first opened, so a later
 *   change has no effect on that run; reviews is read by each ai_review call. .opts is
 *   merged field by field under each call's options, for text calls only (its .model
 *   is ignored); embeddings take .embed_dimensions as their default dimensions. Calls
 *   in flight keep the settings they started with. Vectors from different embedders
 *   are not comparable: programs that store vectors should store the embedder's name
 *   next to them.
 *
 * ERRORS
 *   Failure values: NULL, -1, .x == NULL or .ok == 0, as documented per call. Nothing
 *   exits or aborts. ai_status() gives this thread's last code (AI_OK, AI_ENOSERVER,
 *   AI_EHTTP, ...) and ai_error() the message. Every call that needs the server probes
 *   GET /api/version first; with no server every such call fails fast with
 *   AI_ENOSERVER, and the probe is retried at most every 5 s. Failures print nothing;
 *   warnings (an ignored environment value, a cache line that cannot be read or
 *   written, too many kept results) and the review notices are tld.h WARNING and LOG
 *   lines on stderr, so TLD_VERBOSE=0 silences them.
 *
 * SERVER MEMORY
 *   On a machine that can hold only one model, start Ollama with
 *   OLLAMA_MAX_LOADED_MODELS=1; the server then swaps models safely between requests.
 *
 * CLOUD MODELS
 *   Opt-in only. A cloud model is one whose /api/tags entry has "remote_host"; it is
 *   reached through the local server like any other (tld_ai.h does no HTTPS). The defaults
 *   are local models, so prompts leave the machine only when a cloud model is named in
 *   TLD_AI_LLM, TLD_AI_DECIDER, TLD_AI_EMBEDDER or .model. A server that is not signed in answers
 *   with an HTTP error, which comes back as AI_EHTTP with the server's message.
 *
 * CACHE AND REPLAY
 *   Every answered request is appended to TLD_AI_DB as one JSON line
 *   {key, endpoint, model, digest, request, response, seconds, created}, under
 *   flock(LOCK_EX), so several processes can share the file. The key is FNV-1a 64 of
 *   endpoint, model digest and request body, plus "#n" for the n-th identical request
 *   in the process; a rerun therefore replays finished calls in order, and a retry is
 *   a new call. n advances only when an answer is obtained, so a transient failure
 *   does not shift the replay. The digest stops an updated model from replaying old
 *   answers. TLD_AI_FRESH=1 or .fresh = 1 skips the lookup (the answer is still stored).
 *
 * HUMAN REVIEW
 *   ai_review(what, payload) writes TLD_AI_REVIEWS/<key>.json =
 *   {"what":..., "payload":..., "status":"open", "answer":null} and returns NULL (with
 *   ai_status() == AI_OK) while the review is open. A person edits the file, sets
 *   "status" to "done" and "answer" to a string; the next run returns that answer.
 *
 * MEMORY
 *   Strings, vectors and the strings in an AI_STRUCT belong to the library: they are
 *   const, so the compiler warns when one is passed to free(). They stay valid until
 *   ai_reset() or exit, which means memory grows with every call. A program that calls
 *   in a loop should call ai_reset() each round, once it has used or copied that round's
 *   results. When 100000 values are kept without an ai_reset(), the library prints a
 *   warning to stderr.
 *
 * THREADS
 *   Every function may be called from any thread. Call ai_reset() only when no thread
 *   still uses returned values. Error text, status and ai_last() are per thread.
 *
 * LICENCE: MIT, see the end of tld_ai.h.
 */

/* Feature macros for the implementation file. They must precede every system header,
 * which is why they sit above the include guard and the #include "tld.h". */
#if defined(AI_IMPLEMENTATION) && !defined(AI__FEATURES)
#define AI__FEATURES
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#endif

#ifndef AI_H
#define AI_H

#ifndef TLD_H
#include "tld.h"
#endif
#include <stddef.h>

/* Version of tld_ai.h. */
#define AI_VERSION "0.3.0"

/* Prompts and ai_fmt take printf formats. The compiler checks the arguments, and the
 * macros below check that the format is a string literal (TLD__LITERAL, as in tld.h's
 * message macros): a compile error there means text was passed as the format, which
 * would read or write random memory if it contained a %. Pass text with "%s":
 * ai_text("%s", prompt). */

#ifdef __cplusplus
extern "C" {
#endif

/* ---- status codes (ai_status) ------------------------------------------ */

/* Status codes: ai_status() returns the one for this thread's last call. */
enum {
    AI_OK = 0,
    AI_ENOSERVER, /* no Ollama server answers at TLD_AI_HOST                       */
    AI_EHTTP,     /* https host, timeout, truncated reply, HTTP error status    */
    AI_EPARSE,    /* the reply is not valid JSON or lacks the expected fields   */
    AI_ESCHEMA,   /* valid JSON that does not fit the struct, labels or range   */
    AI_ENOMODEL,  /* the model is not installed on the server                   */
    AI_EIO,       /* cache, review or image file could not be read or written   */
    AI_ENOMEM,    /* out of memory                                              */
    AI_EARG       /* invalid argument (NULL format, n <= 0, ...)                */
};

/* ---- options ----------------------------------------------------------- */

/* Optional numbers for ai_opts and ai_config: .set = 1 marks a value as given, so an
 * unset field differs from 0. Write AI_F(0.2) and AI_I(7). */
typedef struct { int set; double v; } ai_f;   /* optional double: unset is not 0 */
typedef struct { int set; long   v; } ai_i;   /* optional integer */
#define AI_F(x) ((ai_f){1, (x)})
#define AI_I(x) ((ai_i){1, (x)})

/* Per-call options for the *_with calls; every field is optional (0 / NULL / unset).
 * Text calls (ai_text, ai_fill) use all fields except dimensions and no_truncate, with
 * unset fields taken from ai_defaults(.opts). Decisions (ai_yesno, ai_choose, ai_rate)
 * use only model, images and fresh. Embeddings use model, dimensions, no_truncate,
 * fresh and the "options" fields (temperature .. num_predict, stop, extra_options),
 * think and keep_alive. retries and max_retries apply to ai_fill. */
typedef struct {
    const char *model;            /* overrides TLD_AI_LLM / TLD_AI_DECIDER / TLD_AI_EMBEDDER */
    const char *system;           /* system prompt (LLM calls) */
    ai_f temperature, top_p, min_p;
    ai_i top_k, seed, num_ctx, num_predict;
    const char *const *stop;      /* NULL-terminated stop sequences */
    const char *think;            /* "true", "false", "low", "medium", "high" */
    const char *keep_alive;       /* "10m", "0", ... */
    const char *const *images;    /* NULL-terminated image file paths */
    const char *extra_options;    /* raw JSON object merged into "options" */
    ai_i dimensions;              /* embeddings: output dimensions, if the model allows */
    int no_truncate;              /* embeddings: fail instead of truncating long input */
    int retries;                  /* extra attempts on malformed output (0 = default 2) */
    int fresh;                    /* 1 = bypass the cache for this call */
    ai_i max_retries;             /* extra attempts, 0 allowed; overrides .retries */
} ai_opts;

/* AI_OPTS(.temperature = AI_F(0), .seed = AI_I(7)) -> const ai_opts *, pointing to a
 * compound literal that lives until the end of the enclosing block. */
#define AI_OPTS(...) (&(const ai_opts){__VA_ARGS__})

/* ---- program-set defaults ---------------------------------------------- */

/* Program-wide defaults for ai_defaults, written AI_CONFIG(.llm = "...", ...). Precedence,
 * per setting: per-call options > environment > ai_defaults() > built-in. The
 * environment is read once, at the first call into tld_ai.h. */
typedef struct {
    const char *host;      /* TLD_AI_HOST      e.g. "http://192.168.1.20:11434" (http:// only) */
    const char *llm;       /* TLD_AI_LLM       text model: ai_text, ai_fill                  */
    const char *decider;   /* TLD_AI_DECIDER   decision model: ai_yesno, ai_choose, ai_rate  */
    const char *embedder;  /* TLD_AI_EMBEDDER  embedding model: ai_embed, ai_embed_all       */
    const char *db;        /* TLD_AI_DB        cache file, or "off"                          */
    const char *reviews;   /* TLD_AI_REVIEWS   review directory                              */
    ai_i num_ctx;          /* TLD_AI_NUM_CTX   context length for LLM and embedding calls    */
    ai_i timeout;          /* TLD_AI_TIMEOUT   seconds per request                           */
    ai_i fresh;            /* TLD_AI_FRESH     1 = ignore the cache                          */
    ai_i embed_dimensions; /* default output dimensions for embeddings, if the model allows */
    const ai_opts *opts;   /* default options for text calls (system, temperature, seed,
                              stop, think, keep_alive, retries, ...); decisions take only
                              .fresh from it, embeddings .dimensions, .no_truncate, .fresh */
} ai_config;
#define AI_CONFIG(...) (&(const ai_config){__VA_ARGS__})

/* Set program-wide defaults: the fields c sets replace the built-in defaults; NULL resets
 * to the built-ins. Each call replaces the previous call's settings; strings and .opts are
 * copied. 0 on success, -1 with ai_status()/ai_error(); an invalid call (host not
 * http://, empty name, number <= 0) changes nothing. Calls in flight keep their settings. */
int ai_defaults(const ai_config *c);

/* ---- plain calls ------------------------------------------------------- */

/* Text from the LLM (TLD_AI_LLM, or o->model) for the printf-style prompt fmt. The
 * string is library memory, valid until ai_reset() (never free it); NULL on failure.
 * ai_text_with takes per-call options (o may be NULL). */
const char *ai_text(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
const char *ai_text_with(const ai_opts *o, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/* Probability in [0, 1] that the answer to question about state is yes, from the
 * decision model (TLD_AI_DECIDER, or o->model); -1 on failure. A state that starts
 * with '{' and parses as JSON is sent as a JSON object, anything else as text. */
double      ai_yesno(const char *question, const char *state);
double      ai_yesno_with(const ai_opts *o, const char *question, const char *state);

/* ---- embeddings -------------------------------------------------------- */

/* An embedding of dim floats at x (x == NULL on failure), and n embeddings with row i at
 * x + i*dim. The floats are library memory, valid until ai_reset(); never free them.
 * Vectors from different embedding models are not comparable. */
typedef struct { int dim; const float *x; } ai_vec;              /* x == NULL on failure */
typedef struct { int n, dim; const float *x; } ai_vecs;          /* row i at x + i*dim */

/* The embedding of text from the embedding model (TLD_AI_EMBEDDER, or o->model); .x is
 * NULL on failure. Long input is truncated unless o->no_truncate is set. */
ai_vec  ai_embed(const char *text);
ai_vec  ai_embed_with(const ai_opts *o, const char *text);

/* Embeddings of texts[0..n-1] in one request, as n rows; .x is NULL on failure. */
ai_vecs ai_embed_all(const char *const *texts, int n);           /* one request, n rows */
ai_vecs ai_embed_all_with(const ai_opts *o, const char *const *texts, int n);

/* Row i of the ai_vecs vs as an ai_vec (no copy, no bounds check). */
#define ai_row(vs, i) ((ai_vec){(vs).dim, (vs).x + (size_t)(i) * (vs).dim})

/* Cosine similarity of a and b, in [-1, 1]; 0 if either is all zeros; -2 if the
 * dimensions differ or either .x is NULL. */
double ai_cosine(ai_vec a, ai_vec b);                            /* -2 if dims differ */

/* The k rows of m most similar to q (cosine), best first: row indices into idx and
 * similarities into score, two arrays of at least k elements. Returns how many were
 * written (fewer than k when m has fewer rows; 0 on bad input). */
int    ai_nearest(ai_vec q, ai_vecs m, int k, int *idx, double *score);

/* ---- typed structs filled by an LLM ------------------------------------ */

/* Run-time descriptions made by AI_STRUCT, AI_CHOICE and AI_SCALE: a member's type
 * (ai_kind), one member (ai_field), a struct (ai_type) and one choice or scale label
 * (ai_label). Programs do not normally use them. */
typedef enum { AI_T_str, AI_T_num, AI_T_int, AI_T_bool } ai_kind;
typedef struct { const char *name; ai_kind kind; size_t off; const char *desc; } ai_field;
typedef struct { const char *name; size_t size; const ai_field *f; int n; } ai_type;
typedef struct { const char *name; const char *desc; } ai_label;

#define AI__CT_str  const char *
#define AI__CT_num  double
#define AI__CT_int  long
#define AI__CT_bool int
#define AI__DECL(t, n, d)  AI__CT_##t n;
#define AI__FIELD(t, n, d) { #n, AI_T_##t, offsetof(S, n), d },

/* AI_STRUCT(name, FIELDS) defines struct name for ai_fill. FIELDS is an X-macro of
 * X(kind, member, "description") entries; kind is str (char *), num (double), int
 * (long) or bool (int). The descriptions reach the model in a JSON schema.
 *
 *   #define FIX(X) X(str, title, "One-line summary") X(num, risk, "Risk 0 to 1")
 *   AI_STRUCT(fix, FIX)      // struct fix { char *title; double risk; };
 */
#define AI_STRUCT(name, FIELDS)                                               \
    struct name { FIELDS(AI__DECL) };                                         \
    static inline const ai_type *name##__type(void) {                         \
        typedef struct name S;                                                \
        static const ai_field f[] = { FIELDS(AI__FIELD) };                    \
        static const ai_type t = { #name, sizeof(S), f,                       \
                                   (int)(sizeof f / sizeof f[0]) };           \
        return &t;                                                            \
    }

/* ---- decisions: pick one label ----------------------------------------- */

#define AI__ENUM(n, d)  n,
#define AI__LABEL(n, d) { #n, d },

/* AI_CHOICE(name, question, OPTIONS) defines enum name, one constant per label in order,
 * and struct name_answer { int ok; enum name value; double confidence; double p[]; }
 * (p: one probability per label), for ai_choose. OPTIONS is an X-macro of
 * X(label, "description") entries. Labels become bare enum constants, so two choices
 * cannot share a label (the compiler reports the clash).
 *
 *   #define TEAM(X) X(bug, "Software defects") X(billing, "Payments and refunds")
 *   AI_CHOICE(team, "Which team should handle this?", TEAM)
 */
#define AI_CHOICE(name, question, OPTIONS)                                    \
    enum name { OPTIONS(AI__ENUM) name##__count };                            \
    struct name##_answer { int ok; enum name value; double confidence;        \
                           double p[name##__count]; };                        \
    static inline struct name##_answer name##__choose(const char *state,      \
                                                      const ai_opts *o) {     \
        static const ai_label L[] = { OPTIONS(AI__LABEL) };                   \
        struct name##_answer r = {0};                                         \
        int i = ai__choose(#name, question, L, name##__count, state, o,       \
                           &r.confidence, r.p);                               \
        r.ok = i >= 0; r.value = (enum name)(i >= 0 ? i : 0);                 \
        return r;                                                             \
    }

/* ---- decisions: place on an ordered scale ------------------------------ */

/* AI_SCALE(name, question, LEVELS) defines enum name, one constant per level from lowest
 * to highest, and struct name_answer { int ok; enum name level; double score; double
 * confidence; }, for ai_rate. LEVELS is an X-macro of X(level, "description") entries.
 * score is the model's probability-weighted position, 0 (lowest level) to n-1; level is
 * score rounded to the nearest level.
 *
 * How many levels a scale may have depends on the decision model, and models do not
 * advertise it: autotrust-jev (the default) needs exactly 6. A count the model rejects
 * fails with AI_EHTTP and the model's own message in ai_error(), e.g. 'question
 * "urgency": score criteria must contain exactly 6 levels'.
 *
 *   #define URGENCY(X) X(none, "No action") X(someday, "Within a month")              \
 *                      X(routine, "Within a week") X(soon, "This week")               \
 *                      X(urgent, "Today") X(critical, "Many customers, now")
 *   AI_SCALE(urgency, "How urgent is this?", URGENCY)
 */
#define AI_SCALE(name, question, LEVELS)                                      \
    enum name { LEVELS(AI__ENUM) name##__count };                             \
    struct name##_answer { int ok; enum name level; double score;             \
                           double confidence; };                              \
    static inline struct name##_answer name##__rate(const char *state,        \
                                                    const ai_opts *o) {       \
        static const ai_label L[] = { LEVELS(AI__LABEL) };                    \
        struct name##_answer r = {0};                                         \
        int i = ai__rate(#name, question, L, name##__count, state, o,         \
                         &r.score, &r.confidence);                            \
        r.ok = i >= 0; r.level = (enum name)(i >= 0 ? i : 0);                 \
        return r;                                                             \
    }

/* ---- the calls a program makes ----------------------------------------- */

#define AI__CHECK(p, T) ((void)sizeof((p) == (struct T *)0), (void *)(p))

/* ai_fill(&s, T, fmt, ...): ask the LLM to fill s, a struct T made by AI_STRUCT, for the
 * printf-style task fmt. The struct's JSON schema is appended to the prompt; a reply
 * that does not fit is retried (o->retries, default 2 extra attempts). 0 on success, -1
 * on failure with s zeroed. Strings in s are library memory until ai_reset(); never free
 * them. ai_fill_with(o, &s, T, fmt, ...) takes per-call options. */
#define ai_fill(out, T, ...) \
    (TLD__LITERAL(__VA_ARGS__), ai__fill(AI__CHECK(out, T), T##__type(), NULL, __VA_ARGS__))
#define ai_fill_with(o, out, T, ...) \
    (TLD__LITERAL(__VA_ARGS__), ai__fill(AI__CHECK(out, T), T##__type(), (o), __VA_ARGS__))

/* ai_choose(T, state): ask the decision model which label of the choice T (AI_CHOICE)
 * fits state; returns struct T_answer (struct team_answer for T = team): .ok (0 on
 * failure), .value, .confidence and .p[label]. A state that starts with '{' and parses as JSON is sent as JSON.
 * ai_choose_with(o, T, state) takes options (model, images, fresh). */
#define ai_choose(T, state)             T##__choose((state), NULL)
#define ai_choose_with(o, T, state)     T##__choose((state), (o))

/* ai_rate(T, state): place state on the scale T (AI_SCALE); returns struct T_answer:
 * .ok (0 on failure), .score (the model's score), .level (score rounded to the nearest
 * level and clamped to the scale) and .confidence. ai_rate_with(o, T, state) takes
 * options (model, images, fresh). */
#define ai_rate(T, state)               T##__rate((state), NULL)
#define ai_rate_with(o, T, state)       T##__rate((state), (o))

/* ---- asking a person --------------------------------------------------- */

/* Ask a person (see Human review above). The first call writes TLD_AI_REVIEWS/<key>.json, the
 * key a hash of what and payload, and returns NULL with ai_status() == AI_OK; so does
 * every call while the review is open. Once a person has set "status" to "done" and
 * "answer" to a string, the call returns that answer (library memory). NULL with
 * another status on errors. */
const char *ai_review(const char *what, const char *payload);

/* ---- errors and the last call ------------------------------------------ */

/* The message about this thread's last call: why it failed, or empty after a success
 * (ai_review leaves a note while a review is open). Valid until the thread's next call. */
const char *ai_error(void);

/* The status code (AI_OK, AI_ENOSERVER, ...) of this thread's last call. */
int         ai_status(void);

/* Details of a request to the server, from ai_last(). Strings are library memory. */
typedef struct {
    const char *thinking;         /* model's thinking text, if requested */
    long   prompt_tokens, output_tokens;
    double seconds;               /* wall time of the HTTP call, 0 if cached */
    int    from_cache;
    const char *model;            /* model that answered */
    const char *host;             /* server that was asked */
} ai_info;

/* Details of the last request this thread sent (or tried to send) to the server. */
const ai_info *ai_last(void);

/* ---- library memory ---------------------------------------------------- */

/* sprintf into library memory: the formatted string, valid until ai_reset() (never free
 * it); NULL on failure. */
const char *ai_fmt(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* ai_text, ai_text_with and ai_fmt are also macros that require a literal format (see
 * TLD__LITERAL); (ai_text)(fmt, ...) calls the function itself. */
#define ai_text(...) (TLD__LITERAL(__VA_ARGS__), (ai_text)(__VA_ARGS__))
#define ai_text_with(o, ...) (TLD__LITERAL(__VA_ARGS__), (ai_text_with)((o), __VA_ARGS__))
#define ai_fmt(...) (TLD__LITERAL(__VA_ARGS__), (ai_fmt)(__VA_ARGS__))

/* Free every string and vector the library has returned, in all threads. Call it only
 * when no thread still uses them; the library keeps working afterwards. */
void        ai_reset(void);

/* ---- parallel work ----------------------------------------------------- */

/* Run fn(i, ctx) for i = 0..n-1 on `threads` threads and wait for all. The calling
 * thread works too; the calls run concurrently, in no fixed order, and may themselves
 * call tld_ai.h. */
void ai_parallel(int n, int threads, void (*fn)(int i, void *ctx), void *ctx);

/* internals used by the macros */
int ai__fill(void *out, const ai_type *t, const ai_opts *o, const char *fmt, ...) __attribute__((format(printf, 4, 5)));
int ai__choose(const char *name, const char *question, const ai_label *L, int n,
               const char *state, const ai_opts *o, double *confidence, double *p);
int ai__rate(const char *name, const char *question, const ai_label *L, int n,
             const char *state, const ai_opts *o, double *score, double *confidence);

#ifdef __cplusplus
}
#endif
#endif /* AI_H */

/* ======================================================================== */
/*                              IMPLEMENTATION                              */
/* ======================================================================== */

#ifdef AI_IMPLEMENTATION
#ifndef AI__IMPLEMENTED
#define AI__IMPLEMENTED

#ifdef __cplusplus
#error "tld_ai.h: the implementation is C only; define AI_IMPLEMENTATION in a .c file"
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* Declarations that a strict ISO C build hides when a system header was included
 * before tld_ai.h (so the feature macros at the top came too late). */
#ifndef LOCK_EX
#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_UN 8
extern int flock(int fd, int op);
#endif
#if defined(__APPLE__) && !defined(SO_NOSIGPIPE)
#define SO_NOSIGPIPE 0x1022
#endif
#if defined(__GLIBC__) && !defined(__USE_XOPEN2K)
struct addrinfo {
    int ai_flags, ai_family, ai_socktype, ai_protocol;
    socklen_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};
extern int getaddrinfo(const char *, const char *, const struct addrinfo *,
                       struct addrinfo **);
extern void freeaddrinfo(struct addrinfo *);
extern const char *gai_strerror(int);
#endif
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define AI__REPROBE 5.0          /* seconds between probes while the server is down */
#define AI__CONNECT_MAX 5.0      /* connect timeout cap, seconds */
#define AI__MAX_BODY (512 << 20) /* largest reply accepted, bytes */

/* ---- status and per-thread results ------------------------------------- */

static _Thread_local int ai__t_st;
static _Thread_local char ai__t_err[512];
static _Thread_local ai_info ai__t_last;

static void ai__begin(void) {
    ai__t_st = AI_OK;
    ai__t_err[0] = 0;
}

static int ai__fail(int st, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ai__t_err, sizeof ai__t_err, fmt, ap);
    va_end(ap);
    ai__t_st = st;
    return -1;
}

static int ai__nomem(void) { return ai__fail(AI_ENOMEM, "out of memory"); }

const char *ai_error(void) { return ai__t_err; }
int ai_status(void) { return ai__t_st; }
const ai_info *ai_last(void) { return &ai__t_last; }

static double ai__now(void) {
#ifdef CLOCK_MONOTONIC
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
    struct timespec t;
    if (timespec_get(&t, TIME_UTC) != TIME_UTC) return (double)time(NULL);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}


static char *ai__vfmt(const char *fmt, va_list ap) {
    va_list cp;
    va_copy(cp, ap);
    int n = vsnprintf(NULL, 0, fmt, cp);
    va_end(cp);
    if (n < 0) {
        ai__fail(AI_EARG, "bad format string");
        return NULL;
    }
    char *s = malloc((size_t)n + 1);
    if (!s) {
        ai__nomem();
        return NULL;
    }
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    return s;
}

/* FNV-1a 64 over a NUL-terminated part; a zero byte separates parts. */
static uint64_t ai__fnv(uint64_t h, const char *s) {
    for (; *s; s++) {
        h ^= (unsigned char)*s;
        h *= 1099511628211ULL;
    }
    return h * 1099511628211ULL; /* the separator: h ^= 0, then multiply */
}
#define AI__FNV0 1469598103934665603ULL
#define AI__KEEP_WARN 100000 /* kept results before the one-time warning */

/* ---- arena: every string and vector handed to the caller lives here ---- */

static pthread_mutex_t ai__mem_mu = PTHREAD_MUTEX_INITIALIZER;
static void **ai__arena;
static size_t ai__arena_n, ai__arena_cap;
static pthread_once_t ai__exit_once = PTHREAD_ONCE_INIT;
static void ai__shutdown(void);
static void ai__exit_register(void) { atexit(ai__shutdown); }

/* Takes ownership of p. On failure frees p and returns NULL (AI_ENOMEM). */
static void *ai__keep(void *p) {
    if (!p) return NULL;
    pthread_once(&ai__exit_once, ai__exit_register);
    pthread_mutex_lock(&ai__mem_mu);
    if (ai__arena_n == ai__arena_cap) {
        size_t cap = ai__arena_cap ? 2 * ai__arena_cap : 64;
        void **a = realloc(ai__arena, cap * sizeof *a);
        if (!a) {
            pthread_mutex_unlock(&ai__mem_mu);
            free(p);
            ai__nomem();
            return NULL;
        }
        ai__arena = a;
        ai__arena_cap = cap;
    }
    ai__arena[ai__arena_n++] = p;
    int warn = ai__arena_n == AI__KEEP_WARN;
    pthread_mutex_unlock(&ai__mem_mu);
    if (warn)
        WARNING_MSG("tld_ai: %d results are kept in memory; call ai_reset() when they "
                    "are no longer needed (see MEMORY in tld_ai.h)",
                    AI__KEEP_WARN);
    return p;
}

static char *ai__kdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) {
        ai__nomem();
        return NULL;
    }
    return ai__keep(d);
}

void ai_reset(void) {
    pthread_mutex_lock(&ai__mem_mu);
    for (size_t i = 0; i < ai__arena_n; i++) free(ai__arena[i]);
    ai__arena_n = 0;
    pthread_mutex_unlock(&ai__mem_mu);
}

const char *(ai_fmt)(const char *fmt, ...) {
    ai__begin();
    if (!fmt) {
        ai__fail(AI_EARG, "ai_fmt: NULL format");
        return NULL;
    }
    va_list ap;
    va_start(ap, fmt);
    char *s = ai__vfmt(fmt, ap);
    va_end(ap);
    return ai__keep(s);
}

/* ---- JSON helpers on top of tld_json ----------------------------------- */
/* tld_ai.h adds one entry at a time and checks each add (json_*_add_multiple return
 * NULL on failure and free the entry's object or array). */

static void ai__drop(tld_json_decoration d) {
    if (!d.value) return;
    if (d.type == TLD__JVAL_OBJ) tld_json_obj_free((tld_json_obj *)d.value);
    if (d.type == TLD__JVAL_ARR) tld_json_arr_free((tld_json_arr *)d.value);
}

/* Add d to *o (created if NULL). A NULL value is skipped (also a NULL object or array,
 * unlike tld_json_obj_add). Ownership of an object or array value passes to *o, also on
 * failure, when tld frees it (*o is then unchanged: NULL, or the caller's object without
 * the child). */
static int ai__put(tld_json_obj **o, tld_json_decoration d) {
    if (!d.value) return 0;
    return tld_json_obj_add(o, d) == OK ? 0 : ai__nomem();
}

static int ai__push(tld_json_arr **a, tld_json_decoration d) {
    if (!d.value) return 0;
    return tld_json_arr_add(a, d) == OK ? 0 : ai__nomem();
}

/* JSON text of o (or {} for NULL) in a new strbuf: compact, or indented when pretty. */
static int ai__dump(tld_json_obj *o, tld_strbuf **out, int pretty) {
    tld_strbuf *b = NULL;
    if (tld_strbuf_alloc(&b, 1024) != OK) return ai__nomem();
    if (o) {
        if (tld_json_dump(o, b, pretty ? 0 : TLD_JSON_COMPACT) != OK) {
            tld_strbuf_free(b);
            return ai__nomem();
        }
    } else if (tld_strbuf_append(b, "{}") != OK) {
        tld_strbuf_free(b);
        return ai__nomem();
    }
    *out = b;
    return 0;
}

/* Parse the n bytes at s, which must hold a JSON object. 0, -1 when it does not, or the
 * AI_ENOMEM status. */
static int ai__parse_n(const char *s, size_t n, tld_json_obj **out) {
    size_t i = 0;
    while (i < n && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) i++;
    if (i == n || s[i] != '{') return -1;
    *out = NULL;
    errno = 0;
    if (tld_json_parse_n(s, n, out) == OK && *out) return 0;
    /* a failed malloc sets ENOMEM and nothing else in the parser does: out of memory is
       not "not JSON" */
    if (errno == ENOMEM) return ai__nomem();
    return -1;
}

static int ai__parse_buf(tld_strbuf *b, tld_json_obj **out) {
    return ai__parse_n(TLD_STR(b), (size_t)b->len, out);
}

static int ai__parse(const char *s, tld_json_obj **out) {
    size_t n = strlen(s);
    if (n > (size_t)AI__MAX_BODY) return -1;
    return ai__parse_n(s, n, out);
}

static const char *ai__str(tld_json_obj *o, const char *path) {
    const char *s = NULL;
    return tld_json_get_str(o, path, &s) == OK ? s : NULL;
}

static double ai__num(tld_json_obj *o, const char *path, double dflt) {
    double d;
    return tld_json_get_num(o, path, &d) == OK ? d : dflt;
}

static long ai__long(tld_json_obj *o, const char *path) {
    int64_t v;
    return tld_json_get_int(o, path, &v) == OK && v >= 0 && v <= 0x7fffffffL ? (long)v : 0;
}

/* ---- configuration ------------------------------------------------------ */
/* Every setting is resolved in ai__resolve(), highest first: per-call options (merged
 * in ai__merge, at the call), the environment (read once, at first use), the values
 * set with ai_defaults(), the built-in defaults. The result is an immutable ai__conf.
 * Each public call holds a reference to the one in force when it started, so
 * ai_defaults() never changes a call in flight; a replaced ai__conf is freed when its
 * last reference goes. One mutex, ai__cfg_mu, guards all of it. */

enum { AI__S_HOST, AI__S_LLM, AI__S_DECIDER, AI__S_EMBEDDER, AI__S_DB, AI__S_REVIEWS,
       AI__S_NUM_CTX, AI__S_TIMEOUT, AI__S_FRESH, AI__S_DIMS };
#define AI__NSTR 6 /* string settings come first */
#define AI__NNUM 4 /* then the numbers */
static const char *const ai__s_name[AI__NSTR + AI__NNUM] = {
    "host", "llm", "decider", "embedder", "db", "reviews", "num_ctx", "timeout",
    "fresh", "embed_dimensions"};
static const char *const ai__s_builtin[AI__NSTR] = {
    "http://localhost:11434", "inkling-2-mini:latest", "autotrust-jev:latest",
    "embeddinggemma-2:latest", "off", "ai-reviews"};

/* One layer of settings. Strings are owned; NULL / .set == 0 = not set here. */
typedef struct {
    char *str[AI__NSTR];
    ai_f num[AI__NNUM]; /* num_ctx, timeout, fresh, embed_dimensions */
    ai_opts *opts;      /* deep copy (defaults layer only) */
} ai__layer;

typedef struct ai__conf {
    int refs;            /* calls using it */
    char *str[AI__NSTR]; /* resolved values, owned */
    long num_ctx;        /* 0 = not sent */
    double timeout;
    int fresh;
    long dims;           /* default embedding dimensions, 0 = not sent */
    ai_opts *opts;       /* the defaults' per-call options (deep copy), or NULL */
    /* TLD_AI_HOST taken apart */
    char name[256], port[16], authority[300], base[256];
    int https, bad_host, cache_on;
} ai__conf;
#define AI__HOST(c) ((c)->str[AI__S_HOST])

static pthread_mutex_t ai__cfg_mu = PTHREAD_MUTEX_INITIALIZER;
static ai__layer ai__env_layer;
static ai__conf *ai__conf_cur;        /* NULL after the exit cleanup */
static _Thread_local ai__conf *ai__c; /* the settings of this thread's call */
static _Thread_local int ai__depth;

static pthread_once_t ai__once = PTHREAD_ONCE_INIT;
static int ai__init_st;

/* Bounded copy that may truncate (no snprintf, so no truncation warnings). */
static void ai__setstr(char *dst, size_t n, const char *s) {
    size_t k = strlen(s);
    if (k >= n) k = n - 1;
    memcpy(dst, s, k);
    dst[k] = 0;
}

/* ---- deep copies and field-by-field merges of ai_opts -------------------- */

static void ai__strv_free(char **v) {
    for (char **p = v; p && *p; p++) free(*p);
    free(v);
}

static int ai__strv_dup(const char *const *v, const char *const **out) {
    *out = NULL;
    if (!v) return 0;
    size_t n = 0;
    while (v[n]) n++;
    char **d = calloc(n + 1, sizeof *d);
    if (!d) return -1;
    for (size_t i = 0; i < n; i++)
        if (!(d[i] = strdup(v[i]))) {
            ai__strv_free(d);
            return -1;
        }
    *out = (const char *const *)d;
    return 0;
}

static void ai__opts_free(ai_opts *o) {
    if (!o) return;
    free((char *)o->system);
    free((char *)o->think);
    free((char *)o->keep_alive);
    free((char *)o->extra_options);
    ai__strv_free((char **)o->stop);
    ai__strv_free((char **)o->images);
    free(o);
}

/* Deep copy; .model is dropped (model defaults are llm, decider and embedder). */
static ai_opts *ai__opts_dup(const ai_opts *s) {
    ai_opts *o = calloc(1, sizeof *o);
    if (!o) return NULL;
    *o = *s;
    o->model = o->system = o->think = o->keep_alive = o->extra_options = NULL;
    o->stop = o->images = NULL;
    if ((s->system && !(o->system = strdup(s->system))) ||
        (s->think && !(o->think = strdup(s->think))) ||
        (s->keep_alive && !(o->keep_alive = strdup(s->keep_alive))) ||
        (s->extra_options && !(o->extra_options = strdup(s->extra_options))) ||
        ai__strv_dup(s->stop, &o->stop) || ai__strv_dup(s->images, &o->images)) {
        ai__opts_free(o);
        return NULL;
    }
    return o;
}

/* out = hi where hi sets a field, else lo. A field is set when its ai_f/ai_i has
 * .set, a pointer is non-NULL, an int is non-zero. lo's model and num_ctx are never
 * taken: the model defaults are per endpoint and num_ctx has its own chain. */
static void ai__merge(const ai_opts *hi, const ai_opts *lo, ai_opts *out) {
    static const ai_opts none;
    if (!hi) hi = &none;
    if (!lo) lo = &none;
    *out = *hi;
#define AI__M(f) if (!hi->f) out->f = lo->f
#define AI__MS(f) if (!hi->f.set) out->f = lo->f
    AI__M(system); AI__M(stop); AI__M(think); AI__M(keep_alive); AI__M(images);
    AI__M(extra_options); AI__M(no_truncate); AI__M(retries); AI__M(fresh);
    AI__MS(temperature); AI__MS(top_p); AI__MS(min_p); AI__MS(top_k); AI__MS(seed);
    AI__MS(num_predict); AI__MS(dimensions); AI__MS(max_retries);
#undef AI__M
#undef AI__MS
}

/* ---- layers ------------------------------------------------------------- */

static void ai__layer_free(ai__layer *l) {
    for (int i = 0; i < AI__NSTR; i++) free(l->str[i]);
    ai__opts_free(l->opts);
    memset(l, 0, sizeof *l);
}

static void ai__env_ignored(const char *name, const char *v, const char *want) {
    WARNING_MSG("tld_ai: ignoring %s=%.400s: expected %s", name, v, want);
}


/* The environment, read once. Values that do not parse are ignored with a warning. */
static int ai__env_read(ai__layer *l) {
    static const char *const names[AI__NSTR + 3] = {
        "TLD_AI_HOST", "TLD_AI_LLM", "TLD_AI_DECIDER", "TLD_AI_EMBEDDER", "TLD_AI_DB", "TLD_AI_REVIEWS",
        "TLD_AI_NUM_CTX", "TLD_AI_TIMEOUT", "TLD_AI_FRESH"};
    static const char *const yes[] = {"1", "true", "yes", "on"}, *const no[] = {"0", "false",
                                                                              "no", "off"};
    memset(l, 0, sizeof *l);
    for (int i = 0; i < AI__NSTR + 3; i++) {
        const char *v = getenv(names[i]);
        if (!v || !*v) continue;
        if (i < AI__NSTR) {
            if (!(l->str[i] = strdup(v))) return -1;
            continue;
        }
        ai_f *n = &l->num[i - AI__NSTR];
        if (i == AI__S_FRESH) {
            for (int k = 0; k < 4; k++) {
                if (!strcasecmp(v, yes[k])) *n = AI_F(1);
                if (!strcasecmp(v, no[k])) *n = AI_F(0);
            }
            if (!n->set) ai__env_ignored(names[i], v, "1/0, true/false, yes/no or on/off");
            continue;
        }
        char *end = NULL;
        errno = 0;
        long d = strtol(v, &end, 10);
        if (end != v && !*end && !errno && d > 0)
            *n = AI_F((double)d);
        else
            ai__env_ignored(names[i], v, "a positive integer");
    }
    return 0;
}

static void ai__conf_host(ai__conf *c) {
    char *h = c->str[AI__S_HOST];
    size_t n = strlen(h);
    while (n > 0 && h[n - 1] == '/') h[--n] = 0;
    const char *p = h;
    if (!strncmp(p, "https://", 8)) {
        c->https = 1;
        return;
    }
    if (!strncmp(p, "http://", 7))
        p += 7;
    else if (strstr(p, "://")) {
        c->bad_host = 1;
        return;
    }
    const char *slash = strchr(p, '/');
    size_t alen = slash ? (size_t)(slash - p) : strlen(p);
    if (alen == 0 || alen >= sizeof c->authority) {
        c->bad_host = 1;
        return;
    }
    if (slash && strlen(slash) >= sizeof c->base) {
        c->bad_host = 1;
        return;
    }
    memcpy(c->authority, p, alen);
    c->authority[alen] = 0;
    ai__setstr(c->base, sizeof c->base, slash ? slash : "");
    const char *a = c->authority, *port = NULL;
    size_t hl;
    if (a[0] == '[') { /* [::1]:11434 */
        const char *e = strchr(a, ']');
        if (!e) {
            c->bad_host = 1;
            return;
        }
        hl = (size_t)(e - a - 1);
        if (hl >= sizeof c->name) {
            c->bad_host = 1;
            return;
        }
        memcpy(c->name, a + 1, hl);
        if (e[1] == ':') port = e + 2;
    } else {
        const char *col = strrchr(a, ':');
        hl = col ? (size_t)(col - a) : strlen(a);
        if (hl >= sizeof c->name) {
            c->bad_host = 1;
            return;
        }
        memcpy(c->name, a, hl);
        if (col) port = col + 1;
    }
    if (port && strlen(port) >= sizeof c->port) {
        c->bad_host = 1;
        return;
    }
    ai__setstr(c->port, sizeof c->port, port && *port ? port : "11434");
    if (!c->name[0]) c->bad_host = 1;
}

static void ai__conf_free(ai__conf *c) {
    if (!c) return;
    for (int i = 0; i < AI__NSTR; i++) free(c->str[i]);
    ai__opts_free(c->opts);
    free(c);
}

/* THE resolver: every process-level setting comes from here. def may be NULL. */
static ai__conf *ai__resolve(const ai__layer *env, const ai__layer *def) {
    static const ai__layer none;
    static const double builtin_num[AI__NNUM] = {0, 300, 0, 0};
    if (!def) def = &none;
    ai__conf *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    for (int i = 0; i < AI__NSTR; i++) {
        const char *v = env->str[i] ? env->str[i] : def->str[i] ? def->str[i]
                                                                 : ai__s_builtin[i];
        if (!(c->str[i] = strdup(v))) {
            ai__conf_free(c);
            return NULL;
        }
    }
    double num[AI__NNUM];
    for (int i = 0; i < AI__NNUM; i++) {
        const ai_f *e = &env->num[i], *d = &def->num[i];
        num[i] = e->set ? e->v : d->set ? d->v : builtin_num[i];
    }
    /* TLD_AI_NUM_CTX > config.num_ctx > config.opts->num_ctx */
    if (!env->num[0].set && !def->num[0].set && def->opts && def->opts->num_ctx.set &&
        def->opts->num_ctx.v > 0)
        num[0] = (double)def->opts->num_ctx.v;
    c->num_ctx = (long)num[0];
    c->timeout = num[1];
    c->fresh = num[2] != 0;
    c->dims = (long)num[3];
    if (def->opts && !(c->opts = ai__opts_dup(def->opts))) {
        ai__conf_free(c);
        return NULL;
    }
    ai__conf_host(c);
    c->cache_on = strcmp(c->str[AI__S_DB], "off") != 0;
    return c;
}

static void ai__init_once(void) {
    pthread_once(&ai__exit_once, ai__exit_register);
    if (ai__env_read(&ai__env_layer) || !(ai__conf_cur = ai__resolve(&ai__env_layer, NULL)))
        ai__init_st = AI_ENOMEM;
}

static int ai__init(void) {
    pthread_once(&ai__once, ai__init_once);
    if (ai__init_st) return ai__nomem();
    return 0;
}

static int ai__gone(void) {
    return ai__fail(AI_EARG, "tld_ai.h was already shut down (called after exit cleanup)");
}

/* Every public call that uses settings runs between ai__enter and ai__leave. */
static int ai__enter(void) {
    if (ai__init()) return -1;
    if (ai__depth == 0) {
        pthread_mutex_lock(&ai__cfg_mu);
        ai__c = ai__conf_cur;
        if (ai__c) ai__c->refs++;
        pthread_mutex_unlock(&ai__cfg_mu);
        if (!ai__c) return ai__gone();
    }
    ai__depth++;
    return 0;
}

static void ai__leave(void) {
    if (--ai__depth == 0) {
        pthread_mutex_lock(&ai__cfg_mu);
        if (--ai__c->refs == 0 && ai__c != ai__conf_cur) ai__conf_free(ai__c);
        pthread_mutex_unlock(&ai__cfg_mu);
        ai__c = NULL;
    }
}

static int ai__valid_config(const ai_config *c) {
    if (c->host && strncmp(c->host, "http://", 7))
        return ai__fail(AI_EHTTP, "ai_defaults: host %s must start with http:// (tld_ai.h "
                        "talks plain http to the local server)", c->host);
    const char *s[AI__NSTR] = {c->host, c->llm, c->decider, c->embedder, c->db, c->reviews};
    for (int i = 0; i < AI__NSTR; i++)
        if (s[i] && (!*s[i] || (i == AI__S_HOST && !s[i][7])))
            return ai__fail(AI_EARG, "ai_defaults: %s is empty", ai__s_name[i]);
    const ai_i *n[AI__NNUM] = {&c->num_ctx, &c->timeout, &c->fresh, &c->embed_dimensions};
    for (int i = 0; i < AI__NNUM; i++) {
        if (!n[i]->set) continue;
        int flag = i == 2;
        if (flag ? (n[i]->v != 0 && n[i]->v != 1) : n[i]->v <= 0)
            return ai__fail(AI_EARG, "ai_defaults: %s = %ld is invalid (%s)",
                            ai__s_name[AI__NSTR + i], n[i]->v, flag ? "0 or 1" : "must be > 0");
    }
    return 0;
}

/* The defaults layer for c: its strings and .opts copied, its numbers taken. */
static int ai__layer_from(const ai_config *c, ai__layer *l) {
    memset(l, 0, sizeof *l);
    const char *s[AI__NSTR] = {c->host, c->llm, c->decider, c->embedder, c->db, c->reviews};
    const ai_i *n[AI__NNUM] = {&c->num_ctx, &c->timeout, &c->fresh, &c->embed_dimensions};
    for (int i = 0; i < AI__NSTR; i++)
        if (s[i] && !(l->str[i] = strdup(s[i]))) goto nomem;
    for (int i = 0; i < AI__NNUM; i++)
        if (n[i]->set) l->num[i] = AI_F((double)n[i]->v);
    if (c->opts && !(l->opts = ai__opts_dup(c->opts))) goto nomem;
    return 0;
nomem:
    ai__layer_free(l);
    return ai__nomem();
}

int ai_defaults(const ai_config *c) {
    ai__begin();
    if (ai__init() || (c && ai__valid_config(c))) return -1;
    ai__layer l = {0};
    if (c && ai__layer_from(c, &l)) return -1;
    pthread_mutex_lock(&ai__cfg_mu);
    ai__conf *nc = NULL;
    int rc = 0;
    if (!ai__conf_cur)
        rc = ai__gone();
    else if (!(nc = ai__resolve(&ai__env_layer, &l)))
        rc = ai__nomem();
    else {
        ai__conf *old = ai__conf_cur;
        ai__conf_cur = nc;
        if (old->refs == 0) ai__conf_free(old);
    }
    pthread_mutex_unlock(&ai__cfg_mu);
    ai__layer_free(&l);
    return rc;
}

/* ---- cache state (guarded by ai__cache_mu) ------------------------------ */

static pthread_mutex_t ai__cache_mu = PTHREAD_MUTEX_INITIALIZER;
static tld_hash *ai__cache;    /* key -> compact response JSON, as loaded at start */
static tld_hash *ai__seen;     /* base key -> occurrences reserved in this process */
static char *ai__cache_file;   /* the file loaded and appended to, once opened */

static int ai__cache_line(tld_strbuf *line, int *skipped) {
    tld_json_obj *o = NULL, *resp = NULL;
    const char *k;
    const char *p = TLD_STR(line);
    while (*p == ' ' || *p == '\t' || *p == '\r') p++;
    if (!*p) return 0;
    if (ai__parse_buf(line, &o) || !(k = ai__str(o, "key")) ||
        tld_json_get_obj(o, "response", &resp) != OK) {
        tld_json_obj_free(o);
        if (ai__t_st == AI_ENOMEM) return -1;
        (*skipped)++;
        return 0;
    }
    tld_strbuf *txt = NULL;
    int rc = ai__dump(resp, &txt, 0);
    if (!rc && tld_hash_add(&ai__cache, k, TLD_STR(txt)) != OK) rc = ai__nomem();
    tld_strbuf_free(txt);
    tld_json_obj_free(o);
    return rc;
}

static int ai__cache_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (errno == ENOENT) return 0;
        return ai__fail(AI_EIO, "cannot read cache %s: %s", path, strerror(errno));
    }
    tld_strbuf *line = NULL;
    if (tld_strbuf_alloc(&line, 4096) != OK) {
        fclose(f);
        return ai__nomem();
    }
    char buf[16384];
    int rc = 0, skipped = 0;
    size_t got;
    while (!rc && (got = fread(buf, 1, sizeof buf - 1, f)) > 0) {
        buf[got] = 0;
        char *s = buf;
        for (;;) {
            char *nl = memchr(s, '\n', (size_t)(buf + got - s));
            if (nl) *nl = 0;
            if (tld_strbuf_append(line, s) != OK) {
                rc = ai__nomem();
                break;
            }
            if (!nl) break;
            if ((rc = ai__cache_line(line, &skipped))) break;
            tld_strbuf_clear(line);
            s = nl + 1;
        }
    }
    if (!rc && ferror(f))
        rc = ai__fail(AI_EIO, "cannot read cache %s: %s", path, strerror(errno));
    if (!rc) rc = ai__cache_line(line, &skipped); /* last line without '\n' */
    tld_strbuf_free(line);
    fclose(f);
    if (!rc && skipped) WARNING_MSG("tld_ai: skipped %d damaged line(s) in cache %s", skipped, path);
    return rc;
}

/* Load the cache on first use. From then on its path is fixed. */
static int ai__cache_open(void) {
    int rc = 0;
    pthread_mutex_lock(&ai__cache_mu);
    if (!ai__cache_file) {
        char *path = strdup(ai__c->str[AI__S_DB]);
        if (!path)
            rc = ai__nomem();
        else if ((rc = ai__cache_load(path))) {
            free(path);
            tld_hash_free(ai__cache); /* a partial load is dropped; retried next call */
            ai__cache = NULL;
        } else {
            ai__cache_file = path;
        }
    }
    pthread_mutex_unlock(&ai__cache_mu);
    return rc;
}

/* ---- HTTP over POSIX sockets ------------------------------------------- */

/* Server state per host: -1 unknown, 0 down (since .at), 1 up. */
static pthread_mutex_t ai__probe_mu = PTHREAD_MUTEX_INITIALIZER;
static struct {
    char host[512];
    int up;
    double at;
    char why[256];
} ai__probes[16];
static int ai__nprobes;

/* Slot for the current host. Caller holds ai__probe_mu. */
static int ai__probe_slot(void) {
    for (int i = 0; i < ai__nprobes; i++)
        if (!strcmp(ai__probes[i].host, AI__HOST(ai__c))) return i;
    int i = ai__nprobes < 16 ? ai__nprobes++ : 15;
    ai__setstr(ai__probes[i].host, sizeof ai__probes[i].host, AI__HOST(ai__c));
    ai__probes[i].up = -1;
    ai__probes[i].why[0] = 0;
    return i;
}

static void ai__probe_down(const char *why) {
    pthread_mutex_lock(&ai__probe_mu);
    int i = ai__probe_slot();
    ai__probes[i].up = 0;
    ai__probes[i].at = ai__now();
    ai__setstr(ai__probes[i].why, sizeof ai__probes[i].why, why);
    pthread_mutex_unlock(&ai__probe_mu);
}

static int ai__noserver(const char *why) {
    ai__probe_down(why);
    return ai__fail(AI_ENOSERVER,
                    "no Ollama server at %s: %s (start it with `ollama serve`, or set "
                    "TLD_AI_HOST)",
                    AI__HOST(ai__c), why);
}

static int ai__host_ok(void) {
    if (ai__c->https)
        return ai__fail(AI_EHTTP,
                        "TLD_AI_HOST=%s: https is not supported; tld_ai.h talks plain http to the "
                        "local Ollama server (cloud models are reached through it)",
                        AI__HOST(ai__c));
    if (ai__c->bad_host)
        return ai__fail(AI_EHTTP, "TLD_AI_HOST=%s: expected http://host:port", AI__HOST(ai__c));
    return 0;
}

static int ai__ms_left(double deadline) {
    double left = deadline - ai__now();
    if (left <= 0) return 0;
    return left > 1e6 ? 1000000000 : (int)(left * 1000) + 1;
}

static int ai__wait(int fd, short ev, double deadline) {
    for (;;) {
        int ms = ai__ms_left(deadline);
        if (ms <= 0) return 0;
        struct pollfd p = {fd, ev, 0};
        int r = poll(&p, 1, ms);
        if (r > 0) return 1;
        if (r == 0) return 0;
        if (errno != EINTR) return -1;
    }
}

/* Connected non-blocking socket, or -1 with errno-based text in why. */
static int ai__connect(double deadline, char *why, size_t whyn) {
    struct addrinfo hints, *res = NULL, *a;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int g = getaddrinfo(ai__c->name, ai__c->port, &hints, &res);
    if (g) {
        if (snprintf(why, whyn, "cannot resolve %s: %s", ai__c->name, gai_strerror(g)) < 0) why[0] = 0;
        return -1;
    }
    int fd = -1, left = 0;
    for (a = res; a; a = a->ai_next) left++;
    snprintf(why, whyn, "cannot connect");
    /* each address gets an equal share of the time that is left, so one that does not
     * answer cannot use up the budget of the ones after it */
    for (a = res; a; a = a->ai_next, left--) {
        double now = ai__now();
        if (now >= deadline) {
            snprintf(why, whyn, "%s", strerror(ETIMEDOUT));
            break;
        }
        double mine = now + (deadline - now) / left;
        fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd < 0) continue;
        fcntl(fd, F_SETFD, FD_CLOEXEC);
        int fl = fcntl(fd, F_GETFL, 0);
        if (fl < 0 || fcntl(fd, F_SETFL, fl | O_NONBLOCK) < 0) {
            close(fd);
            fd = -1;
            continue;
        }
#ifdef SO_NOSIGPIPE
        int one = 1;
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
        int rc = connect(fd, a->ai_addr, a->ai_addrlen);
        if (rc < 0 && errno == EINPROGRESS) {
            int w = ai__wait(fd, POLLOUT, mine);
            int err = 0;
            socklen_t el = sizeof err;
            if (w == 1 && getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &el) == 0 && !err)
                rc = 0;
            else
                errno = w == 0 ? ETIMEDOUT : (err ? err : errno);
        }
        if (rc == 0) break;
        snprintf(why, whyn, "%s", strerror(errno));
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

static int ai__send_all(int fd, const char *p, size_t n, double deadline) {
    while (n > 0) {
        ssize_t k = send(fd, p, n, MSG_NOSIGNAL);
        if (k > 0) {
            p += k;
            n -= (size_t)k;
            continue;
        }
        if (k < 0 && errno == EINTR) continue;
        if (k < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            int w = ai__wait(fd, POLLOUT, deadline);
            if (w == 1) continue;
            errno = w == 0 ? ETIMEDOUT : errno;
        }
        return -1;
    }
    return 0;
}

/* Decode a chunked body. 1 = complete, 0 = incomplete, -1 = malformed. */
static int ai__dechunk(const char *s, size_t n, tld_strbuf *out) {
    size_t i = 0;
    tld_strbuf_clear(out);
    for (;;) {
        size_t sz = 0;
        int digits = 0;
        while (i < n && isxdigit((unsigned char)s[i])) {
            int c = tolower((unsigned char)s[i]);
            sz = sz * 16 + (size_t)(c <= '9' ? c - '0' : c - 'a' + 10);
            if (++digits > 8) return -1;
            i++;
        }
        if (i >= n) return 0;
        if (!digits) return -1;
        while (i < n && s[i] != '\n') i++; /* chunk extensions, CR */
        if (i >= n) return 0;
        i++;
        if (sz == 0) return 1; /* trailers are ignored */
        if (sz > n - i || n - i - sz < 2) return 0; /* no sz + 2: it wraps on 32-bit */
        if (tld_strbuf_append_n(out, s + i, sz) != OK) return -2;
        i += sz;
        if (s[i] == '\r') i++;
        if (i >= n) return 0;
        if (s[i] != '\n') return -1;
        i++;
    }
}

/* Message for a non-200 reply: the server's {"error": ...} if it has one. */
static void ai__http_error(const char *method, const char *path, int status,
                           tld_strbuf *body) {
    tld_json_obj *o = NULL;
    const char *msg = NULL;
    if (body && body->len && ai__parse_buf(body, &o) == 0) msg = ai__str(o, "error");
    if (!msg) msg = body ? TLD_STR(body) : "";
    int st = AI_EHTTP;
    if (status == 404 && strstr(msg, "not found")) st = AI_ENOMODEL;
    ai__fail(st, "%s %s: HTTP %d: %.300s", method, path, status, msg);
    tld_json_obj_free(o);
}

/* One HTTP/1.1 request with Connection: close. body NULL = GET. On HTTP 200 returns
 * 0 and the body in *out (caller frees). Otherwise -1 with the status set:
 * AI_ENOSERVER when no connection could be made, AI_EHTTP for the rest. */
static int ai__http(const char *method, const char *path, const char *body,
                    double timeout, tld_strbuf **out) {
    *out = NULL;
    if (ai__host_ok()) return -1;
    double t0 = ai__now(), deadline = t0 + timeout;
    char why[256];
    int fd = ai__connect(TLD_MIN(deadline, t0 + AI__CONNECT_MAX), why, sizeof why);
    if (fd < 0) return ai__noserver(why);

    tld_strbuf *req = NULL, *raw = NULL, *res = NULL;
    int rc = -1;
    char line[1024];
    size_t blen = body ? strlen(body) : 0;
    snprintf(line, sizeof line,
             "%s %s%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: tld_ai.h/" AI_VERSION
             "\r\nAccept: application/json\r\nConnection: close\r\n",
             method, ai__c->base, path, ai__c->authority);
    if (tld_strbuf_alloc(&req, (int)(blen + 512)) != OK || tld_strbuf_append(req, line) != OK)
        goto nomem;
    if (body) {
        snprintf(line, sizeof line,
                 "Content-Type: application/json\r\nContent-Length: %zu\r\n", blen);
        if (tld_strbuf_append(req, line) != OK) goto nomem;
    }
    if (tld_strbuf_append(req, "\r\n") != OK || (body && tld_strbuf_append(req, body) != OK))
        goto nomem;
    if (ai__send_all(fd, TLD_STR(req), (size_t)req->len, deadline)) {
        if (errno == ETIMEDOUT)
            ai__fail(AI_EHTTP, "%s %s: timed out after %g s (TLD_AI_TIMEOUT)", method, path,
                     timeout);
        else
            ai__fail(AI_EHTTP, "%s %s: send failed: %s", method, path, strerror(errno));
        goto done;
    }

    if (tld_strbuf_alloc(&raw, 16384) != OK) goto nomem;
    char buf[16384];
    long hdr = -1, clen = -1;
    int chunked = 0, status = 0, complete = 0;
    for (;;) {
        int w = ai__wait(fd, POLLIN, deadline);
        if (w == 0) {
            ai__fail(AI_EHTTP, "%s %s: timed out after %g s (TLD_AI_TIMEOUT)", method, path,
                     timeout);
            goto done;
        }
        if (w < 0) {
            ai__fail(AI_EHTTP, "%s %s: poll failed: %s", method, path, strerror(errno));
            goto done;
        }
        ssize_t k = recv(fd, buf, sizeof buf - 1, 0);
        if (k < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
            ai__fail(AI_EHTTP, "%s %s: receive failed: %s", method, path, strerror(errno));
            goto done;
        }
        if (k == 0) break; /* server closed the connection */
        if (memchr(buf, 0, (size_t)k)) {
            ai__fail(AI_EHTTP, "%s %s: NUL byte in the reply", method, path);
            goto done;
        }
        buf[k] = 0;
        long from = raw->len > 3 ? raw->len - 3 : 0;
        if (tld_strbuf_append(raw, buf) != OK) goto nomem;
        if (raw->len > AI__MAX_BODY) {
            ai__fail(AI_EHTTP, "%s %s: reply larger than %d bytes", method, path,
                     AI__MAX_BODY);
            goto done;
        }
        if (hdr < 0) {
            char *e = strstr(TLD_STR(raw) + from, "\r\n\r\n");
            if (!e) continue;
            hdr = (long)(e - TLD_STR(raw)) + 4;
            const char *s = TLD_STR(raw);
            if (strncmp(s, "HTTP/1.", 7) || sscanf(s + 8, " %d", &status) != 1) {
                ai__fail(AI_EHTTP, "%s %s: not an HTTP reply", method, path);
                goto done;
            }
            for (const char *h = strstr(s, "\r\n") + 2; h < s + hdr - 2;) {
                const char *eol = strstr(h, "\r\n");
                size_t hl = (size_t)(eol - h);
                if (hl > 15 && !strncasecmp(h, "content-length:", 15))
                    clen = strtol(h + 15, NULL, 10);
                else if (hl > 18 && !strncasecmp(h, "transfer-encoding:", 18)) {
                    for (const char *q = h + 18; q + 7 <= eol; q++)
                        if (!strncasecmp(q, "chunked", 7)) chunked = 1;
                }
                h = eol + 2;
            }
        }
        if (!chunked && clen >= 0 && raw->len - hdr >= clen) {
            complete = 1;
            break;
        }
        if (chunked && raw->len >= 5 && !strcmp(TLD_STR(raw) + raw->len - 5, "0\r\n\r\n")) {
            if (!res && tld_strbuf_alloc(&res, raw->len) != OK) goto nomem;
            int d = ai__dechunk(TLD_STR(raw) + hdr, (size_t)(raw->len - hdr), res);
            if (d == -2) goto nomem;
            if (d == 1) {
                complete = 2;
                break;
            }
        }
    }
    if (hdr < 0) {
        ai__fail(AI_EHTTP, "%s %s: connection closed before a reply", method, path);
        goto done;
    }
    if (complete != 2) {
        if (!res && tld_strbuf_alloc(&res, raw->len - (int)hdr + 1) != OK) goto nomem;
        if (chunked) {
            int d = ai__dechunk(TLD_STR(raw) + hdr, (size_t)(raw->len - hdr), res);
            if (d == -2) goto nomem;
            if (d != 1) {
                ai__fail(AI_EHTTP, "%s %s: truncated or malformed chunked reply", method,
                         path);
                goto done;
            }
        } else {
            if (clen >= 0 && raw->len - hdr < clen) {
                ai__fail(AI_EHTTP, "%s %s: truncated reply (%ld of %ld bytes)", method,
                         path, (long)(raw->len - hdr), clen);
                goto done;
            }
            if (clen >= 0) raw->str[hdr + clen] = 0;
            if (tld_strbuf_append(res, TLD_STR(raw) + hdr) != OK) goto nomem;
        }
    }
    if (status != 200) {
        ai__http_error(method, path, status, res);
        goto done;
    }
    *out = res;
    res = NULL;
    rc = 0;
    goto done;
nomem:
    ai__nomem();
done:
    close(fd);
    tld_strbuf_free(req);
    tld_strbuf_free(raw);
    tld_strbuf_free(res);
    return rc;
}

/* GET with a short deadline, for /api/tags. */
static int ai__get(const char *path, tld_json_obj **out) {
    tld_strbuf *b = NULL;
    *out = NULL;
    if (ai__http("GET", path, NULL, TLD_MIN(ai__c->timeout, 30.0), &b)) return -1;
    int rc = ai__parse_buf(b, out);
    tld_strbuf_free(b);
    if (rc && ai__t_st != AI_ENOMEM)
        return ai__fail(AI_EPARSE, "GET %s: reply is not a JSON object", path);
    return rc ? -1 : 0;
}

/* The server is probed with GET /api/version on the first live call, and again at
 * most every AI__REPROBE seconds while it is down. Racing probes are harmless. */
static int ai__server_up(void) {
    pthread_mutex_lock(&ai__probe_mu);
    int i = ai__probe_slot();
    int up = ai__probes[i].up;
    double at = ai__probes[i].at;
    char why[256];
    ai__setstr(why, sizeof why, ai__probes[i].why);
    pthread_mutex_unlock(&ai__probe_mu);
    if (up == 1) return 0;
    if (up == 0 && ai__now() - at < AI__REPROBE)
        return ai__fail(AI_ENOSERVER,
                        "no Ollama server at %s: %s (start it with `ollama serve`, or set "
                        "TLD_AI_HOST)",
                        AI__HOST(ai__c), why);
    tld_strbuf *b = NULL;
    if (ai__http("GET", "/api/version", NULL, TLD_MIN(ai__c->timeout, AI__CONNECT_MAX),
                 &b)) {
        if (ai__t_st == AI_ENOSERVER) return -1;
        char msg[512];
        ai__setstr(msg, sizeof msg, ai__t_err);
        return ai__noserver(msg);
    }
    tld_strbuf_free(b);
    pthread_mutex_lock(&ai__probe_mu);
    ai__probes[ai__probe_slot()].up = 1;
    pthread_mutex_unlock(&ai__probe_mu);
    return 0;
}

/* ---- models: digests ---------------------------------------------------- */

static pthread_mutex_t ai__dig_mu = PTHREAD_MUTEX_INITIALIZER;
static tld_hash *ai__dig; /* host 0x1f "name:tag" -> digest */

/* "x" and "x:latest" are one model. -1 when the name does not fit in out. */
static int ai__norm(const char *m, char *out, size_t n) {
    const char *slash = strrchr(m, '/');
    int len = snprintf(out, n, strchr(slash ? slash : m, ':') ? "%s" : "%s:latest", m);
    return len < 0 || (size_t)len >= n ? -1 : 0;
}

/* Digests are kept per host: key = host, 0x1f, model. A valid host (at most ~560
 * bytes, see ai__conf_host) and a name that passed ai__norm(.., 300) always fit in
 * 900 bytes. */
static void ai__hkey(const char *norm, char *out, size_t n) {
    snprintf(out, n, "%s\x1f%s", AI__HOST(ai__c), norm);
}

static int ai__dig_lookup(const char *norm, char *digest, size_t dn) {
    const char *v = NULL;
    char hk[900];
    int found = 0;
    ai__hkey(norm, hk, sizeof hk);
    pthread_mutex_lock(&ai__dig_mu);
    if (ai__dig && tld_hash_get(ai__dig, hk, &v) == OK && v) {
        found = 1;
        snprintf(digest, dn, "%s", v);
    }
    pthread_mutex_unlock(&ai__dig_mu);
    return found;
}

/* Digest of model from /api/tags. A failed /api/tags is not cached, and neither is a
 * model that is not listed, so a later pull is seen. */
static int ai__model_info(const char *model, char *digest, size_t dn) {
    char norm[300];
    if (ai__norm(model, norm, sizeof norm))
        return ai__fail(AI_EARG, "model name too long (%zu bytes, at most %zu)", strlen(model),
                        sizeof norm - 8);
    if (ai__dig_lookup(norm, digest, dn)) return 0;
    tld_json_obj *r = NULL;
    tld_json_arr *ms = NULL;
    if (ai__get("/api/tags", &r)) return -1;
    if (tld_json_get_arr(r, "models", &ms) != OK) {
        tld_json_obj_free(r);
        return ai__fail(AI_EPARSE, "GET /api/tags: no models list");
    }
    int rc = 0;
    pthread_mutex_lock(&ai__dig_mu);
    for (int i = 0; i < ms->n && !rc; i++) {
        if (ms->v[i]->type != TLD_JSON_OBJ) continue;
        tld_json_obj *m = ms->v[i]->value.obj;
        const char *name = ai__str(m, "name"), *dg = ai__str(m, "digest");
        if (!name) name = ai__str(m, "model");
        if (!name) continue;
        char nm[300], key[900];
        if (ai__norm(name, nm, sizeof nm)) continue; /* longer than any name we can ask for */
        ai__hkey(nm, key, sizeof key);
        if (tld_hash_add(&ai__dig, key, dg ? dg : "") != OK) rc = ai__nomem();
    }
    pthread_mutex_unlock(&ai__dig_mu);
    tld_json_obj_free(r);
    if (rc) return rc;
    if (ai__dig_lookup(norm, digest, dn)) return 0;
    return ai__fail(AI_ENOMODEL,
                    "model %s is not installed on the Ollama server at %s "
                    "(ollama pull %s)",
                    model, AI__HOST(ai__c), model);
}

/* ---- logged, cached call ------------------------------------------------ */

static void ai__iso_now(char out[64]) {
    time_t t = time(NULL);
    struct tm tm;
    if (!gmtime_r(&t, &tm) || !strftime(out, 64, "%Y-%m-%dT%H:%M:%SZ", &tm)) out[0] = 0;
}

/* Reserve the next occurrence of base. Caller holds ai__cache_mu. */
static int ai__seen_next(const char *base) {
    const char *v = NULL;
    char s[24];
    int n = 0;
    if (ai__seen && tld_hash_get(ai__seen, base, &v) == OK && v) n = atoi(v);
    snprintf(s, sizeof s, "%d", n + 1);
    if (tld_hash_add(&ai__seen, base, s) != OK) return -1;
    return n + 1;
}

/* A call that got no answer gives its occurrence back, so the retry gets the same
 * key and a rerun replays in step. If another thread has reserved a later one
 * meanwhile, the number is left as it is: identical requests are interchangeable,
 * so the gap only costs one extra request on a rerun. */
static void ai__seen_release(const char *base, int occ) {
    const char *v = NULL;
    char s[24];
    pthread_mutex_lock(&ai__cache_mu);
    if (ai__seen && tld_hash_get(ai__seen, base, &v) == OK && v && atoi(v) == occ) {
        snprintf(s, sizeof s, "%d", occ - 1);
        tld_hash_add(&ai__seen, base, s); /* replaces in place, cannot grow */
    }
    pthread_mutex_unlock(&ai__cache_mu);
}

static int ai__write_all(int fd, const char *p, size_t n) {
    while (n > 0) {
        ssize_t k = write(fd, p, n);
        if (k < 0 && errno == EINTR) continue;
        if (k <= 0) return -1;
        p += k;
        n -= (size_t)k;
    }
    return 0;
}

/* Append one line under flock(LOCK_EX). A line cut short by a crashed writer is
 * terminated first, so it costs only itself. Caller holds ai__cache_mu. */
static int ai__append_line(const char *path, tld_strbuf *line) {
    int fd = open(path, O_RDWR | O_APPEND | O_CREAT, 0644);
    if (fd < 0) return -1;
    int rc = -1;
    while (flock(fd, LOCK_EX) < 0)
        if (errno != EINTR) goto out;
    off_t end = lseek(fd, 0, SEEK_END);
    char last = '\n';
    if (end > 0 && (lseek(fd, end - 1, SEEK_SET) < 0 || read(fd, &last, 1) != 1))
        goto unlock;
    if (last != '\n' && ai__write_all(fd, "\n", 1)) goto unlock;
    if (tld_strbuf_append_char(line, '\n') != OK) goto unlock;
    rc = ai__write_all(fd, TLD_STR(line), (size_t)line->len);
unlock:
    flock(fd, LOCK_UN);
out:
    close(fd);
    return rc;
}

/* One cache line in *out: {"key", "endpoint", "model", "digest", "request", "response",
 * "seconds", "created"}. req and resp stay the caller's: they are written as text and
 * never attached to an object that could free them. */
static int ai__cache_entry(tld_strbuf **out, const char *key, const char *ep, const char *model,
                          const char *digest, tld_json_obj *req, tld_json_obj *resp,
                          double seconds, const char *created) {
    tld_json_obj *head = NULL, *tail = NULL;
    tld_strbuf *line = NULL, *rq = NULL, *rs = NULL, *tl = NULL;
    int rc = ai__put(&head, TLD_JSTR("key", key)) || ai__put(&head, TLD_JSTR("endpoint", ep)) ||
             ai__put(&head, TLD_JSTR("model", model)) || ai__put(&head, TLD_JSTR("digest", digest)) ||
             ai__put(&tail, TLD_JNUM("seconds", seconds)) || ai__put(&tail, TLD_JSTR("created", created)) ||
             ai__dump(head, &line, 0) || ai__dump(req, &rq, 0) || ai__dump(resp, &rs, 0) ||
             ai__dump(tail, &tl, 0);
    if (!rc) {
        /* line and tl are compact {...}: drop line's '}' and tl's '{' and join */
        line->str[--line->len] = 0;
        const char *sep = line->len > 1 ? "," : "";
        if (tld_strbuf_append(line, sep) != OK || tld_strbuf_append(line, "\"request\":") != OK ||
            tld_strbuf_append(line, TLD_STR(rq)) != OK || tld_strbuf_append(line, ",\"response\":") != OK ||
            tld_strbuf_append(line, TLD_STR(rs)) != OK || tld_strbuf_append(line, ",") != OK ||
            tld_strbuf_append(line, TLD_STR(tl) + 1) != OK)
            rc = ai__nomem();
    }
    tld_json_obj_free(head);
    tld_json_obj_free(tail);
    tld_strbuf_free(rq);
    tld_strbuf_free(rs);
    tld_strbuf_free(tl);
    if (rc) {
        tld_strbuf_free(line);
        return -1;
    }
    *out = line;
    return 0;
}

static void ai__cache_store(const char *key, const char *ep, const char *model,
                            const char *digest, tld_json_obj *req, tld_json_obj *resp,
                            double seconds) {
    char created[64];
    ai__iso_now(created);
    tld_strbuf *line = NULL;
    int err = ai__t_st;
    char msg[512];
    ai__setstr(msg, sizeof msg, ai__t_err);
    if (ai__cache_entry(&line, key, ep, model, digest, req, resp, seconds, created)) {
        WARNING_MSG("tld_ai: cannot write a cache entry: out of memory");
    } else {
        char path[1024] = "";
        int rc = 0, e = 0;
        pthread_mutex_lock(&ai__cache_mu);
        if (ai__cache_file) {
            ai__setstr(path, sizeof path, ai__cache_file);
            rc = ai__append_line(ai__cache_file, line);
            e = errno;
        }
        pthread_mutex_unlock(&ai__cache_mu);
        if (rc) WARNING_MSG("tld_ai: cannot append to cache %s: %s", path, strerror(e));
    }
    tld_strbuf_free(line);
    ai__t_st = err; /* the call itself succeeded */
    ai__setstr(ai__t_err, sizeof ai__t_err, msg);
}

/* Send req to endpoint for model, or replay it from the cache. Returns the parsed
 * reply (caller frees) or NULL with the status set. req stays the caller's. */

static tld_json_obj *ai__call(const char *ep, const char *model, tld_json_obj *req,
                              int fresh) {
    memset(&ai__t_last, 0, sizeof ai__t_last);
    ai__t_last.model = ai__kdup(model);
    ai__t_last.host = ai__kdup(ai__c->str[AI__S_HOST]);
    if ((model && !ai__t_last.model) || (ai__c->str[AI__S_HOST] && !ai__t_last.host))
        return NULL; /* out of memory, status set */
    DEBUG_MSG("ai: %s %s @ %s", ep, model, ai__c->str[AI__S_HOST]);
    if (ai__host_ok() || ai__server_up()) return NULL;
    if (ai__c->cache_on && ai__cache_open()) return NULL;
    char dg[200];
    if (ai__model_info(model, dg, sizeof dg)) return NULL;
    tld_strbuf *body = NULL;
    if (ai__dump(req, &body, 0)) return NULL;

    char base[24], key[48], *hit = NULL;
    snprintf(base, sizeof base, "%016llx",
             (unsigned long long)ai__fnv(ai__fnv(ai__fnv(AI__FNV0, ep), dg), TLD_STR(body)));
    pthread_mutex_lock(&ai__cache_mu);
    int occ = ai__seen_next(base);
    if (occ > 0) {
        snprintf(key, sizeof key, "%s#%d", base, occ);
        const char *v = NULL;
        if (!fresh && !ai__c->fresh && ai__c->cache_on && ai__cache &&
            tld_hash_get(ai__cache, key, &v) == OK && v && !(hit = strdup(v)))
            occ = -1;
    }
    pthread_mutex_unlock(&ai__cache_mu);
    if (occ < 0) {
        tld_strbuf_free(body);
        ai__nomem();
        return NULL;
    }

    tld_json_obj *resp = NULL;
    if (hit) {
        int rc = ai__parse(hit, &resp);
        free(hit);
        tld_strbuf_free(body);
        if (rc) {
            if (ai__t_st == AI_OK) ai__fail(AI_EPARSE, "%s: cached reply is damaged", ep);
            ai__seen_release(base, occ);
            return NULL;
        }
        ai__t_last.from_cache = 1;
        DEBUG_MSG("ai: %s answered from the cache", ep);
        return resp;
    }

    double t0 = ai__now();
    tld_strbuf *rb = NULL;
    int rc = ai__http("POST", ep, TLD_STR(body), ai__c->timeout, &rb);
    ai__t_last.seconds = ai__now() - t0;
    if (!rc && ai__parse_buf(rb, &resp))
        rc = ai__t_st == AI_ENOMEM
                 ? -1
                 : ai__fail(AI_EPARSE, "POST %s: reply is not a JSON object", ep);
    tld_strbuf_free(rb);
    tld_strbuf_free(body);
    if (rc) {
        ai__seen_release(base, occ);
        DEBUG_MSG("ai: %s failed after %.1f s: %s", ep, ai__t_last.seconds, ai_error());
        return NULL;
    }
    DEBUG_MSG("ai: %s answered in %.1f s", ep, ai__t_last.seconds);
    if (ai__c->cache_on) ai__cache_store(key, ep, model, dg, req, resp,
                                          ai__t_last.seconds);
    return resp;
}

/* ---- request building --------------------------------------------------- */

/* Read the whole file at path into a new NUL-terminated buffer (*data, *len bytes,
 * free with free()). Returns 0, or 1 if the file does not exist and missing_ok is set,
 * or -1 with the error set. what names the file in messages ("image", "review"). */
/* The file at path in a new strbuf: 0, 1 when it does not exist and missing_ok, or a
 * failure status (AI_EIO, AI_ENOMEM). */
static int ai__slurp(const char *path, const char *what, int missing_ok, tld_strbuf **out) {
    *out = NULL;
    if (missing_ok && !tld_path_exists(path)) return 1;
    errno = 0;
    if (tld_path_read(path, out) != OK) {
        /* tld's message keeps errno */
        if (errno == ENOMEM) return ai__nomem();
        return ai__fail(AI_EIO, "cannot read %s %s", what, path);
    }
    if ((*out)->len > AI__MAX_BODY) {
        tld_strbuf_free(*out);
        *out = NULL;
        return ai__fail(AI_EIO, "%s %s is too large", what, path);
    }
    return 0;
}

static int ai__b64_file(const char *path, char **out) {
    *out = NULL;
    tld_strbuf *d = NULL;
    if (ai__slurp(path, "image", 0, &d)) return -1;
    size_t got = (size_t)d->len;
    char *o = malloc(4 * ((got + 2) / 3) + 1);
    if (!o) {
        tld_strbuf_free(d);
        return ai__nomem();
    }
    static const char t[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const unsigned char *u = (const unsigned char *)d->str;
    char *q = o;
    for (size_t i = 0; i < got; i += 3) {
        uint32_t v = (uint32_t)u[i] << 16 | (i + 1 < got ? (uint32_t)u[i + 1] << 8 : 0) |
                     (i + 2 < got ? u[i + 2] : 0);
        *q++ = t[v >> 18 & 63];
        *q++ = t[v >> 12 & 63];
        *q++ = i + 1 < got ? t[v >> 6 & 63] : '=';
        *q++ = i + 2 < got ? t[v & 63] : '=';
    }
    *q = 0;
    tld_strbuf_free(d);
    *out = o;
    return 0;
}

static int ai__add_images(tld_json_obj **obj, const ai_opts *o) {
    if (!o || !o->images) return 0;
    tld_json_arr *a = NULL;
    for (const char *const *p = o->images; *p; p++) {
        char *e = NULL;
        if (ai__b64_file(*p, &e) || ai__push(&a, TLD_JSTR(NULL, e))) {
            free(e);
            tld_json_arr_free(a);
            return -1;
        }
        free(e);
    }
    return ai__put(obj, TLD_JARR("images", a));
}

/* Model options, plus TLD_AI_NUM_CTX when the caller did not set num_ctx. */
static int ai__add_options(tld_json_obj **req, const ai_opts *o) {
    tld_json_obj *op = NULL;
    int rc = 0;
    if (o->temperature.set) rc = rc || ai__put(&op, TLD_JNUM("temperature", o->temperature.v));
    if (o->top_p.set) rc = rc || ai__put(&op, TLD_JNUM("top_p", o->top_p.v));
    if (o->min_p.set) rc = rc || ai__put(&op, TLD_JNUM("min_p", o->min_p.v));
    if (o->top_k.set) rc = rc || ai__put(&op, TLD_JINT("top_k", o->top_k.v));
    if (o->seed.set) rc = rc || ai__put(&op, TLD_JINT("seed", o->seed.v));
    if (o->num_ctx.set)
        rc = rc || ai__put(&op, TLD_JINT("num_ctx", o->num_ctx.v));
    else if (ai__c->num_ctx > 0)
        rc = rc || ai__put(&op, TLD_JINT("num_ctx", ai__c->num_ctx));
    if (o->num_predict.set) rc = rc || ai__put(&op, TLD_JINT("num_predict", o->num_predict.v));
    if (!rc && o->stop) {
        tld_json_arr *a = NULL;
        for (const char *const *p = o->stop; *p && !rc; p++) rc = ai__push(&a, TLD_JSTR(NULL, *p));
        if (rc && a) tld_json_arr_free(a);
        if (!rc) rc = ai__put(&op, TLD_JARR("stop", a));
    }
    if (!rc && o->extra_options) {
        tld_json_obj *x = NULL;
        if (ai__parse(o->extra_options, &x)) {
            if (ai__t_st == AI_OK)
                rc = ai__fail(AI_EARG, "extra_options is not a JSON object: %.200s",
                              o->extra_options);
            else
                rc = -1;
        }
        if (!rc && !op && tld_json_obj_alloc(&op) != OK) rc = ai__nomem();
        /* a key already present (built-in or earlier) is replaced, a new one appended */
        for (int i = 0; !rc && x && i < x->n; i++) {
            const char *k = TLD_STR(x->key[i]);
            if (!*k || strpbrk(k, ".[")) {
                rc = ai__fail(AI_EARG, "extra_options: option name \"%.100s\" is empty or contains '.' or '['", k);
                break;
            }
            if (tld_json_set_val(op, k, x->v[i]) != OK) rc = ai__nomem();
        }
        tld_json_obj_free(x);
    }
    if (rc) {
        tld_json_obj_free(op);
        return -1;
    }
    if (op && op->n > 0) {
        if (ai__put(req, TLD_JOBJ("options", op))) return -1;
    } else if (op) {
        tld_json_obj_free(op);
    }
    if (o->think) {
        if (!strcmp(o->think, "true") || !strcmp(o->think, "false"))
            rc = ai__put(req, TLD_JBOOL("think", !strcmp(o->think, "true")));
        else
            rc = ai__put(req, TLD_JSTR("think", o->think));
    }
    if (!rc && o->keep_alive) rc = ai__put(req, TLD_JSTR("keep_alive", o->keep_alive));
    return rc;
}

/* One chat turn. format (owned, may be NULL) is a JSON schema. Returns malloc'd
 * content or NULL. */
static char *ai__chat(const ai_opts *call, const char *prompt, tld_json_obj *format) {
    ai_opts merged; /* the call's options over the defaults' options */
    ai__merge(call, ai__c->opts, &merged);
    const ai_opts *o = &merged;
    const char *model = o->model ? o->model : ai__c->str[AI__S_LLM];
    tld_json_obj *req = NULL, *sys = NULL, *user = NULL;
    tld_json_arr *msgs = NULL;
    char *out = NULL;
    int rc = ai__put(&req, TLD_JSTR("model", model));
    if (!rc && o->system) {
        rc = ai__put(&sys, TLD_JSTR("role", "system")) || ai__put(&sys, TLD_JSTR("content", o->system));
        if (rc && sys) tld_json_obj_free(sys);
        if (!rc) rc = ai__push(&msgs, TLD_JOBJ(NULL, sys));
    }
    if (!rc) {
        rc = ai__put(&user, TLD_JSTR("role", "user")) || ai__put(&user, TLD_JSTR("content", prompt)) ||
             ai__add_images(&user, o);
        if (rc && user) tld_json_obj_free(user);
        if (!rc) rc = ai__push(&msgs, TLD_JOBJ(NULL, user));
    }
    if (rc && msgs) tld_json_arr_free(msgs);
    if (!rc) rc = ai__put(&req, TLD_JARR("messages", msgs));
    if (!rc) rc = ai__put(&req, TLD_JBOOL("stream", 0));
    if (!rc && format) {
        rc = ai__put(&req, TLD_JOBJ("format", format));
        format = NULL;
    }
    if (!rc) rc = ai__add_options(&req, o);
    tld_json_obj_free(format);
    if (!rc) {
        tld_json_obj *r = ai__call("/api/chat", model, req, o->fresh);
        if (r) {
            const char *c = ai__str(r, "message.content");
            const char *th = ai__str(r, "message.thinking");
            ai__t_last.prompt_tokens = ai__long(r, "prompt_eval_count");
            ai__t_last.output_tokens = ai__long(r, "eval_count");
            if (th && *th) ai__t_last.thinking = ai__kdup(th);
            if (!c)
                ai__fail(AI_EPARSE, "/api/chat: no message content in the reply");
            else if (!(out = strdup(c)))
                ai__nomem();
            tld_json_obj_free(r);
        }
    }
    tld_json_obj_free(req);
    return out;
}

/* ---- public: text ------------------------------------------------------- */

static char *ai__text(const ai_opts *o, const char *fmt, va_list ap) {
    if (!fmt) {
        ai__fail(AI_EARG, "ai_text: NULL format");
        return NULL;
    }
    char *p = ai__vfmt(fmt, ap);
    if (!p || ai__enter()) {
        free(p);
        return NULL;
    }
    char *r = ai__chat(o, p, NULL);
    ai__leave();
    free(p);
    return ai__keep(r);
}

const char *(ai_text_with)(const ai_opts *o, const char *fmt, ...) {
    ai__begin();
    va_list ap;
    va_start(ap, fmt);
    char *r = ai__text(o, fmt, ap);
    va_end(ap);
    return r;
}

const char *(ai_text)(const char *fmt, ...) {
    ai__begin();
    va_list ap;
    va_start(ap, fmt);
    char *r = ai__text(NULL, fmt, ap);
    va_end(ap);
    return r;
}

/* ---- public: structs ---------------------------------------------------- */

static tld_json_obj *ai__schema(const ai_type *t) {
    static const char *js[] = {"string", "number", "integer", "boolean"};
    tld_json_obj *s = NULL, *props = NULL;
    tld_json_arr *req = NULL;
    int rc = ai__put(&s, TLD_JSTR("type", "object"));
    if (!rc && tld_json_obj_alloc(&props) != OK) rc = ai__nomem();
    for (int i = 0; !rc && i < t->n; i++) {
        tld_json_obj *p = NULL;
        rc = ai__put(&p, TLD_JSTR("type", js[t->f[i].kind])) ||
             ai__put(&p, TLD_JSTR("description", t->f[i].desc ? t->f[i].desc : ""));
        if (rc && p) tld_json_obj_free(p);
        if (!rc) rc = ai__put(&props, TLD_JOBJ(t->f[i].name, p));
        if (!rc) rc = ai__push(&req, TLD_JSTR(NULL, t->f[i].name));
    }
    if (!rc) {
        rc = ai__put(&s, TLD_JOBJ("properties", props));
        props = NULL;
    }
    if (!rc && req) {
        rc = ai__put(&s, TLD_JARR("required", req));
        req = NULL;
    }
    tld_json_obj_free(props);
    tld_json_arr_free(req);
    if (rc) {
        tld_json_obj_free(s);
        return NULL;
    }
    return s;
}

/* Unpack text into the scratch struct. Strings are malloc'd; the caller frees them
 * on failure and moves them into the arena on success. */
static int ai__unpack(void *out, const ai_type *t, const char *text) {
    tld_json_obj *j = NULL;
    memset(out, 0, t->size);
    if (ai__parse(text, &j)) {
        if (ai__t_st == AI_OK || ai__t_st == AI_EPARSE)
            ai__fail(AI_EPARSE, "%s: reply is not a JSON object", t->name);
        return -1;
    }
    int rc = 0;
    for (int i = 0; i < t->n && !rc; i++) {
        const ai_field *f = &t->f[i];
        char *dst = (char *)out + f->off;
        const char *s;
        double d;
        int64_t v;
        int b;
        switch (f->kind) {
        case AI_T_str:
            if (tld_json_get_str(j, f->name, &s) != OK)
                rc = -1;
            else if (!(*(const char **)dst = strdup(s)))
                rc = ai__nomem();
            break;
        case AI_T_num:
            if (tld_json_get_num(j, f->name, &d) != OK) rc = -1;
            else *(double *)dst = d;
            break;
        case AI_T_int:
            if (tld_json_get_int(j, f->name, &v) != OK || v < LONG_MIN || v > LONG_MAX)
                rc = -1;
            else
                *(long *)dst = (long)v;
            break;
        case AI_T_bool:
            if (tld_json_get_bool(j, f->name, &b) != OK) rc = -1;
            else *(int *)dst = b;
            break;
        }
        if (rc && ai__t_st != AI_ENOMEM)
            ai__fail(AI_ESCHEMA, "%s.%s missing or wrong type", t->name, f->name);
    }
    tld_json_obj_free(j);
    return rc;
}

static void ai__free_strs(void *s, const ai_type *t) {
    for (int i = 0; i < t->n; i++)
        if (t->f[i].kind == AI_T_str) {
            const char **p = (const char **)((char *)s + t->f[i].off);
            free((void *)*p);
            *p = NULL;
        }
}

int ai__fill(void *out, const ai_type *t, const ai_opts *o, const char *fmt, ...) {
    ai__begin();
    if (!out || !t) {
        ai__fail(AI_EARG, "ai_fill: NULL argument");
        return -1;
    }
    memset(out, 0, t->size);
    if (!fmt) {
        ai__fail(AI_EARG, "ai_fill: NULL format");
        return -1;
    }
    if (ai__enter()) return -1;
    ai_opts merged; /* retries and max_retries may come from the defaults */
    ai__merge(o, ai__c->opts, &merged);
    o = &merged;
    va_list ap;
    va_start(ap, fmt);
    char *task = ai__vfmt(fmt, ap);
    va_end(ap);
    tld_json_obj *sch = task ? ai__schema(t) : NULL;
    tld_strbuf *sch_txt = NULL;
    char *prompt = NULL;
    void *tmp = NULL;
    int rc = -1;
    if (!sch || ai__dump(sch, &sch_txt, 0)) goto out;
    size_t pn = strlen(task) + (size_t)sch_txt->len + 64;
    if (!(prompt = malloc(pn)) || !(tmp = calloc(1, t->size))) {
        ai__nomem();
        goto out;
    }
    snprintf(prompt, pn, "%s\n\nRespond with JSON matching this schema:\n%s", task,
             TLD_STR(sch_txt));
    int tries = 1 + (o->max_retries.set ? (o->max_retries.v > 0 ? (int)o->max_retries.v : 0)
                                            : (o->retries > 0 ? o->retries : 2));
    for (int k = 0; k < tries && rc; k++) {
        tld_json_obj *fmt_copy = NULL;
        if (tld_json_obj_copy(&fmt_copy, sch) != OK) {
            ai__nomem();
            break;
        }
        ai__begin();
        char *r = ai__chat(o, prompt, fmt_copy);
        if (r) {
            rc = ai__unpack(tmp, t, r);
            if (rc) ai__free_strs(tmp, t);
        }
        free(r);
        if (ai__t_st == AI_ENOMEM) break;
    }
    if (!rc) { /* move the strings into the arena; the struct is complete */
        for (int i = 0; i < t->n; i++)
            if (t->f[i].kind == AI_T_str) {
                const char **p = (const char **)((char *)tmp + t->f[i].off);
                if (!ai__keep((void *)*p)) rc = -1;
                if (rc) *p = NULL;
            }
        if (!rc) memcpy(out, tmp, t->size);
    }
out:
    free(task);
    free(prompt);
    free(tmp);
    tld_strbuf_free(sch_txt);
    tld_json_obj_free(sch);
    ai__leave();
    return rc ? -1 : 0;
}

/* ---- public: decisions -------------------------------------------------- */

/* Sends one question about state to the decision model. Returns the whole reply
 * (caller frees) and sets *ans to answers.<name> inside it. criteria is owned. */
static tld_json_obj *ai__decide(const ai_opts *call, const char *name, const char *type,
                                const char *question, tld_json_decoration criteria,
                                const char *state, tld_json_obj **ans) {
    *ans = NULL;
    /* text options never reach the decision endpoint; of the defaults' options only
     * .fresh applies */
    ai_opts dec = call ? *call : (ai_opts){0};
    if (!dec.fresh && ai__c->opts) dec.fresh = ai__c->opts->fresh;
    const ai_opts *o = &dec;
    if (!question || !state) {
        ai__drop(criteria);
        ai__fail(AI_EARG, "decision: NULL question or state");
        return NULL;
    }
    char path[300]; /* answers.<name> */
    if (snprintf(path, sizeof path, "answers.%s", name) >= (int)sizeof path) {
        ai__drop(criteria);
        ai__fail(AI_EARG, "decision name too long: %.40s...", name);
        return NULL;
    }
    const char *model = o->model ? o->model : ai__c->str[AI__S_DECIDER];
    tld_json_obj *req = NULL, *q = NULL, *qs = NULL, *st = NULL, *r = NULL;
    int rc = ai__put(&req, TLD_JSTR("model", model));
    const char *p = state;
    while (isspace((unsigned char)*p)) p++;
    if (!rc && *p == '{' && ai__parse(state, &st) == 0) /* JSON state is sent as JSON */
        rc = ai__put(&req, TLD_JOBJ("state", st));
    else if (!rc) {
        if (ai__t_st == AI_ENOMEM) rc = -1;
        else rc = ai__put(&req, TLD_JSTR("state", state));
    }
    if (!rc) {
        rc = ai__put(&q, TLD_JSTR("type", type)) || ai__put(&q, TLD_JSTR("instructions", question));
        criteria.key = (char *)"criteria";
        if (!rc) rc = ai__put(&q, criteria);
        else ai__drop(criteria);
        criteria.value = NULL;
        if (rc && q) tld_json_obj_free(q);
        if (!rc) rc = ai__put(&qs, TLD_JOBJ(name, q));
        if (!rc) rc = ai__put(&req, TLD_JOBJ("questions", qs));
    }
    ai__drop(criteria);
    if (!rc) rc = ai__add_images(&req, o);
    if (!rc) {
        r = ai__call("/v1/systemone", model, req, o->fresh);
        if (r) {
            ai__t_last.prompt_tokens = ai__long(r, "usage.input_tokens");
            ai__t_last.output_tokens = ai__long(r, "usage.output_tokens");
            if (tld_json_get_obj(r, path, ans) != OK) {
                ai__fail(AI_EPARSE, "/v1/systemone: no answer for %s", name);
                tld_json_obj_free(r);
                r = NULL;
            }
        }
    }
    tld_json_obj_free(req);
    return r;
}

static int ai__choose_impl(const char *name, const char *question, const ai_label *L,
                           int n, const char *state, const ai_opts *o, double *confidence,
                           double *p) {
    tld_json_obj *crit = NULL, *a = NULL;
    if (!L || n <= 0) return ai__fail(AI_EARG, "%s: no labels", name ? name : "choice");
    for (int i = 0; i < n; i++)
        if (ai__put(&crit, TLD_JSTR(L[i].name, L[i].desc))) {
            tld_json_obj_free(crit);
            return -1;
        }
    tld_json_obj *r = ai__decide(o, name, "choice", question, TLD_JOBJ(NULL, crit), state, &a);
    if (!r) return -1;
    const char *c = ai__str(a, "choice");
    tld_json_obj *pr = NULL;
    int have_p = tld_json_get_obj(a, "probabilities", &pr) == OK, idx = -1;
    for (int i = 0; i < n; i++) {
        if (c && !strcmp(c, L[i].name)) idx = i;
        p[i] = have_p ? ai__num(pr, L[i].name, 0) : 0;
    }
    if (!c && have_p) /* no choice field: take the most probable label */
        for (int i = 0; i < n; i++)
            if (idx < 0 || p[i] > p[idx]) idx = i;
    *confidence = ai__num(a, "confidence", idx >= 0 ? p[idx] : 0);
    if (!c && !have_p)
        ai__fail(AI_EPARSE, "%s: the answer has neither choice nor probabilities", name);
    else if (idx < 0)
        ai__fail(AI_ESCHEMA, "%s: model chose unknown label '%s'", name, c);
    tld_json_obj_free(r);
    return idx;
}

static int ai__rate_impl(const char *name, const char *question, const ai_label *L, int n,
                         const char *state, const ai_opts *o, double *score,
                         double *confidence) {
    tld_json_arr *crit = NULL;
    tld_json_obj *a = NULL;
    for (int i = 0; i < n; i++)
        if (ai__push(&crit, TLD_JSTR(NULL, L[i].desc))) {
            tld_json_arr_free(crit);
            return -1;
        }
    tld_json_obj *r = ai__decide(o, name, "score", question, TLD_JARR(NULL, crit), state, &a);
    if (!r) return -1;
    double s = ai__num(a, "score", NAN);
    *confidence = ai__num(a, "confidence", 0);
    tld_json_obj_free(r);
    if (isnan(s)) {
        ai__fail(AI_EPARSE, "%s: no score in the answer", name);
        return -1;
    }
    *score = s;
    long lv = lround(s);
    return (int)(lv < 0 ? 0 : lv >= n ? n - 1 : lv);
}

static double ai__yesno_impl(const ai_opts *o, const char *question, const char *state) {
    tld_json_obj *a = NULL;
    tld_json_decoration none = TLD_JNULL(NULL);
    none.value = NULL; /* no criteria for noul */
    tld_json_obj *r = ai__decide(o, "q", "noul", question, none, state, &a);
    if (!r) return -1;
    double v = ai__num(a, "noul", NAN);
    tld_json_obj_free(r);
    if (isnan(v)) {
        ai__fail(AI_EPARSE, "yes/no: no probability in the answer");
        return -1;
    }
    if (v < 0 || v > 1) {
        ai__fail(AI_ESCHEMA, "yes/no: probability %g outside [0,1]", v);
        return -1;
    }
    return v;
}

int ai__choose(const char *name, const char *question, const ai_label *L, int n,
               const char *state, const ai_opts *o, double *confidence, double *p) {
    ai__begin();
    if (ai__enter()) return -1;
    int r = ai__choose_impl(name, question, L, n, state, o, confidence, p);
    ai__leave();
    return r;
}

int ai__rate(const char *name, const char *question, const ai_label *L, int n,
             const char *state, const ai_opts *o, double *score, double *confidence) {
    ai__begin();
    if (ai__enter()) return -1;
    int r = ai__rate_impl(name, question, L, n, state, o, score, confidence);
    ai__leave();
    return r;
}

double ai_yesno_with(const ai_opts *o, const char *question, const char *state) {
    ai__begin();
    if (ai__enter()) return -1;
    double r = ai__yesno_impl(o, question, state);
    ai__leave();
    return r;
}

double ai_yesno(const char *question, const char *state) {
    return ai_yesno_with(NULL, question, state);
}

/* ---- public: embeddings ------------------------------------------------- */

static ai_vecs ai__embed_impl(const ai_opts *call, const char *const *texts, int n) {
    ai_vecs out = {0, 0, NULL};
    /* text options never come from the defaults here; dimensions default to
     * embed_dimensions, and no_truncate and fresh may come from the defaults' opts */
    ai_opts emb = call ? *call : (ai_opts){0};
    const ai_opts *d = ai__c->opts;
    if (!emb.dimensions.set && ai__c->dims > 0) emb.dimensions = AI_I(ai__c->dims);
    if (!emb.dimensions.set && d && d->dimensions.set) emb.dimensions = d->dimensions;
    if (!emb.no_truncate && d) emb.no_truncate = d->no_truncate;
    if (!emb.fresh && d) emb.fresh = d->fresh;
    const ai_opts *o = &emb;
    if (!texts || n <= 0) {
        ai__fail(AI_EARG, "ai_embed_all: no texts");
        return out;
    }
    const char *model = o->model ? o->model : ai__c->str[AI__S_EMBEDDER];
    tld_json_obj *req = NULL;
    tld_json_arr *in = NULL;
    int rc = ai__put(&req, TLD_JSTR("model", model));
    for (int i = 0; !rc && i < n; i++) {
        if (!texts[i]) rc = ai__fail(AI_EARG, "ai_embed_all: text %d is NULL", i);
        else rc = ai__push(&in, TLD_JSTR(NULL, texts[i]));
    }
    if (rc && in) tld_json_arr_free(in);
    if (!rc) rc = ai__put(&req, TLD_JARR("input", in));
    if (!rc && o->no_truncate) rc = ai__put(&req, TLD_JBOOL("truncate", 0));
    if (!rc && o->dimensions.set) rc = ai__put(&req, TLD_JINT("dimensions", o->dimensions.v));
    if (!rc) rc = ai__add_options(&req, o);
    tld_json_obj *r = rc ? NULL : ai__call("/api/embed", model, req, o->fresh);
    tld_json_obj_free(req);
    if (!r) return out;
    ai__t_last.prompt_tokens = ai__long(r, "prompt_eval_count");
    tld_json_arr *rows = NULL;
    int got = tld_json_get_arr(r, "embeddings", &rows) == OK ? rows->n : 0;
    int dim = got > 0 && rows->v[0]->type == TLD_JSON_ARR && rows->v[0]->value.arr
                  ? rows->v[0]->value.arr->n
                  : 0;
    float *x = NULL;
    if (got != n || dim <= 0) {
        ai__fail(AI_EPARSE, "/api/embed: expected %d vectors, got %d", n, got);
    } else if (!(x = malloc((size_t)n * (size_t)dim * sizeof *x))) {
        ai__nomem();
    } else {
        int ok = 1;
        for (int i = 0; i < n && ok; i++) {
            tld_json_val *rv = rows->v[i];
            if (rv->type != TLD_JSON_ARR || !rv->value.arr || rv->value.arr->n != dim) {
                ok = 0;
                ai__fail(AI_EPARSE, "/api/embed: vectors of unequal length");
                break;
            }
            for (int j = 0; j < dim; j++) {
                tld_json_val *v = rv->value.arr->v[j];
                if (v->type == TLD_JSON_DBL)
                    x[(size_t)i * dim + j] = (float)v->value.d_num;
                else if (v->type == TLD_JSON_INT)
                    x[(size_t)i * dim + j] = (float)v->value.i_num;
                else {
                    ok = 0;
                    ai__fail(AI_EPARSE, "/api/embed: non-numeric vector element");
                    break;
                }
            }
        }
        if (ok && (x = ai__keep(x)))
            out = (ai_vecs){n, dim, x};
        else if (!ok)
            free(x);
    }
    tld_json_obj_free(r);
    return out;
}

ai_vecs ai_embed_all_with(const ai_opts *o, const char *const *texts, int n) {
    ai__begin();
    ai_vecs out = {0, 0, NULL};
    if (ai__enter()) return out;
    out = ai__embed_impl(o, texts, n);
    ai__leave();
    return out;
}

ai_vecs ai_embed_all(const char *const *texts, int n) {
    return ai_embed_all_with(NULL, texts, n);
}

ai_vec ai_embed_with(const ai_opts *o, const char *text) {
    ai_vecs v = ai_embed_all_with(o, &text, 1);
    return (ai_vec){v.dim, v.x};
}

ai_vec ai_embed(const char *text) { return ai_embed_with(NULL, text); }

double ai_cosine(ai_vec a, ai_vec b) {
    if (!a.x || !b.x || a.dim != b.dim) return -2;
    double ab = 0, aa = 0, bb = 0;
    for (int i = 0; i < a.dim; i++) {
        ab += (double)a.x[i] * b.x[i];
        aa += (double)a.x[i] * a.x[i];
        bb += (double)b.x[i] * b.x[i];
    }
    return aa > 0 && bb > 0 ? ab / sqrt(aa * bb) : 0;
}

int ai_nearest(ai_vec q, ai_vecs m, int k, int *idx, double *score) {
    if (!q.x || !m.x || q.dim != m.dim || k <= 0 || !idx || !score) return 0;
    int have = 0;
    for (int i = 0; i < m.n; i++) {
        double s = ai_cosine(q, ai_row(m, i));
        if (have == k && s <= score[k - 1]) continue;
        int j = have < k ? have++ : k - 1; /* insertion into the sorted top k */
        while (j > 0 && score[j - 1] < s) {
            score[j] = score[j - 1];
            idx[j] = idx[j - 1];
            j--;
        }
        score[j] = s;
        idx[j] = i;
    }
    return have;
}

/* ---- public: parallel work ---------------------------------------------- */

typedef struct {
    int n;
    atomic_int next;
    void (*fn)(int, void *);
    void *ctx;
} ai__par_job;

static void *ai__par_worker(void *a) {
    ai__par_job *p = a;
    for (int i; (i = atomic_fetch_add(&p->next, 1)) < p->n;) p->fn(i, p->ctx);
    return NULL;
}

void ai_parallel(int n, int threads, void (*fn)(int i, void *ctx), void *ctx) {
    if (n <= 0 || !fn) return;
    if (threads < 1) threads = 1;
    if (threads > n) threads = n;
    ai__par_job job = {.n = n, .fn = fn, .ctx = ctx};
    atomic_init(&job.next, 0);
    pthread_t *t = threads > 1 ? malloc((size_t)threads * sizeof *t) : NULL;
    int started = 0;
    for (int i = 1; t && i < threads; i++) /* without memory, run serially */
        if (pthread_create(&t[started], NULL, ai__par_worker, &job) == 0) started++;
    ai__par_worker(&job); /* the calling thread works too */
    for (int i = 0; i < started; i++) pthread_join(t[i], NULL);
    free(t);
}

/* ---- public: human review ----------------------------------------------- */

static pthread_mutex_t ai__review_mu = PTHREAD_MUTEX_INITIALIZER;

/* A review file as a strbuf: 0, 1 if it does not exist, -1 on error. */
static int ai__read_file(const char *path, tld_strbuf **out) {
    tld_strbuf *b = NULL;
    *out = NULL;
    int rc = ai__slurp(path, "review", 1, &b);
    if (rc) return rc;
    if (memchr(b->str, 0, (size_t)b->len)) {
        tld_strbuf_free(b);
        return ai__fail(AI_EPARSE, "review %s contains a NUL byte", path);
    }
    *out = b;
    return 0;
}

static const char *ai__review_impl(const char *what, const char *payload) {
    if (!what || !payload) {
        ai__fail(AI_EARG, "ai_review: NULL argument");
        return NULL;
    }
    char path[1200];
    const char *dir = ai__c->str[AI__S_REVIEWS];
    if (snprintf(path, sizeof path, "%s/%016llx.json", dir,
                 (unsigned long long)ai__fnv(ai__fnv(AI__FNV0, what), payload)) >= (int)sizeof path) {
        ai__fail(AI_EARG, "ai_review: review directory name too long (%zu bytes)", strlen(dir));
        return NULL;
    }
    const char *ans = NULL;
    pthread_mutex_lock(&ai__review_mu);
    tld_strbuf *b = NULL;
    int rc = ai__read_file(path, &b);
    if (rc == 0) {
        tld_json_obj *o = NULL;
        if (ai__parse_buf(b, &o)) {
            if (ai__t_st == AI_OK) ai__fail(AI_EPARSE, "review %s is not valid JSON", path);
        } else {
            const char *st = ai__str(o, "status"), *a = ai__str(o, "answer");
            if (st && !strcmp(st, "done") && a)
                ans = ai__kdup(a);
            else if (st && !strcmp(st, "done"))
                ai__fail(AI_ESCHEMA, "review %s is done but has no string answer", path);
            else {
                ai__fail(AI_OK, "review still open: %s (%s)", path, what);
                LOG_MSG("tld_ai: review still open: %s: %s", path, what);
            }
        }
        tld_json_obj_free(o);
    } else if (rc == 1) { /* new review */
        tld_json_obj *o = NULL;
        tld_strbuf *txt = NULL;
        char tmp[1300]; /* path (< 1200) + ".tmp" + a pid always fits */
        snprintf(tmp, sizeof tmp, "%s.tmp%ld", path, (long)getpid());
        if (mkdir(dir, 0755) < 0 && errno != EEXIST)
            ai__fail(AI_EIO, "cannot create %s: %s", dir, strerror(errno));
        else if (!(ai__put(&o, TLD_JSTR("what", what)) || ai__put(&o, TLD_JSTR("payload", payload)) ||
                   ai__put(&o, TLD_JSTR("status", "open")) || ai__put(&o, TLD_JNULL("answer")) ||
                   ai__dump(o, &txt, 1) || tld_strbuf_append_char(txt, '\n') != OK)) {
            FILE *f = fopen(tmp, "wb");
            int bad = !f || fwrite(TLD_STR(txt), 1, (size_t)txt->len, f) != (size_t)txt->len;
            if (f && fclose(f)) bad = 1;
            if (bad || rename(tmp, path)) {
                ai__fail(AI_EIO, "cannot write %s: %s", path, strerror(errno));
                remove(tmp);
            } else {
                ai__fail(AI_OK, "review opened: %s (%s)", path, what);
                LOG_MSG("tld_ai: review opened: %s: %s", path, what);
            }
        }
        tld_json_obj_free(o);
        tld_strbuf_free(txt);
    }
    pthread_mutex_unlock(&ai__review_mu);
    tld_strbuf_free(b);
    return ans;
}

const char *ai_review(const char *what, const char *payload) {
    ai__begin();
    if (ai__enter()) return NULL;
    const char *r = ai__review_impl(what, payload);
    ai__leave();
    return r;
}

/* ---- shutdown ----------------------------------------------------------- */

static void ai__shutdown(void) {
    ai_reset();
    pthread_mutex_lock(&ai__mem_mu);
    free(ai__arena);
    ai__arena = NULL;
    ai__arena_cap = 0;
    pthread_mutex_unlock(&ai__mem_mu);
    pthread_mutex_lock(&ai__cache_mu);
    tld_hash_free(ai__cache);
    tld_hash_free(ai__seen);
    ai__cache = ai__seen = NULL;
    free(ai__cache_file);
    ai__cache_file = NULL;
    pthread_mutex_unlock(&ai__cache_mu);
    pthread_mutex_lock(&ai__dig_mu);
    tld_hash_free(ai__dig);
    ai__dig = NULL;
    pthread_mutex_unlock(&ai__dig_mu);
    pthread_mutex_lock(&ai__cfg_mu); /* later calls fail cleanly (ai__enter) */
    if (ai__conf_cur && ai__conf_cur->refs == 0) ai__conf_free(ai__conf_cur);
    ai__conf_cur = NULL; /* one still in use is freed by its last ai__leave */
    ai__layer_free(&ai__env_layer);
    pthread_mutex_unlock(&ai__cfg_mu);
}

#endif /* AI__IMPLEMENTED */
#endif /* AI_IMPLEMENTATION */

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
