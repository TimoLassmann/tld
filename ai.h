/* ai.h - local LLMs, decision models and embeddings through the Ollama server on this
 * machine, called as ordinary C functions. Single header, builds on tld.h. Thread-safe.
 *
 *   char *s = ai_text("Summarise:\n%s", doc);
 *   double p = ai_yesno("Is a refund being requested?", ticket);
 *   struct team_answer t = ai_choose(team, ticket);
 *   ai_fill(&fix, fix, "Propose a fix for:\n%s", ticket);
 *   ai_vec v = ai_embed(abstract);
 *
 * USAGE
 *   In exactly one .c file, before any other #include:
 *
 *       #define TLD_IMPLEMENTATION      // unless another file already compiles tld.h
 *       #define AI_IMPLEMENTATION
 *       #include "ai.h"                 // includes "tld.h" itself if TLD_H is not defined
 *
 *   and plain #include "ai.h" everywhere else. The program must compile
 *   TLD_IMPLEMENTATION in some file (ai.h uses tld's JSON, strings and hash map).
 *   Build: cc -std=c11 prog.c -lpthread -lm. Nothing else: no libcurl, no SQLite, no
 *   cJSON (HTTP, JSON, cache and reviews are done here and in tld.h).
 *
 *   C11 plus POSIX (sockets, poll, flock, pthreads); macOS and Linux. In the
 *   AI_IMPLEMENTATION file ai.h asks for POSIX.1-2008 itself (it defines
 *   _POSIX_C_SOURCE, _DEFAULT_SOURCE and _DARWIN_C_SOURCE when they are not set), so
 *   nothing needs -D flags. That works when ai.h comes before every system header in
 *   that file; if a system header came first, ai.h supplies the few declarations a
 *   strict -std=c11 build hides.
 *
 * ENVIRONMENT (all optional; read once, at the first call into ai.h, so a later
 * setenv() has no effect)
 *   AI_HOST      Ollama base URL                      default http://localhost:11434
 *                (plain http only; https:// fails with AI_EHTTP)
 *   AI_LLM       default LLM                          default inkling-2-mini:latest
 *   AI_DECIDER   default decision model               default autotrust-jev:latest
 *   AI_EMBEDDER  default embedding model              default embeddinggemma-2:latest
 *   AI_NUM_CTX   context length sent with every LLM and embedding call that does not
 *                set .num_ctx                         default: not sent (server decides)
 *   AI_TIMEOUT   seconds per HTTP request             default 300
 *   AI_FRESH     1 = ignore the cache                 default 0
 *   AI_DB        cache file (JSON lines)              default ai.cache, "off" = no cache
 *   AI_REVIEWS   directory for ai_review files        default ai-reviews
 *   AI_FRESH accepts 1/0, true/false, yes/no, on/off (any case); AI_NUM_CTX and
 *   AI_TIMEOUT must be positive integers. A value that does not parse is ignored with a
 *   warning.
 *
 * DEFAULTS SET IN CODE
 *   ai_defaults(AI_CONFIG(.host = "http://192.168.1.20:11434", .llm = "inkling-2-mini",
 *                         .decider = "autotrust-jev", .embedder = "embeddinggemma-2",
 *                         .opts = AI_OPTS(.temperature = AI_F(0.2))));
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
 *   AI_ENOSERVER, and the probe is retried at most every 5 s.
 *
 * MEMORY
 *   On a machine that can hold only one model, start Ollama with
 *   OLLAMA_MAX_LOADED_MODELS=1; the server then swaps models safely between requests.
 *
 * CLOUD MODELS
 *   Opt-in only. A cloud model is one whose /api/tags entry has "remote_host"; it is
 *   reached through the local server like any other (ai.h does no HTTPS). The defaults
 *   are local models, so prompts leave the machine only when a cloud model is named in
 *   AI_LLM, AI_DECIDER, AI_EMBEDDER or .model. A server that is not signed in answers
 *   with an HTTP error, which comes back as AI_EHTTP with the server's message.
 *
 * CACHE AND REPLAY
 *   Every answered request is appended to AI_DB as one JSON line
 *   {key, endpoint, model, digest, request, response, seconds, created}, under
 *   flock(LOCK_EX), so several processes can share the file. The key is FNV-1a 64 of
 *   endpoint, model digest and request body, plus "#n" for the n-th identical request
 *   in the process; a rerun therefore replays finished calls in order, and a retry is
 *   a new call. n advances only when an answer is obtained, so a transient failure
 *   does not shift the replay. The digest stops an updated model from replaying old
 *   answers. AI_FRESH=1 or .fresh = 1 skips the lookup (the answer is still stored).
 *
 * HUMAN REVIEW
 *   ai_review(what, payload) writes AI_REVIEWS/<key>.json =
 *   {"what":..., "payload":..., "status":"open", "answer":null} and returns NULL (with
 *   ai_status() == AI_OK) while the review is open. A person edits the file, sets
 *   "status" to "done" and "answer" to a string; the next run returns that answer.
 *
 * THREADS
 *   Every function may be called from any thread. Strings and vectors returned by the
 *   library live until ai_reset() or exit; never free() them. Call ai_reset() only when
 *   no thread still uses returned values. Error text, status and ai_last() are
 *   per thread.
 *
 * LICENCE: MIT, see the end of this file.
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

#define AI_VERSION "0.3.0"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- status codes (ai_status) ------------------------------------------ */

enum {
    AI_OK = 0,
    AI_ENOSERVER, /* no Ollama server answers at AI_HOST                       */
    AI_EHTTP,     /* https host, timeout, truncated reply, HTTP error status    */
    AI_EPARSE,    /* the reply is not valid JSON or lacks the expected fields   */
    AI_ESCHEMA,   /* valid JSON that does not fit the struct, labels or range   */
    AI_ENOMODEL,  /* the model is not installed on the server                   */
    AI_EIO,       /* cache, review or image file could not be read or written   */
    AI_ENOMEM,    /* out of memory                                              */
    AI_EARG       /* invalid argument (NULL format, n <= 0, ...)                */
};

/* ---- options ----------------------------------------------------------- */

typedef struct { int set; double v; } ai_f;   /* optional double: unset is not 0 */
typedef struct { int set; long   v; } ai_i;   /* optional integer */
#define AI_F(x) ((ai_f){1, (x)})
#define AI_I(x) ((ai_i){1, (x)})

typedef struct {
    const char *model;            /* overrides AI_LLM / AI_DECIDER / AI_EMBEDDER */
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

/* AI_OPTS(.temperature = AI_F(0), .seed = AI_I(7)) -> const ai_opts * */
#define AI_OPTS(...) (&(const ai_opts){__VA_ARGS__})

/* ---- program-set defaults ---------------------------------------------- */
/* Precedence, per setting: per-call options > environment > ai_defaults() > built-in.
 * The environment is read once, at the first call into ai.h. */
typedef struct {
    const char *host;      /* AI_HOST      e.g. "http://192.168.1.20:11434" (http:// only) */
    const char *llm;       /* AI_LLM       text model: ai_text, ai_fill                  */
    const char *decider;   /* AI_DECIDER   decision model: ai_yesno, ai_choose, ai_rate  */
    const char *embedder;  /* AI_EMBEDDER  embedding model: ai_embed, ai_embed_all       */
    const char *db;        /* AI_DB        cache file, or "off"                          */
    const char *reviews;   /* AI_REVIEWS   review directory                              */
    ai_i num_ctx;          /* AI_NUM_CTX   context length for LLM and embedding calls    */
    ai_i timeout;          /* AI_TIMEOUT   seconds per request                           */
    ai_i fresh;            /* AI_FRESH     1 = ignore the cache                          */
    ai_i embed_dimensions; /* default output dimensions for embeddings, if the model allows */
    const ai_opts *opts;   /* default per-call options (system, temperature, seed, top_p,
                              stop, think, keep_alive, retries, ...) applied to every call */
} ai_config;
#define AI_CONFIG(...) (&(const ai_config){__VA_ARGS__})

/* The fields c sets replace the built-in defaults; NULL resets to the built-ins.
 * 0 on success, -1 with ai_status()/ai_error(); an invalid call changes nothing. */
int ai_defaults(const ai_config *c);

/* ---- plain calls ------------------------------------------------------- */

/* Text from an LLM; NULL on failure. */
char       *ai_text(const char *fmt, ...);
char       *ai_text_with(const ai_opts *o, const char *fmt, ...);
/* Probability that the answer is yes, from a decision model; -1 on failure. */
double      ai_yesno(const char *question, const char *state);
double      ai_yesno_with(const ai_opts *o, const char *question, const char *state);
/* Ask a person: returns their answer, or NULL while the review is open. */
const char *ai_review(const char *what, const char *payload);
char       *ai_fmt(const char *fmt, ...);   /* sprintf into library memory */
const char *ai_error(void);                 /* last error message in this thread */
int         ai_status(void);                /* last status code in this thread (AI_OK..) */
void        ai_reset(void);                 /* free all returned strings and vectors */

typedef struct {
    const char *thinking;         /* model's thinking text, if requested */
    long   prompt_tokens, output_tokens;
    double seconds;               /* wall time of the HTTP call, 0 if cached */
    int    from_cache;
} ai_info;
const ai_info *ai_last(void);                /* previous call in this thread */

/* ---- embeddings -------------------------------------------------------- */

typedef struct { int dim; const float *x; } ai_vec;              /* x == NULL on failure */
typedef struct { int n, dim; const float *x; } ai_vecs;          /* row i at x + i*dim */

ai_vec  ai_embed(const char *text);
ai_vec  ai_embed_with(const ai_opts *o, const char *text);
ai_vecs ai_embed_all(const char *const *texts, int n);           /* one request, n rows */
ai_vecs ai_embed_all_with(const ai_opts *o, const char *const *texts, int n);
#define ai_row(vs, i) ((ai_vec){(vs).dim, (vs).x + (size_t)(i) * (vs).dim})

double ai_cosine(ai_vec a, ai_vec b);                            /* -2 if dims differ */
/* Indices of the k rows of m most similar to q, best first; returns how many. */
int    ai_nearest(ai_vec q, ai_vecs m, int k, int *idx, double *score);

/* ---- parallel work ----------------------------------------------------- */

/* Run fn(i, ctx) for i = 0..n-1 on `threads` threads and wait for all. */
void ai_parallel(int n, int threads, void (*fn)(int i, void *ctx), void *ctx);

/* ---- typed structs filled by an LLM ------------------------------------ */
/* #define FIX(X) X(str, title, "One-line summary") X(num, risk, "Risk 0 to 1")
 * AI_STRUCT(fix, FIX)              -> struct fix { char *title; double risk; };
 * ai_fill(&f, fix, fmt, ...)       -> 0 on success, -1 on failure (f zeroed)  */

typedef enum { AI_T_str, AI_T_num, AI_T_int, AI_T_bool } ai_kind;
typedef struct { const char *name; ai_kind kind; size_t off; const char *desc; } ai_field;
typedef struct { const char *name; size_t size; const ai_field *f; int n; } ai_type;
typedef struct { const char *name; const char *desc; } ai_label;

#define AI__CT_str  char *
#define AI__CT_num  double
#define AI__CT_int  long
#define AI__CT_bool int
#define AI__DECL(t, n, d)  AI__CT_##t n;
#define AI__FIELD(t, n, d) { #n, AI_T_##t, offsetof(S, n), d },

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
/* #define TEAM(X) X(bug, "Software defects") X(billing, "Payments and refunds")
 * AI_CHOICE(team, "Which team should handle this?", TEAM)
 *   -> enum team { bug, billing };  struct team_answer { ok, value, confidence, p[] }
 * Labels become bare enum constants, so two choices cannot share a label
 * (the compiler reports the clash).                                          */

#define AI__ENUM(n, d)  n,
#define AI__LABEL(n, d) { #n, d },

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
/* #define URGENCY(X) X(routine, "Can wait a week") X(soon, "...") X(urgent, "...")
 * AI_SCALE(urgency, "How urgent is this?", URGENCY)
 *   -> struct urgency_answer { ok, level, score, confidence }   (lowest first) */

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

#define ai_fill(out, T, ...) \
    ai__fill(AI__CHECK(out, T), T##__type(), NULL, __VA_ARGS__)
#define ai_fill_with(o, out, T, ...) \
    ai__fill(AI__CHECK(out, T), T##__type(), (o), __VA_ARGS__)
#define ai_choose(T, state)             T##__choose((state), NULL)
#define ai_choose_with(o, T, state)     T##__choose((state), (o))
#define ai_rate(T, state)               T##__rate((state), NULL)
#define ai_rate_with(o, T, state)       T##__rate((state), (o))

/* internals used by the macros */
int ai__fill(void *out, const ai_type *t, const ai_opts *o, const char *fmt, ...);
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
#error "ai.h: the implementation is C only; define AI_IMPLEMENTATION in a .c file"
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
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* Declarations that a strict ISO C build hides when a system header was included
 * before ai.h (so the feature macros at the top came too late). */
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

#define AI__MIN(a, b) ((a) < (b) ? (a) : (b))
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

static char *ai__strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
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
    pthread_mutex_unlock(&ai__mem_mu);
    return p;
}

static char *ai__kdup(const char *s) {
    if (!s) return NULL;
    char *d = ai__strdup(s);
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

char *ai_fmt(const char *fmt, ...) {
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
/* JSON_OBJ/json_obj_add_multiple swallow allocation failures once the object
 * exists, so ai.h adds one entry at a time and checks that the count grew. */

static void ai__drop(tld_json_decoration d) {
    if (!d.value) return;
    if (d.type == VAL_OBJ) tld_json_obj_free((tld_json_obj *)d.value);
    if (d.type == VAL_ARR) tld_json_arr_free((tld_json_arr *)d.value);
}

/* Add d to *o (created if NULL). A NULL value is skipped, as in tld. Ownership of an
 * object or array value passes to *o, also on failure, when it is freed: by tld when
 * *o was NULL (the result is NULL), here when *o exists (returned unchanged, without
 * the child). */
static int ai__put(tld_json_obj **o, tld_json_decoration d) {
    if (!d.value) return 0;
    int before = *o ? (*o)->n : 0;
    tld_json_obj *r = json_obj_add_multiple(*o, &d, 1);
    if (!r || r->n != before + 1) {
        if (r) ai__drop(d);
        return ai__nomem();
    }
    *o = r;
    return 0;
}

static int ai__push(tld_json_arr **a, tld_json_decoration d) {
    if (!d.value) return 0;
    int before = *a ? (*a)->n : 0;
    tld_json_arr *r = json_arr_add_multiple(*a, &d, 1);
    if (!r || r->n != before + 1) {
        if (r) ai__drop(d);
        return ai__nomem();
    }
    *a = r;
    return 0;
}

/* Detach a borrowed object or array stored under key, so freeing o leaves it alone. */
static void ai__detach(tld_json_obj *o, const char *key) {
    for (int i = 0; o && i < o->n; i++)
        if (!strcmp(TLD_STR(o->key[i]), key)) o->v[i]->type = TLD_JSON_UNDEF;
}

/* tld_json_dump writes indented text; drop the whitespace outside strings. */
static void ai__compact(tld_strbuf *b) {
    char *s = (char *)b->str;
    int w = 0, instr = 0, esc = 0;
    for (int r = 0; r < b->len; r++) {
        char c = s[r];
        if (instr) {
            s[w++] = c;
            if (esc)
                esc = 0;
            else if (c == '\\')
                esc = 1;
            else if (c == '"')
                instr = 0;
        } else if (c != ' ' && c != '\n' && c != '\t' && c != '\r') {
            s[w++] = c;
            if (c == '"') instr = 1;
        }
    }
    b->len = w;
    s[w] = 0;
}

/* Compact JSON text of o in a new strbuf. indent >= 0 keeps tld's layout. */
static int ai__dump(tld_json_obj *o, tld_strbuf **out, int pretty) {
    tld_strbuf *b = NULL;
    if (tld_strbuf_alloc(&b, 1024) != OK) return ai__nomem();
    if (o) {
        if (tld_json_dump(o, b, 0) != OK) {
            tld_strbuf_free(b);
            return ai__nomem();
        }
    } else if (tld_append(b, "{}") != OK) {
        tld_strbuf_free(b);
        return ai__nomem();
    }
    if (!pretty) ai__compact(b);
    *out = b;
    return 0;
}

/* Parse NUL-terminated text that must hold a JSON object. */
static int ai__parse_buf(tld_strbuf *b, tld_json_obj **out) {
    const char *p = TLD_STR(b);
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    if (*p != '{') return -1;
    *out = NULL;
    return tld_json_parse(b, out) == OK && *out ? 0 : -1;
}

static int ai__parse(const char *s, tld_json_obj **out) {
    tld_strbuf *b = NULL;
    size_t n = strlen(s);
    if (n > (size_t)AI__MAX_BODY) return -1;
    if (tld_strbuf_alloc(&b, (int)n + 1) != OK) return ai__nomem();
    if (tld_append(b, (char *)s) != OK) {
        tld_strbuf_free(b);
        return ai__nomem();
    }
    int rc = ai__parse_buf(b, out);
    tld_strbuf_free(b);
    return rc;
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

#ifndef AI_LOG /* level: 0 info, 1 warning, 2 error */
#define AI_LOG(level, msg)                                                             \
    fprintf(stderr, "ai: %s%s\n", (level) >= 2 ? "error: " : (level) == 1 ? "warning: " : "", \
            (msg))
#endif

enum { AI__S_HOST, AI__S_LLM, AI__S_DECIDER, AI__S_EMBEDDER, AI__S_DB, AI__S_REVIEWS,
       AI__S_NUM_CTX, AI__S_TIMEOUT, AI__S_FRESH, AI__S_DIMS };
#define AI__NSTR 6 /* string settings come first */
#define AI__NNUM 4 /* then the numbers */
static const char *const ai__s_name[AI__NSTR + AI__NNUM] = {
    "host", "llm", "decider", "embedder", "db", "reviews", "num_ctx", "timeout",
    "fresh", "embed_dimensions"};
static const char *const ai__s_builtin[AI__NSTR] = {
    "http://localhost:11434", "inkling-2-mini:latest", "autotrust-jev:latest",
    "embeddinggemma-2:latest", "ai.cache", "ai-reviews"};

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
    /* AI_HOST taken apart */
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

static void ai__setstr(char *dst, size_t n, const char *s) { snprintf(dst, n, "%s", s); }

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
        if (!(d[i] = ai__strdup(v[i]))) {
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
    if ((s->system && !(o->system = ai__strdup(s->system))) ||
        (s->think && !(o->think = ai__strdup(s->think))) ||
        (s->keep_alive && !(o->keep_alive = ai__strdup(s->keep_alive))) ||
        (s->extra_options && !(o->extra_options = ai__strdup(s->extra_options))) ||
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
    char msg[600];
    snprintf(msg, sizeof msg, "ignoring %s=%.400s: expected %s", name, v, want);
    AI_LOG(1, msg);
}

static int ai__icase(const char *a, const char *b) { /* equal, ignoring ASCII case */
    for (; *a && tolower((unsigned char)*a) == tolower((unsigned char)*b); a++, b++) {}
    return tolower((unsigned char)*a) == tolower((unsigned char)*b);
}

/* The environment, read once. Values that do not parse are ignored with a warning. */
static int ai__env_read(ai__layer *l) {
    static const char *const names[AI__NSTR + 3] = {
        "AI_HOST", "AI_LLM", "AI_DECIDER", "AI_EMBEDDER", "AI_DB", "AI_REVIEWS",
        "AI_NUM_CTX", "AI_TIMEOUT", "AI_FRESH"};
    static const char *const yes[] = {"1", "true", "yes", "on"}, *const no[] = {"0", "false",
                                                                              "no", "off"};
    memset(l, 0, sizeof *l);
    for (int i = 0; i < AI__NSTR + 3; i++) {
        const char *v = getenv(names[i]);
        if (!v || !*v) continue;
        if (i < AI__NSTR) {
            if (!(l->str[i] = ai__strdup(v))) return -1;
            continue;
        }
        ai_f *n = &l->num[i - AI__NSTR];
        if (i == AI__S_FRESH) {
            for (int k = 0; k < 4; k++) {
                if (ai__icase(v, yes[k])) *n = AI_F(1);
                if (ai__icase(v, no[k])) *n = AI_F(0);
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
        memcpy(c->name, a + 1, AI__MIN(hl, sizeof c->name - 1));
        if (e[1] == ':') port = e + 2;
    } else {
        const char *col = strrchr(a, ':');
        hl = col ? (size_t)(col - a) : strlen(a);
        memcpy(c->name, a, AI__MIN(hl, sizeof c->name - 1));
        if (col) port = col + 1;
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
        if (!(c->str[i] = ai__strdup(v))) {
            ai__conf_free(c);
            return NULL;
        }
    }
    double num[AI__NNUM];
    for (int i = 0; i < AI__NNUM; i++) {
        const ai_f *e = &env->num[i], *d = &def->num[i];
        num[i] = e->set ? e->v : d->set ? d->v : builtin_num[i];
    }
    /* AI_NUM_CTX > config.num_ctx > config.opts->num_ctx */
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
    return ai__fail(AI_EARG, "ai.h was already shut down (called after exit cleanup)");
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
        return ai__fail(AI_EHTTP, "ai_defaults: host %s must start with http:// (ai.h "
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
        if (s[i] && !(l->str[i] = ai__strdup(s[i]))) goto nomem;
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
        if (o) tld_json_obj_free(o);
        (*skipped)++;
        return 0;
    }
    tld_strbuf *txt = NULL;
    int rc = ai__dump(resp, &txt, 0);
    if (!rc && tld_hash_add(&ai__cache, (char *)k, TLD_STR(txt)) != OK) rc = ai__nomem();
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
            if (tld_append(line, s) != OK) {
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
    if (!rc && skipped) {
        char msg[1200];
        snprintf(msg, sizeof msg, "skipped %d damaged line(s) in cache %s", skipped, path);
        AI_LOG(1, msg);
    }
    return rc;
}

/* Load the cache on first use. From then on its path is fixed. */
static int ai__cache_open(void) {
    int rc = 0;
    pthread_mutex_lock(&ai__cache_mu);
    if (!ai__cache_file) {
        char *path = ai__strdup(ai__c->str[AI__S_DB]);
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
                    "AI_HOST)",
                    AI__HOST(ai__c), why);
}

static int ai__host_ok(void) {
    if (ai__c->https)
        return ai__fail(AI_EHTTP,
                        "AI_HOST=%s: https is not supported; ai.h talks plain http to the "
                        "local Ollama server (cloud models are reached through it)",
                        AI__HOST(ai__c));
    if (ai__c->bad_host)
        return ai__fail(AI_EHTTP, "AI_HOST=%s: expected http://host:port", AI__HOST(ai__c));
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
        snprintf(why, whyn, "cannot resolve %s: %s", ai__c->name, gai_strerror(g));
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

static int ai__ieq(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return 0;
    return 1;
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
        if (n - i < sz + 2) return 0;
        char save = s[i + sz];
        ((char *)s)[i + sz] = 0;
        int rc = tld_append(out, (char *)s + i);
        ((char *)s)[i + sz] = save;
        if (rc != OK) return -2;
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
    if (o) tld_json_obj_free(o);
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
    int fd = ai__connect(AI__MIN(deadline, t0 + AI__CONNECT_MAX), why, sizeof why);
    if (fd < 0) return ai__noserver(why);

    tld_strbuf *req = NULL, *raw = NULL, *res = NULL;
    int rc = -1;
    char line[1024];
    size_t blen = body ? strlen(body) : 0;
    snprintf(line, sizeof line,
             "%s %s%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: ai.h/" AI_VERSION
             "\r\nAccept: application/json\r\nConnection: close\r\n",
             method, ai__c->base, path, ai__c->authority);
    if (tld_strbuf_alloc(&req, (int)(blen + 512)) != OK || tld_append(req, line) != OK)
        goto nomem;
    if (body) {
        snprintf(line, sizeof line,
                 "Content-Type: application/json\r\nContent-Length: %zu\r\n", blen);
        if (tld_append(req, line) != OK) goto nomem;
    }
    if (tld_append(req, "\r\n") != OK || (body && tld_append(req, (char *)body) != OK))
        goto nomem;
    if (ai__send_all(fd, TLD_STR(req), (size_t)req->len, deadline)) {
        if (errno == ETIMEDOUT)
            ai__fail(AI_EHTTP, "%s %s: timed out after %g s (AI_TIMEOUT)", method, path,
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
            ai__fail(AI_EHTTP, "%s %s: timed out after %g s (AI_TIMEOUT)", method, path,
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
        if (tld_append(raw, buf) != OK) goto nomem;
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
                if (hl > 15 && ai__ieq(h, "content-length:", 15))
                    clen = strtol(h + 15, NULL, 10);
                else if (hl > 18 && ai__ieq(h, "transfer-encoding:", 18)) {
                    for (const char *q = h + 18; q + 7 <= eol; q++)
                        if (ai__ieq(q, "chunked", 7)) chunked = 1;
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
            if (tld_append(res, TLD_STR(raw) + hdr) != OK) goto nomem;
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
    if (ai__http("GET", path, NULL, AI__MIN(ai__c->timeout, 30.0), &b)) return -1;
    int rc = ai__parse_buf(b, out);
    tld_strbuf_free(b);
    if (rc) return ai__fail(AI_EPARSE, "GET %s: reply is not a JSON object", path);
    return 0;
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
                        "AI_HOST)",
                        AI__HOST(ai__c), why);
    tld_strbuf *b = NULL;
    if (ai__http("GET", "/api/version", NULL, AI__MIN(ai__c->timeout, AI__CONNECT_MAX),
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

/* "x" and "x:latest" are one model. */
static void ai__norm(const char *m, char *out, size_t n) {
    const char *slash = strrchr(m, '/');
    snprintf(out, n, strchr(slash ? slash : m, ':') ? "%s" : "%s:latest", m);
}

/* Digests are kept per host: key = host, 0x1f, model. */
static void ai__hkey(const char *norm, char *out, size_t n) {
    snprintf(out, n, "%s\x1f%s", AI__HOST(ai__c), norm);
}

static int ai__dig_lookup(const char *norm, char *digest, size_t dn) {
    char *v = NULL, hk[900];
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
    ai__norm(model, norm, sizeof norm);
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
        ai__norm(name, nm, sizeof nm);
        ai__hkey(nm, key, sizeof key);
        if (tld_hash_add(&ai__dig, key, (char *)(dg ? dg : "")) != OK) rc = ai__nomem();
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
    long long t = (long long)time(NULL);
    long long days = t / 86400, sec = t % 86400;
    /* civil from days (Howard Hinnant) */
    long long z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097;
    long long doe = z - era * 146097;
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long y = yoe + era * 400, doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long long mp = (5 * doy + 2) / 153, d = doy - (153 * mp + 2) / 5 + 1;
    long long m = mp < 10 ? mp + 3 : mp - 9;
    snprintf(out, 64, "%04lld-%02lld-%02lldT%02lld:%02lld:%02lldZ", y + (m <= 2), m, d,
             sec / 3600, sec / 60 % 60, sec % 60);
}

/* Reserve the next occurrence of base. Caller holds ai__cache_mu. */
static int ai__seen_next(const char *base) {
    char *v = NULL, s[24];
    int n = 0;
    if (ai__seen && tld_hash_get(ai__seen, (char *)base, &v) == OK && v) n = atoi(v);
    snprintf(s, sizeof s, "%d", n + 1);
    if (tld_hash_add(&ai__seen, (char *)base, s) != OK) return -1;
    return n + 1;
}

/* A call that got no answer gives its occurrence back, so the retry gets the same
 * key and a rerun replays in step. If another thread has reserved a later one
 * meanwhile, the number is left as it is: identical requests are interchangeable,
 * so the gap only costs one extra request on a rerun. */
static void ai__seen_release(const char *base, int occ) {
    char *v = NULL, s[24];
    pthread_mutex_lock(&ai__cache_mu);
    if (ai__seen && tld_hash_get(ai__seen, (char *)base, &v) == OK && v && atoi(v) == occ) {
        snprintf(s, sizeof s, "%d", occ - 1);
        tld_hash_add(&ai__seen, (char *)base, s); /* replaces in place, cannot grow */
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
    if (tld_append_char(line, '\n') != OK) goto unlock;
    rc = ai__write_all(fd, TLD_STR(line), (size_t)line->len);
unlock:
    flock(fd, LOCK_UN);
out:
    close(fd);
    return rc;
}

static void ai__cache_store(const char *key, const char *ep, const char *model,
                            const char *digest, tld_json_obj *req, tld_json_obj *resp,
                            double seconds) {
    char created[64];
    ai__iso_now(created);
    tld_json_obj *o = NULL;
    tld_strbuf *line = NULL;
    int err = ai__t_st, rc;
    char msg[512];
    ai__setstr(msg, sizeof msg, ai__t_err);
    rc = ai__put(&o, JSTR("key", key)) || ai__put(&o, JSTR("endpoint", ep)) ||
         ai__put(&o, JSTR("model", model)) || ai__put(&o, JSTR("digest", digest)) ||
         ai__put(&o, JO("request", req)) || ai__put(&o, JO("response", resp)) ||
         ai__put(&o, JNUM("seconds", seconds)) || ai__put(&o, JSTR("created", created)) ||
         ai__dump(o, &line, 0);
    ai__detach(o, "request");
    ai__detach(o, "response");
    if (o) tld_json_obj_free(o);
    char path[1024] = "";
    if (!rc) {
        pthread_mutex_lock(&ai__cache_mu);
        if (ai__cache_file) {
            ai__setstr(path, sizeof path, ai__cache_file);
            rc = ai__append_line(ai__cache_file, line);
        }
        pthread_mutex_unlock(&ai__cache_mu);
    }
    tld_strbuf_free(line);
    if (rc) {
        char m[1200];
        snprintf(m, sizeof m, "cannot append to cache %s: %s", path, strerror(errno));
        AI_LOG(1, m);
    }
    ai__t_st = err; /* the call itself succeeded */
    ai__setstr(ai__t_err, sizeof ai__t_err, msg);
}

/* Send req to endpoint for model, or replay it from the cache. Returns the parsed
 * reply (caller frees) or NULL with the status set. req stays the caller's. */

static tld_json_obj *ai__call(const char *ep, const char *model, tld_json_obj *req,
                              int fresh) {
    memset(&ai__t_last, 0, sizeof ai__t_last);
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
        char *v = NULL;
        if (!fresh && !ai__c->fresh && ai__c->cache_on && ai__cache &&
            tld_hash_get(ai__cache, key, &v) == OK && v && !(hit = ai__strdup(v)))
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
        return resp;
    }

    double t0 = ai__now();
    tld_strbuf *rb = NULL;
    int rc = ai__http("POST", ep, TLD_STR(body), ai__c->timeout, &rb);
    ai__t_last.seconds = ai__now() - t0;
    if (!rc && ai__parse_buf(rb, &resp))
        rc = ai__fail(AI_EPARSE, "POST %s: reply is not a JSON object", ep);
    tld_strbuf_free(rb);
    tld_strbuf_free(body);
    if (rc) {
        ai__seen_release(base, occ);
        return NULL;
    }
    if (ai__c->cache_on) ai__cache_store(key, ep, model, dg, req, resp,
                                          ai__t_last.seconds);
    return resp;
}

/* ---- request building --------------------------------------------------- */

static int ai__b64_file(const char *path, char **out) {
    *out = NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return ai__fail(AI_EIO, "cannot read image %s: %s", path, strerror(errno));
    long n = -1;
    if (fseek(f, 0, SEEK_END) == 0) n = ftell(f);
    if (n < 0 || fseek(f, 0, SEEK_SET) != 0 || n > (long)AI__MAX_BODY / 2) {
        fclose(f);
        return ai__fail(AI_EIO, "cannot read image %s", path);
    }
    unsigned char *d = malloc((size_t)n + 1);
    char *o = malloc(4 * (((size_t)n + 2) / 3) + 1);
    if (!d || !o) {
        free(d);
        free(o);
        fclose(f);
        return ai__nomem();
    }
    size_t got = fread(d, 1, (size_t)n, f);
    int bad = got != (size_t)n || ferror(f);
    fclose(f);
    if (bad) {
        free(d);
        free(o);
        return ai__fail(AI_EIO, "short read from image %s", path);
    }
    static const char t[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char *q = o;
    for (size_t i = 0; i < got; i += 3) {
        uint32_t v = (uint32_t)d[i] << 16 | (i + 1 < got ? (uint32_t)d[i + 1] << 8 : 0) |
                     (i + 2 < got ? d[i + 2] : 0);
        *q++ = t[v >> 18 & 63];
        *q++ = t[v >> 12 & 63];
        *q++ = i + 1 < got ? t[v >> 6 & 63] : '=';
        *q++ = i + 2 < got ? t[v & 63] : '=';
    }
    *q = 0;
    free(d);
    *out = o;
    return 0;
}

static int ai__add_images(tld_json_obj **obj, const ai_opts *o) {
    if (!o || !o->images) return 0;
    tld_json_arr *a = NULL;
    for (const char *const *p = o->images; *p; p++) {
        char *e = NULL;
        if (ai__b64_file(*p, &e) || ai__push(&a, JSTR(NULL, e))) {
            free(e);
            if (a) tld_json_arr_free(a);
            return -1;
        }
        free(e);
    }
    return ai__put(obj, JA("images", a));
}

/* Model options, plus AI_NUM_CTX when the caller did not set num_ctx. */
static int ai__add_options(tld_json_obj **req, const ai_opts *o) {
    tld_json_obj *op = NULL;
    int rc = 0;
    if (o && o->temperature.set) rc = rc || ai__put(&op, JNUM("temperature", o->temperature.v));
    if (o && o->top_p.set) rc = rc || ai__put(&op, JNUM("top_p", o->top_p.v));
    if (o && o->min_p.set) rc = rc || ai__put(&op, JNUM("min_p", o->min_p.v));
    if (o && o->top_k.set) rc = rc || ai__put(&op, JINT("top_k", o->top_k.v));
    if (o && o->seed.set) rc = rc || ai__put(&op, JINT("seed", o->seed.v));
    if (o && o->num_ctx.set)
        rc = rc || ai__put(&op, JINT("num_ctx", o->num_ctx.v));
    else if (ai__c->num_ctx > 0)
        rc = rc || ai__put(&op, JINT("num_ctx", ai__c->num_ctx));
    if (o && o->num_predict.set) rc = rc || ai__put(&op, JINT("num_predict", o->num_predict.v));
    if (!rc && o && o->stop) {
        tld_json_arr *a = NULL;
        for (const char *const *p = o->stop; *p && !rc; p++) rc = ai__push(&a, JSTR(NULL, *p));
        if (rc && a) tld_json_arr_free(a);
        if (!rc) rc = ai__put(&op, JA("stop", a));
    }
    if (!rc && o && o->extra_options) {
        tld_json_obj *x = NULL;
        if (ai__parse(o->extra_options, &x)) {
            if (ai__t_st == AI_OK)
                rc = ai__fail(AI_EARG, "extra_options is not a JSON object: %.200s",
                              o->extra_options);
            else
                rc = -1;
        }
        if (!rc && !op && tld_json_obj_alloc(&op) != OK) rc = ai__nomem();
        for (int i = 0; !rc && x && i < x->n; i++) {
            int j = 0; /* a key already present (built-in or earlier) is replaced */
            while (j < op->n && strcmp(TLD_STR(op->key[j]), TLD_STR(x->key[i]))) j++;
            if (j < op->n) {
                if (tld_json_val_copy(op->v[j], x->v[i]) != OK) rc = ai__nomem();
            } else if (tld_append(op->key[op->n], TLD_STR(x->key[i])) != OK ||
                       tld_json_val_copy(op->v[op->n], x->v[i]) != OK)
                rc = ai__nomem();
            else {
                op->n++;
                if (tld_json_obj_chk_space(op) != OK) rc = ai__nomem();
            }
        }
        if (x) tld_json_obj_free(x);
    }
    if (rc) {
        if (op) tld_json_obj_free(op);
        return -1;
    }
    if (op && op->n > 0) {
        if (ai__put(req, JO("options", op))) return -1;
    } else if (op) {
        tld_json_obj_free(op);
    }
    if (o && o->think) {
        if (!strcmp(o->think, "true") || !strcmp(o->think, "false"))
            rc = ai__put(req, JBOOL("think", !strcmp(o->think, "true")));
        else
            rc = ai__put(req, JSTR("think", o->think));
    }
    if (!rc && o && o->keep_alive) rc = ai__put(req, JSTR("keep_alive", o->keep_alive));
    return rc;
}

/* One chat turn. format (owned, may be NULL) is a JSON schema. Returns malloc'd
 * content or NULL. */
static char *ai__chat(const ai_opts *call, const char *prompt, tld_json_obj *format) {
    ai_opts merged; /* the call's options over the defaults' options */
    ai__merge(call, ai__c->opts, &merged);
    const ai_opts *o = &merged;
    const char *model = o && o->model ? o->model : ai__c->str[AI__S_LLM];
    tld_json_obj *req = NULL, *sys = NULL, *user = NULL;
    tld_json_arr *msgs = NULL;
    char *out = NULL;
    int rc = ai__put(&req, JSTR("model", model));
    if (!rc && o && o->system) {
        rc = ai__put(&sys, JSTR("role", "system")) || ai__put(&sys, JSTR("content", o->system));
        if (rc && sys) tld_json_obj_free(sys);
        if (!rc) rc = ai__push(&msgs, JO(NULL, sys));
    }
    if (!rc) {
        rc = ai__put(&user, JSTR("role", "user")) || ai__put(&user, JSTR("content", prompt)) ||
             ai__add_images(&user, o);
        if (rc && user) tld_json_obj_free(user);
        if (!rc) rc = ai__push(&msgs, JO(NULL, user));
    }
    if (rc && msgs) tld_json_arr_free(msgs);
    if (!rc) rc = ai__put(&req, JA("messages", msgs));
    if (!rc) rc = ai__put(&req, JBOOL("stream", 0));
    if (!rc && format) {
        rc = ai__put(&req, JO("format", format));
        format = NULL;
    }
    if (!rc) rc = ai__add_options(&req, o);
    if (format) tld_json_obj_free(format);
    if (!rc) {
        tld_json_obj *r = ai__call("/api/chat", model, req, o && o->fresh);
        if (r) {
            const char *c = ai__str(r, "message.content");
            const char *th = ai__str(r, "message.thinking");
            ai__t_last.prompt_tokens = ai__long(r, "prompt_eval_count");
            ai__t_last.output_tokens = ai__long(r, "eval_count");
            if (th && *th) ai__t_last.thinking = ai__kdup(th);
            if (!c)
                ai__fail(AI_EPARSE, "/api/chat: no message content in the reply");
            else if (!(out = ai__strdup(c)))
                ai__nomem();
            tld_json_obj_free(r);
        }
    }
    if (req) tld_json_obj_free(req);
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

char *ai_text_with(const ai_opts *o, const char *fmt, ...) {
    ai__begin();
    va_list ap;
    va_start(ap, fmt);
    char *r = ai__text(o, fmt, ap);
    va_end(ap);
    return r;
}

char *ai_text(const char *fmt, ...) {
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
    int rc = ai__put(&s, JSTR("type", "object"));
    if (!rc && tld_json_obj_alloc(&props) != OK) rc = ai__nomem();
    for (int i = 0; !rc && i < t->n; i++) {
        tld_json_obj *p = NULL;
        rc = ai__put(&p, JSTR("type", js[t->f[i].kind])) ||
             ai__put(&p, JSTR("description", t->f[i].desc ? t->f[i].desc : ""));
        if (rc && p) tld_json_obj_free(p);
        if (!rc) rc = ai__put(&props, JO(t->f[i].name, p));
        if (!rc) rc = ai__push(&req, JSTR(NULL, t->f[i].name));
    }
    if (!rc) {
        rc = ai__put(&s, JO("properties", props));
        props = NULL;
    }
    if (!rc && req) {
        rc = ai__put(&s, JA("required", req));
        req = NULL;
    }
    if (props) tld_json_obj_free(props);
    if (req) tld_json_arr_free(req);
    if (rc) {
        if (s) tld_json_obj_free(s);
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
            else if (!(*(char **)dst = ai__strdup(s)))
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
            char **p = (char **)((char *)s + t->f[i].off);
            free(*p);
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
    int tries = 1 + (o && o->max_retries.set ? (o->max_retries.v > 0 ? (int)o->max_retries.v : 0)
                                            : (o && o->retries > 0 ? o->retries : 2));
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
                char **p = (char **)((char *)tmp + t->f[i].off);
                if (!ai__keep(*p)) rc = -1;
                if (rc) *p = NULL;
            }
        if (!rc) memcpy(out, tmp, t->size);
    }
out:
    free(task);
    free(prompt);
    free(tmp);
    tld_strbuf_free(sch_txt);
    if (sch) tld_json_obj_free(sch);
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
    const char *model = o && o->model ? o->model : ai__c->str[AI__S_DECIDER];
    tld_json_obj *req = NULL, *q = NULL, *qs = NULL, *st = NULL, *r = NULL;
    int rc = ai__put(&req, JSTR("model", model));
    const char *p = state;
    while (isspace((unsigned char)*p)) p++;
    if (!rc && *p == '{' && ai__parse(state, &st) == 0) /* JSON state is sent as JSON */
        rc = ai__put(&req, JO("state", st));
    else if (!rc) {
        if (ai__t_st == AI_ENOMEM) rc = -1;
        else rc = ai__put(&req, JSTR("state", state));
    }
    if (!rc) {
        rc = ai__put(&q, JSTR("type", type)) || ai__put(&q, JSTR("instructions", question));
        criteria.key = (char *)"criteria";
        if (!rc) rc = ai__put(&q, criteria);
        else ai__drop(criteria);
        criteria.value = NULL;
        if (rc && q) tld_json_obj_free(q);
        if (!rc) rc = ai__put(&qs, JO(name, q));
        if (!rc) rc = ai__put(&req, JO("questions", qs));
    }
    ai__drop(criteria);
    if (!rc) rc = ai__add_images(&req, o);
    if (!rc) {
        r = ai__call("/v1/systemone", model, req, o && o->fresh);
        if (r) {
            char path[300];
            snprintf(path, sizeof path, "answers.%s", name);
            ai__t_last.prompt_tokens = ai__long(r, "usage.input_tokens");
            ai__t_last.output_tokens = ai__long(r, "usage.output_tokens");
            if (tld_json_get_obj(r, path, ans) != OK) {
                ai__fail(AI_EPARSE, "/v1/systemone: no answer for %s", name);
                tld_json_obj_free(r);
                r = NULL;
            }
        }
    }
    if (req) tld_json_obj_free(req);
    return r;
}

static int ai__choose_impl(const char *name, const char *question, const ai_label *L,
                           int n, const char *state, const ai_opts *o, double *confidence,
                           double *p) {
    tld_json_obj *crit = NULL, *a = NULL;
    if (!L || n <= 0) return ai__fail(AI_EARG, "%s: no labels", name ? name : "choice");
    for (int i = 0; i < n; i++)
        if (ai__put(&crit, JSTR(L[i].name, L[i].desc))) {
            if (crit) tld_json_obj_free(crit);
            return -1;
        }
    tld_json_obj *r = ai__decide(o, name, "choice", question, JO(NULL, crit), state, &a);
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
        if (ai__push(&crit, JSTR(NULL, L[i].desc))) {
            if (crit) tld_json_arr_free(crit);
            return -1;
        }
    tld_json_obj *r = ai__decide(o, name, "score", question, JA(NULL, crit), state, &a);
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
    tld_json_decoration none = JNULL(NULL);
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
    const char *model = o && o->model ? o->model : ai__c->str[AI__S_EMBEDDER];
    tld_json_obj *req = NULL;
    tld_json_arr *in = NULL;
    int rc = ai__put(&req, JSTR("model", model));
    for (int i = 0; !rc && i < n; i++) {
        if (!texts[i]) rc = ai__fail(AI_EARG, "ai_embed_all: text %d is NULL", i);
        else rc = ai__push(&in, JSTR(NULL, texts[i]));
    }
    if (rc && in) tld_json_arr_free(in);
    if (!rc) rc = ai__put(&req, JA("input", in));
    if (!rc && o && o->no_truncate) rc = ai__put(&req, JBOOL("truncate", 0));
    if (!rc && o && o->dimensions.set) rc = ai__put(&req, JINT("dimensions", o->dimensions.v));
    if (!rc) rc = ai__add_options(&req, o);
    tld_json_obj *r = rc ? NULL : ai__call("/api/embed", model, req, o && o->fresh);
    if (req) tld_json_obj_free(req);
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

static int ai__read_file(const char *path, tld_strbuf **out) {
    *out = NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return errno == ENOENT ? 1 : ai__fail(AI_EIO, "cannot read %s: %s", path,
                                                   strerror(errno));
    tld_strbuf *b = NULL;
    char buf[4096];
    size_t got;
    if (tld_strbuf_alloc(&b, 4096) != OK) {
        fclose(f);
        return ai__nomem();
    }
    while ((got = fread(buf, 1, sizeof buf - 1, f)) > 0) {
        buf[got] = 0;
        if (tld_append(b, buf) != OK) {
            fclose(f);
            tld_strbuf_free(b);
            return ai__nomem();
        }
    }
    int bad = ferror(f);
    fclose(f);
    if (bad) {
        tld_strbuf_free(b);
        return ai__fail(AI_EIO, "cannot read %s", path);
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
    snprintf(path, sizeof path, "%s/%016llx.json", dir,
             (unsigned long long)ai__fnv(ai__fnv(AI__FNV0, what), payload));
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
                fprintf(stderr, "ai: review still open: %s: %s\n", path, what);
            }
        }
        if (o) tld_json_obj_free(o);
    } else if (rc == 1) { /* new review */
        tld_json_obj *o = NULL;
        tld_strbuf *txt = NULL;
        char tmp[1300];
        snprintf(tmp, sizeof tmp, "%s.tmp%ld", path, (long)getpid());
        if (mkdir(dir, 0755) < 0 && errno != EEXIST)
            ai__fail(AI_EIO, "cannot create %s: %s", dir, strerror(errno));
        else if (!(ai__put(&o, JSTR("what", what)) || ai__put(&o, JSTR("payload", payload)) ||
                   ai__put(&o, JSTR("status", "open")) || ai__put(&o, JNULL("answer")) ||
                   ai__dump(o, &txt, 1) || tld_append_char(txt, '\n') != OK)) {
            FILE *f = fopen(tmp, "wb");
            int bad = !f || fwrite(TLD_STR(txt), 1, (size_t)txt->len, f) != (size_t)txt->len;
            if (f && fclose(f)) bad = 1;
            if (bad || rename(tmp, path)) {
                ai__fail(AI_EIO, "cannot write %s: %s", path, strerror(errno));
                remove(tmp);
            } else {
                ai__fail(AI_OK, "review opened: %s (%s)", path, what);
                fprintf(stderr, "ai: review opened: %s: %s\n", path, what);
            }
        }
        if (o) tld_json_obj_free(o);
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
    pthread_mutex_unlock(&ai__cache_mu);
    pthread_mutex_lock(&ai__dig_mu);
    tld_hash_free(ai__dig);
    ai__dig = NULL;
    pthread_mutex_unlock(&ai__dig_mu);
    pthread_mutex_lock(&ai__cache_mu);
    free(ai__cache_file);
    ai__cache_file = NULL;
    pthread_mutex_unlock(&ai__cache_mu);
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
