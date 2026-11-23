/* tlddoc - write the tld manuals, docs/tld.md and docs/tld_ai.md, from the comments in
 * tld.h and tld_ai.h, so the manuals follow the headers.
 *
 *   tlddoc [--check] [--out DIR] ROOT
 *   tlddoc --examples DIR ROOT
 *
 * ROOT is the repository root (it holds tld.h and tld_ai.h); DIR defaults to ROOT/docs.
 * Without --check the two files are (re)written. With --check nothing is written: the
 * exit status is 1, with the problems listed on stderr, when a manual on disk differs
 * from what would be generated or when a public symbol has no doc comment; 0 otherwise.
 * The output holds no dates or paths, so it is byte-for-byte reproducible.
 *
 * --examples writes no manual. Each block the manual shows as C (a code example in a
 * doc comment) becomes its own program, DIR/tld-NN.c or DIR/tld_ai-NN.c: #define lines,
 * top-level macro invocations and whole function definitions at file scope, the other
 * lines in a function that has the ERROR label RUN and ASSERT jump to, and a main.
 * Blocks with an #include (the USAGE lines) are skipped. tests/docs/examples.sh
 * compiles and runs them (the doc-examples and doc-examples-ai-mock tests).
 *
 * Build: cc -std=c11 tools/tlddoc.c -o tlddoc -lm -lpthread (or the CMake target).
 *
 * COMMENT CONVENTION
 *   A doc comment is a comment block that ends on the line directly above a public
 *   declaration (a function prototype, a #define, a typedef, struct or enum, or an
 *   extern variable), with no blank line in between. It documents that declaration and
 *   every declaration that follows it without a blank line or another comment in
 *   between, so a family (TLD_MIN and TLD_MAX, the tld_json_get_* getters) shares
 *   one comment. #if, #ifdef, #ifndef, #else and #endif lines do not interrupt this. A
 *   comment at the end of a declaration's last line also documents that declaration.
 *
 *   Each module header in src/ starts with a module comment on its first line,
 *   "name: what the module is for", in 1 to 4 lines.
 *
 *   The top comment of each header gives the title and overview (its first paragraph,
 *   "file - what it is") and sections, each started by a line in capitals ("USAGE",
 *   "ERRORS", "LICENCE: ...") and indented beneath it. Text is plain; a block of lines
 *   that is indented further, or aligned with runs of spaces, is shown verbatim.
 *
 * WHAT IS PUBLIC
 *   tld.h: the declaration half, split into modules at the "---- src/<path> ----" marker
 *   comments that tools/amalgamate.py writes. tld_ai.h: everything outside
 *   AI_IMPLEMENTATION, split into sections at its own "---- name ----" comment lines.
 *   Skipped: anything inside #if/#ifdef ..._IMPLEMENTATION, static declarations, names
 *   that start with '_' or contain "__", include guards, macros #undef'd in the same
 *   module and forward declarations without a doc comment (the full definition
 *   is documented instead). A function-like macro whose body pastes its argument into
 *   prototypes (TLD__SORT_HEADER) is expanded where it is invoked, so tld_sort_asc_int32_t and
 *   the like are documented and indexed.
 *
 * Parsing is line-based and assumes the style of these headers.
 */
#define TLD_IMPLEMENTATION
#include "../tld.h"

#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#define MACRO_FULL_LINES 7   /* longer macros are shown as their signature only */

/* ------------------------------------------------------------------------ */
/* small containers                                                         */
/* ------------------------------------------------------------------------ */

typedef struct {
        char **s;
        int n;
        int n_alloc;
} strlist;

static char *xstrndup(const char *s, size_t n)
{
        char *d = NULL;
        MMALLOC(d, n + 1);
        memcpy(d, s, n);
        d[n] = 0;
        return d;
ERROR:
        return NULL;
}

static char *xstrdup(const char *s)
{
        return xstrndup(s, strlen(s));
}

static int sl_add(strlist *l, const char *s)
{
        char *c = NULL;
        if(l->n == l->n_alloc){
                l->n_alloc = l->n_alloc ? l->n_alloc * 2 : 8;
                MREALLOC(l->s, sizeof(char *) * (size_t)l->n_alloc);
        }
        c = xstrdup(s);
        ASSERT(c != NULL, "Out of memory");
        l->s[l->n++] = c;
        return OK;
ERROR:
        return FAIL;
}

static int sl_has(const strlist *l, const char *s)
{
        for(int i = 0; i < l->n; i++){
                if(strcmp(l->s[i], s) == 0){
                        return 1;
                }
        }
        return 0;
}

static void sl_free(strlist *l)
{
        for(int i = 0; i < l->n; i++){
                MFREE(l->s[i]);
        }
        MFREE(l->s);
        l->n = 0;
        l->n_alloc = 0;
}

static int is_ident(int c)
{
        return isalnum((unsigned char)c) || c == '_';
}

static const char *skip_ws(const char *p)
{
        while(*p && isspace((unsigned char)*p)){
                p++;
        }
        return p;
}

static int blank(const char *s)
{
        return *skip_ws(s) == 0;
}

static int starts(const char *s, const char *prefix)
{
        return strncmp(s, prefix, strlen(prefix)) == 0;
}

static int appendf(tld_strbuf *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static int appendf(tld_strbuf *b, const char *fmt, ...)
{
        char buf[4096];
        va_list ap;
        int n;
        va_start(ap, fmt);
        n = vsnprintf(buf, sizeof buf, fmt, ap);
        va_end(ap);
        ASSERT(n >= 0 && n < (int)sizeof buf, "Formatted text too long");
        RUN(tld_strbuf_append(b, buf));
        return OK;
ERROR:
        return FAIL;
}

/* Append a declaration as the manual shows it: a leading "TLD_API" (the linkage macro,
   documented once in its own entry) and the spaces after it are dropped from each line. */
static int append_code(tld_strbuf *b, const char *code)
{
        const char *p = code;
        while(*p){
                const char *nl = strchr(p, '\n');
                size_t len = nl ? (size_t)(nl - p + 1) : strlen(p);
                if(strncmp(p, "TLD_API ", 8) == 0){
                        size_t skip = 7;
                        while(skip < len && p[skip] == ' '){
                                skip++;
                        }
                        p += skip;
                        len -= skip;
                }
                RUN(tld_strbuf_append_n(b, p, len));
                p += len;
        }
        return OK;
ERROR:
        return FAIL;
}

/* Capitalise the first letter of a sentence, unless its first word is an identifier
   (contains '_' or a digit, e.g. tld_strbuf), which keeps its spelling. */
static void cap_first(char *s)
{
        size_t i = 0;
        while(isalnum((unsigned char)s[i]) || s[i] == '_'){
                if(s[i] == '_' || isdigit((unsigned char)s[i])){
                        return;
                }
                i++;
        }
        s[0] = (char)toupper((unsigned char)s[0]);
}

/* ------------------------------------------------------------------------ */
/* the parsed header                                                        */
/* ------------------------------------------------------------------------ */

enum { IT_BLANK, IT_COMMENT, IT_COND, IT_OTHER, IT_IGNORE, IT_DECL };
enum { D_FUNC, D_MACRO, D_TYPE, D_FWD, D_VAR, D_INVOKE };

typedef struct {
        int kind;
        int first;              /* source lines first..last */
        int last;
        int cond;               /* IT_COND: +1 opens, -1 closes, 0 #else / #elif */
        int dkind;              /* IT_DECL */
        int is_static;
        strlist names;          /* names declared */
        strlist kinds;          /* "function", "macro", ... per name */
        strlist extra;          /* enum constants, for the index */
        strlist generic;        /* D_INVOKE: names with <param> for the heading */
        char *code;             /* text shown in the manual */
        char *trail;            /* comment at the end of the last line */
        char *text;             /* IT_COMMENT: the comment text */
        int dead;               /* IT_COMMENT: commented-out code */
} item;

typedef struct {
        strlist names;
        strlist kinds;
        strlist extra;
        char *heading;          /* NULL: the names */
        tld_strbuf *code;
        char *doc;
        strlist trails;
        int n_decl;
        int n_trailed;
} entry;

typedef struct {
        char *name;             /* src/... path or section name */
        char *title;
        char *desc;             /* module comment without "title: " */
        char *anchor;
        entry **e;
        int n_e;
        int n_alloc;
} module;

typedef struct {
        char *file;             /* "tld.h" */
        char **l;               /* lines, without newline */
        int n;
        int n_alloc;
        uint8_t *skip;          /* line lies inside an ..._IMPLEMENTATION conditional */
        int top_end;            /* last line of the top comment */
        char *top;              /* the cleaned top comment */
        module **m;
        int n_m;
        int m_alloc;
        tld_hash *templates;    /* macro name -> "param\001body" */
        tld_hash *seen;         /* public names already documented */
} header;

static void item_clear(item *it)
{
        sl_free(&it->names);
        sl_free(&it->kinds);
        sl_free(&it->extra);
        sl_free(&it->generic);
        MFREE(it->code);
        MFREE(it->trail);
        MFREE(it->text);
}

static void entry_free(entry *e)
{
        if(e){
                sl_free(&e->names);
                sl_free(&e->kinds);
                sl_free(&e->extra);
                sl_free(&e->trails);
                MFREE(e->heading);
                MFREE(e->doc);
                tld_strbuf_free(e->code);
                MFREE(e);
        }
}

static void module_free(module *m)
{
        if(m){
                for(int i = 0; i < m->n_e; i++){
                        entry_free(m->e[i]);
                }
                MFREE(m->e);
                MFREE(m->name);
                MFREE(m->title);
                MFREE(m->desc);
                MFREE(m->anchor);
                MFREE(m);
        }
}

static void header_free(header *h)
{
        if(h){
                for(int i = 0; i < h->n; i++){
                        MFREE(h->l[i]);
                }
                MFREE(h->l);
                MFREE(h->skip);
                MFREE(h->top);
                MFREE(h->file);
                for(int i = 0; i < h->n_m; i++){
                        module_free(h->m[i]);
                }
                MFREE(h->m);
                tld_hash_free(h->templates);
                tld_hash_free(h->seen);
                MFREE(h);
        }
}

/* ------------------------------------------------------------------------ */
/* reading                                                                  */
/* ------------------------------------------------------------------------ */

/* The file at path, which must not contain NUL bytes. */
static int read_file(const char *path, tld_strbuf **out)
{
        tld_strbuf *b = NULL;
        RUN(tld_path_read(path, &b));
        ASSERT(memchr(b->str, 0, (size_t) b->len) == NULL, "%s contains a NUL byte", path);
        *out = b;
        return OK;
ERROR:
        tld_strbuf_free(b);
        return FAIL;
}

static int add_line(header *h, const char *s, size_t n)
{
        if(h->n == h->n_alloc){
                h->n_alloc = h->n_alloc ? h->n_alloc * 2 : 1024;
                MREALLOC(h->l, sizeof(char *) * (size_t)h->n_alloc);
        }
        h->l[h->n] = xstrndup(s, n);
        ASSERT(h->l[h->n] != NULL, "Out of memory");
        h->n++;
        return OK;
ERROR:
        return FAIL;
}

/* Does a '/' '*' comment that is open at the start of s stay open at its end? Strings
 * and character constants are skipped. */
static int comment_open_after(const char *s, int open)
{
        for(const char *p = s; *p; p++){
                if(open){
                        if(p[0] == '*' && p[1] == '/'){
                                open = 0;
                                p++;
                        }
                }else if(p[0] == '/' && p[1] == '*'){
                        open = 1;
                        p++;
                }else if(p[0] == '/' && p[1] == '/'){
                        break;
                }else if(*p == '"' || *p == '\''){
                        char q = *p++;
                        while(*p && *p != q){
                                if(*p == '\\' && p[1]){
                                        p++;
                                }
                                p++;
                        }
                        if(!*p){
                                break;
                        }
                }
        }
        return open;
}

/* The directive of a preprocessor line ("define", "ifdef", ...) into d; 0 if none. */
static int directive(const char *line, char *d, size_t dn)
{
        const char *p = skip_ws(line);
        size_t n = 0;
        if(*p != '#'){
                return 0;
        }
        p = skip_ws(p + 1);
        while(is_ident(p[n]) && n + 1 < dn){
                d[n] = p[n];
                n++;
        }
        d[n] = 0;
        return 1;
}

/* Mark the lines inside #if / #ifdef conditionals that mention _IMPLEMENTATION. */
static int mark_skip(header *h)
{
        int open = 0;
        int depth = 0;
        MMALLOC(h->skip, (size_t)h->n + 1);
        for(int i = 0; i < h->n; i++){
                char d[32];
                int was_open = open;
                open = comment_open_after(h->l[i], open);
                if(was_open || !directive(h->l[i], d, sizeof d)){
                        h->skip[i] = depth > 0;
                        continue;
                }
                if(depth > 0){
                        h->skip[i] = 1;
                        if(starts(d, "if")){
                                depth++;
                        }else if(strcmp(d, "endif") == 0){
                                depth--;
                        }
                        continue;
                }
                if((strcmp(d, "if") == 0 || strcmp(d, "ifdef") == 0) &&
                   strstr(h->l[i], "_IMPLEMENTATION")){
                        depth = 1;
                        h->skip[i] = 1;
                }
        }
        return OK;
ERROR:
        return FAIL;
}

/* ------------------------------------------------------------------------ */
/* comment text                                                             */
/* ------------------------------------------------------------------------ */

/* The text of the comment on lines first..last: comment markers and the leading
 * " * " removed, the common indentation removed, lines joined with '\n'. */
static int clean_comment(header *h, int first, int last, char **out)
{
        strlist lines = {0};
        tld_strbuf *b = NULL;
        int min_ind = 1 << 20;
        for(int i = first; i <= last; i++){
                const char *s = h->l[i];
                const char *p;
                const char *e;
                char *t;
                if(i == first){
                        p = strstr(s, "/*");
                        if(p){
                                p += 2;
                        }else{
                                p = strstr(s, "//");
                                p = p ? p + 2 : s;
                        }
                        if(*p == ' '){
                                p++;
                        }
                }else{
                        p = skip_ws(s);
                        if(starts(p, "//")){
                                p += 2;
                                if(*p == ' '){
                                        p++;
                                }
                        }else if(p[0] == '*' && p[1] != '/'){
                                p++;
                                if(*p == ' '){
                                        p++;
                                }
                        }
                }
                e = strstr(p, "*/");
                if(e == NULL){
                        e = p + strlen(p);
                }
                while(e > p && isspace((unsigned char)e[-1])){
                        e--;
                }
                t = xstrndup(p, (size_t)(e - p));
                ASSERT(t != NULL, "Out of memory");
                if(sl_add(&lines, t) != OK){
                        MFREE(t);
                        ERROR_MSG("Out of memory");
                }
                MFREE(t);
        }
        while(lines.n && lines.s[lines.n - 1][0] == 0){
                MFREE(lines.s[lines.n - 1]);
                lines.n--;
        }
        for(int i = 0; i < lines.n; i++){
                if(lines.s[i][0]){
                        int ind = (int)(skip_ws(lines.s[i]) - lines.s[i]);
                        if(ind < min_ind){
                                min_ind = ind;
                        }
                }
        }
        RUN(tld_strbuf_alloc(&b, 256));
        for(int i = 0; i < lines.n; i++){
                const char *s = lines.s[i];
                if((int)strlen(s) >= min_ind){
                        s += min_ind;
                }
                RUN(tld_strbuf_append(b, (char *)s));
                if(i + 1 < lines.n){
                        RUN(tld_strbuf_append_char(b, '\n'));
                }
        }
        *out = xstrdup(TLD_STR(b));
        ASSERT(*out != NULL, "Out of memory");
        tld_strbuf_free(b);
        sl_free(&lines);
        return OK;
ERROR:
        tld_strbuf_free(b);
        sl_free(&lines);
        return FAIL;
}

/* ------------------------------------------------------------------------ */
/* declarations                                                             */
/* ------------------------------------------------------------------------ */

/* The identifier that ends just before s[pos] (spaces, '*' and [...] skipped). */
static int ident_before(const char *s, int pos, char *out, size_t on)
{
        int e = pos;
        int b;
        for(;;){
                while(e > 0 && (isspace((unsigned char)s[e - 1]) || s[e - 1] == '*')){
                        e--;
                }
                if(e > 0 && s[e - 1] == ']'){
                        while(e > 0 && s[e - 1] != '['){
                                e--;
                        }
                        if(e > 0){
                                e--;
                        }
                        continue;
                }
                break;
        }
        b = e;
        while(b > 0 && is_ident(s[b - 1])){
                b--;
        }
        if(b == e || (size_t)(e - b) >= on){
                return FAIL;
        }
        memcpy(out, s + b, (size_t)(e - b));
        out[e - b] = 0;
        return OK;
}

static int ident_at(const char *s, char *out, size_t on)
{
        size_t n = 0;
        s = skip_ws(s);
        while(is_ident(s[n]) && n + 1 < on){
                out[n] = s[n];
                n++;
        }
        out[n] = 0;
        return n ? OK : FAIL;
}

/* Is name public? */
static int public_name(const char *name)
{
        return name[0] != '_' && strstr(name, "__") == NULL;
}

static int item_name(item *it, const char *name, const char *kind)
{
        if(!public_name(name) || sl_has(&it->names, name)){
                return OK;
        }
        RUN(sl_add(&it->names, name));
        RUN(sl_add(&it->kinds, kind));
        return OK;
ERROR:
        return FAIL;
}

/* The constants of the enum body starting at the '{' at s. */
static int enum_constants(const char *s, strlist *out)
{
        int depth = 0;
        const char *p = s;
        int want = 1;
        for(; *p; p++){
                if(*p == '{'){
                        depth++;
                        want = depth == 1;
                        continue;
                }
                if(*p == '}'){
                        if(--depth == 0){
                                break;
                        }
                        continue;
                }
                if(depth == 1 && *p == ','){
                        want = 1;
                        continue;
                }
                if(depth == 1 && want && is_ident(*p)){
                        char name[256];
                        RUN(ident_at(p, name, sizeof name));
                        if(public_name(name)){
                                RUN(sl_add(out, name));
                        }
                        p += strlen(name) - 1;
                        want = 0;
                }
        }
        return OK;
ERROR:
        return FAIL;
}

/* Classify the statement s (comments removed, spaces collapsed). */
static int classify(item *it, const char *s)
{
        char name[256];
        const char *brace = strchr(s, '{');
        const char *semi = strrchr(s, ';');
        int pos = semi ? (int)(semi - s) : (int)strlen(s);

        if(starts(s, "static ")){
                it->is_static = 1;
                return OK;
        }
        if(starts(s, "typedef ")){
                if(brace){
                        const char *close = strrchr(s, '}');
                        ASSERT(close != NULL, "Unbalanced braces in: %s", s);
                        RUN(ident_at(close + 1, name, sizeof name));
                        it->dkind = D_TYPE;
                        RUN(item_name(it, name, "type"));
                        if(starts(s, "typedef enum")){
                                RUN(enum_constants(brace, &it->extra));
                        }
                        return OK;
                }
                if(strstr(s, "(*")){
                        RUN(ident_at(strstr(s, "(*") + 2, name, sizeof name));
                        it->dkind = D_TYPE;
                        return item_name(it, name, "type");
                }
                RUN(ident_before(s, pos, name, sizeof name));
                it->dkind = (starts(s, "typedef struct") || starts(s, "typedef union") ||
                             starts(s, "typedef enum")) ? D_FWD : D_TYPE;
                return item_name(it, name, "type");
        }
        if(starts(s, "struct") || starts(s, "union") || starts(s, "enum")){
                const char *p = s + (s[0] == 'u' ? 5 : s[0] == 's' ? 6 : 4);
                int tagged;
                if(is_ident(*p)){
                        goto FUNC_OR_VAR;       /* e.g. "structure x;" */
                }
                tagged = ident_at(p, name, sizeof name) == OK;
                if(brace == NULL){
                        ASSERT(tagged, "Cannot read: %s", s);
                        it->dkind = D_FWD;
                        return item_name(it, name, "type");
                }
                it->dkind = D_TYPE;
                if(tagged && skip_ws(p) + strlen(name) <= brace){
                        RUN(item_name(it, name, "type"));
                        if(starts(s, "enum")){
                                RUN(enum_constants(brace, &it->extra));
                        }
                        return OK;
                }
                if(starts(s, "enum")){
                        /* anonymous enum: the constants are the symbols */
                        strlist c = {0};
                        if(enum_constants(brace, &c) != OK){
                                sl_free(&c);
                                ERROR_MSG("Cannot read enum: %s", s);
                        }
                        for(int i = 0; i < c.n; i++){
                                if(item_name(it, c.s[i], "constant") != OK){
                                        sl_free(&c);
                                        ERROR_MSG("Out of memory");
                                }
                        }
                        sl_free(&c);
                }
                return OK;
        }
FUNC_OR_VAR:
        if(strchr(s, '(')){
                RUN(ident_before(s, (int)(strchr(s, '(') - s), name, sizeof name));
                it->dkind = D_FUNC;
                return item_name(it, name, "function");
        }
        RUN(ident_before(s, pos, name, sizeof name));
        it->dkind = D_VAR;
        return item_name(it, name, "variable");
ERROR:
        return FAIL;
}

/* Expand the one-parameter macro template "param\001body" for arg into prototypes,
 * one per line, and their names. */
static int expand_template(const char *tmpl, const char *arg, tld_strbuf *code, strlist *names)
{
        const char *sep = strchr(tmpl, '\001');
        char param[128];
        tld_strbuf *b = NULL;
        tld_strbuf *stmt = NULL;
        size_t pn;
        ASSERT(sep != NULL, "Bad template");
        pn = (size_t)(sep - tmpl);
        ASSERT(pn < sizeof param, "Bad template");
        memcpy(param, tmpl, pn);
        param[pn] = 0;
        RUN(tld_strbuf_alloc(&b, 256));
        for(const char *p = sep + 1; *p;){
                if(p[0] == '#' && p[1] == '#'){
                        while(b->len && b->str[b->len - 1] == ' '){
                                b->len--;
                                b->str[b->len] = 0;
                        }
                        p = skip_ws(p + 2);
                        continue;
                }
                if(is_ident(*p)){
                        size_t n = 0;
                        while(is_ident(p[n])){
                                n++;
                        }
                        if(n == pn && strncmp(p, param, n) == 0){
                                RUN(tld_strbuf_append(b, (char *)arg));
                        }else{
                                RUN(tld_strbuf_append_n(b, p, n));
                        }
                        p += n;
                        continue;
                }
                if(isspace((unsigned char)*p)){
                        if(b->len && b->str[b->len - 1] != ' '){
                                RUN(tld_strbuf_append_char(b, ' '));
                        }
                        p++;
                        continue;
                }
                RUN(tld_strbuf_append_char(b, *p));
                p++;
        }
        /* split into statements; drop the space before '(' */
        RUN(tld_strbuf_alloc(&stmt, 256));
        for(int i = 0; i < b->len; i++){
                char c = (char)b->str[i];
                if(c == ' ' && i + 1 < b->len && b->str[i + 1] == '('){
                        continue;
                }
                if(c == ' ' && stmt->len == 0){
                        continue;
                }
                RUN(tld_strbuf_append_char(stmt, c));
                if(c == ';'){
                        char name[256];
                        const char *s = TLD_STR(stmt);
                        const char *paren = strchr(s, '(');
                        const char *nb;
                        ASSERT(paren != NULL, "Template does not declare a function: %s", s);
                        /* the name; '<' and '>' belong to it in the generic form */
                        nb = paren;
                        while(nb > s && (is_ident(nb[-1]) || nb[-1] == '<' || nb[-1] == '>')){
                                nb--;
                        }
                        ASSERT(nb < paren && (size_t)(paren - nb) < sizeof name,
                               "Template does not declare a function: %s", s);
                        memcpy(name, nb, (size_t)(paren - nb));
                        name[paren - nb] = 0;
                        if(names){
                                RUN(sl_add(names, name));
                        }
                        if(code){
                                RUN(tld_strbuf_append(code, (char *)s));
                                RUN(tld_strbuf_append_char(code, '\n'));
                        }
                        RUN(tld_strbuf_clear(stmt));
                }
        }
        tld_strbuf_free(b);
        tld_strbuf_free(stmt);
        return OK;
ERROR:
        tld_strbuf_free(b);
        tld_strbuf_free(stmt);
        return FAIL;
}

/* Lines first..last joined with '\n' (no final newline). */
static char *join_lines(header *h, int first, int last)
{
        tld_strbuf *b = NULL;
        char *r = NULL;
        RUN(tld_strbuf_alloc(&b, 256));
        for(int i = first; i <= last; i++){
                RUN(tld_strbuf_append(b, h->l[i]));
                if(i < last){
                        RUN(tld_strbuf_append_char(b, '\n'));
                }
        }
        r = xstrdup(TLD_STR(b));
        tld_strbuf_free(b);
        return r;
ERROR:
        tld_strbuf_free(b);
        return NULL;
}

/* A #define spanning lines first..last. */
static int lex_define(header *h, item *it, int first, int last, const char *last_ifndef)
{
        tld_strbuf *body = NULL;
        char name[256];
        char params[256];
        const char *p = skip_ws(h->l[first]);
        const char *q;
        int fnlike = 0;
        int n_params = 1;

        p = skip_ws(p + 1);             /* '#' */
        p = skip_ws(p + 6);             /* "define" */
        RUN(ident_at(p, name, sizeof name));
        q = p + strlen(name);
        params[0] = 0;
        if(*q == '('){
                const char *e = strchr(q, ')');
                ASSERT(e != NULL, "Unclosed macro parameters: %s", h->l[first]);
                ASSERT((size_t)(e - q - 1) < sizeof params, "Too many parameters");
                memcpy(params, q + 1, (size_t)(e - q - 1));
                params[e - q - 1] = 0;
                fnlike = 1;
                for(const char *c = params; *c; c++){
                        n_params += *c == ',';
                }
                q = e + 1;
        }
        /* the body, continuation lines joined */
        RUN(tld_strbuf_alloc(&body, 256));
        for(int i = first; i <= last; i++){
                const char *s = i == first ? q : h->l[i];
                size_t n = strlen(s);
                if(n && s[n - 1] == '\\'){
                        n--;
                }
                RUN(tld_strbuf_append_n(body, s, n));
                RUN(tld_strbuf_append_char(body, ' '));
        }
        if(blank(TLD_STR(body)) && strcmp(name, last_ifndef) == 0){
                it->kind = IT_IGNORE;   /* include guard */
                tld_strbuf_free(body);
                return OK;
        }
        it->kind = IT_DECL;
        it->dkind = D_MACRO;
        RUN(item_name(it, name, "macro"));
        if(fnlike && n_params == 1 && strstr(TLD_STR(body), "##")){
                tld_strbuf *t = NULL;
                char pname[128];
                RUN(ident_at(params, pname, sizeof pname));
                RUN(tld_strbuf_alloc(&t, 256));
                if(tld_strbuf_append(t, pname) != OK || tld_strbuf_append_char(t, '\001') != OK ||
                   tld_strbuf_append(t, TLD_STR(body)) != OK ||
                   tld_hash_add(&h->templates, name, TLD_STR(t)) != OK){
                        tld_strbuf_free(t);
                        ERROR_MSG("Out of memory");
                }
                tld_strbuf_free(t);
        }
        if(last - first + 1 <= MACRO_FULL_LINES){
                it->code = join_lines(h, first, last);
        }else{
                tld_strbuf *sig = NULL;
                RUN(tld_strbuf_alloc(&sig, 128));
                if(appendf(sig, "#define %s%s%s%s ...", name, fnlike ? "(" : "", params,
                           fnlike ? ")" : "") != OK){
                        tld_strbuf_free(sig);
                        ERROR_MSG("Out of memory");
                }
                it->code = xstrdup(TLD_STR(sig));
                tld_strbuf_free(sig);
        }
        ASSERT(it->code != NULL, "Out of memory");
        tld_strbuf_free(body);
        return OK;
ERROR:
        tld_strbuf_free(body);
        return FAIL;
}

/* A C statement starting at line first: read up to the ';' that ends it. */
static int lex_statement(header *h, item *it, int first, int end, int *lastp)
{
        tld_strbuf *s = NULL;
        int depth = 0;
        int open = 0;
        int done = 0;
        int i;
        RUN(tld_strbuf_alloc(&s, 256));
        for(i = first; i < end && !done; i++){
                const char *p = h->l[i];
                ASSERT(!h->skip[i], "Declaration runs into an implementation block at %s:%d",
                       h->file, first + 1);
                for(; *p; p++){
                        if(open){
                                if(p[0] == '*' && p[1] == '/'){
                                        open = 0;
                                        p++;
                                }
                                continue;
                        }
                        if(p[0] == '/' && p[1] == '*'){
                                open = 1;
                                p++;
                                continue;
                        }
                        if(p[0] == '/' && p[1] == '/'){
                                break;
                        }
                        if(*p == '{' || *p == '('){
                                depth++;
                        }else if(*p == '}' || *p == ')'){
                                depth--;
                        }
                        if(isspace((unsigned char)*p)){
                                if(s->len && s->str[s->len - 1] != ' '){
                                        RUN(tld_strbuf_append_char(s, ' '));
                                }
                                continue;
                        }
                        RUN(tld_strbuf_append_char(s, *p));
                        if(*p == ';' && depth == 0){
                                const char *t = skip_ws(p + 1);
                                done = 1;
                                if(starts(t, "/*")){
                                        const char *e = strstr(t, "*/");
                                        t = skip_ws(t + 2);
                                        if(e){
                                                while(e > t && isspace((unsigned char)e[-1])){
                                                        e--;
                                                }
                                                it->trail = xstrndup(t, (size_t)(e - t));
                                        }
                                }
                                break;
                        }
                }
                if(s->len && s->str[s->len - 1] != ' '){
                        RUN(tld_strbuf_append_char(s, ' '));
                }
        }
        ASSERT(done, "%s:%d: declaration without ';'", h->file, first + 1);
        *lastp = i - 1;
        it->kind = IT_DECL;
        RUN(classify(it, TLD_STR(s)));
        it->code = join_lines(h, first, i - 1);
        ASSERT(it->code != NULL, "Out of memory");
        tld_strbuf_free(s);
        return OK;
ERROR:
        tld_strbuf_free(s);
        return FAIL;
}

/* Is line a bare invocation NAME(ARG) of a template macro? */
static int is_invocation(header *h, const char *line, char *name, size_t nn, char *arg, size_t an)
{
        const char *p = skip_ws(line);
        const char *v = NULL;
        if(ident_at(p, name, nn) != OK){
                return 0;
        }
        p += strlen(name);
        if(*p != '('){
                return 0;
        }
        if(ident_at(p + 1, arg, an) != OK){
                return 0;
        }
        p = skip_ws(p + 1);
        p += strlen(arg);
        p = skip_ws(p);
        if(*p != ')' || !blank(p + 1)){
                return 0;
        }
        return h->templates && tld_hash_get(h->templates, name, &v) == OK;
}

/* Split lines begin..end-1 into items. */
static int lex(header *h, int begin, int end, item **out, int *n_out)
{
        item *it = NULL;
        int n = 0;
        int alloc = 0;
        char last_ifndef[256] = "";
        int i = begin;
        while(i < end){
                item *cur;
                const char *s = h->l[i];
                const char *t = skip_ws(s);
                char d[32];
                if(n == alloc){
                        alloc = alloc ? alloc * 2 : 64;
                        MREALLOC(it, sizeof(item) * (size_t)alloc);
                }
                cur = &it[n];
                memset(cur, 0, sizeof *cur);
                cur->first = i;
                cur->last = i;
                n++;
                if(h->skip[i] || *t == 0){
                        cur->kind = IT_BLANK;
                        i++;
                        continue;
                }
                if(starts(t, "/*")){
                        int j = i;
                        while(j < end && !strstr(j == i ? t : h->l[j], "*/")){
                                j++;
                        }
                        ASSERT(j < end, "%s:%d: unterminated comment", h->file, i + 1);
                        cur->kind = IT_COMMENT;
                        cur->last = j;
                        RUN(clean_comment(h, i, j, &cur->text));
                        {
                                const char *e = cur->text + strlen(cur->text);
                                while(e > cur->text && isspace((unsigned char)e[-1])){
                                        e--;
                                }
                                /* a one-line comment ending in ';' is commented-out code */
                                cur->dead = j == i && e > cur->text && e[-1] == ';';
                        }
                        i = j + 1;
                        continue;
                }
                if(starts(t, "//")){
                        int j = i;
                        while(j + 1 < end && starts(skip_ws(h->l[j + 1]), "//")){
                                j++;
                        }
                        cur->kind = IT_COMMENT;
                        cur->last = j;
                        RUN(clean_comment(h, i, j, &cur->text));
                        i = j + 1;
                        continue;
                }
                if(directive(s, d, sizeof d)){
                        int j = i;
                        while(j + 1 < end && strlen(h->l[j]) && h->l[j][strlen(h->l[j]) - 1] == '\\'){
                                j++;
                        }
                        cur->last = j;
                        if(strcmp(d, "define") == 0){
                                RUN(lex_define(h, cur, i, j, last_ifndef));
                                last_ifndef[0] = 0;
                        }else if(starts(d, "if") || strcmp(d, "else") == 0 ||
                                 strcmp(d, "elif") == 0 || strcmp(d, "endif") == 0){
                                cur->kind = IT_COND;
                                cur->cond = starts(d, "if") ? 1 : strcmp(d, "endif") == 0 ? -1 : 0;
                                last_ifndef[0] = 0;
                                if(strcmp(d, "ifndef") == 0){
                                        const char *p = skip_ws(skip_ws(t + 1) + 6);
                                        if(ident_at(p, last_ifndef, sizeof last_ifndef) != OK){
                                                last_ifndef[0] = 0;
                                        }
                                }
                        }else{
                                cur->kind = IT_OTHER;
                                last_ifndef[0] = 0;
                        }
                        i = j + 1;
                        continue;
                }
                last_ifndef[0] = 0;
                if(starts(t, "extern \"C\"") || strcmp(t, "}") == 0){
                        cur->kind = IT_IGNORE;  /* C++ linkage block */
                        i++;
                        continue;
                }
                {
                        char name[256];
                        char arg[256];
                        if(is_invocation(h, s, name, sizeof name, arg, sizeof arg)){
                                const char *tmpl = NULL;
                                tld_strbuf *code = NULL;
                                char generic[300];
                                strlist names = {0};
                                RUN(tld_hash_get(h->templates, name, &tmpl));
                                RUN(tld_strbuf_alloc(&code, 256));
                                cur->kind = IT_DECL;
                                cur->dkind = D_INVOKE;
                                {
                                        /* the generic names use <param> */
                                        size_t pn = (size_t)(strchr(tmpl, '\001') - tmpl);
                                        ASSERT(pn + 3 < sizeof generic, "Parameter name too long");
                                        generic[0] = '<';
                                        memcpy(generic + 1, tmpl, pn);
                                        generic[pn + 1] = '>';
                                        generic[pn + 2] = 0;
                                }
                                if(expand_template(tmpl, arg, code, &names) != OK ||
                                   expand_template(tmpl, generic, NULL, &cur->generic) != OK){
                                        tld_strbuf_free(code);
                                        sl_free(&names);
                                        ERROR_MSG("Cannot expand %s(%s)", name, arg);
                                }
                                for(int k = 0; k < names.n; k++){
                                        if(item_name(cur, names.s[k], "function") != OK){
                                                tld_strbuf_free(code);
                                                sl_free(&names);
                                                ERROR_MSG("Out of memory");
                                        }
                                }
                                sl_free(&names);
                                if(code->len && code->str[code->len - 1] == '\n'){
                                        code->len--;
                                        code->str[code->len] = 0;
                                }
                                cur->code = xstrdup(TLD_STR(code));
                                tld_strbuf_free(code);
                                ASSERT(cur->code != NULL, "Out of memory");
                                i++;
                                continue;
                        }
                }
                {
                        int last = i;
                        RUN(lex_statement(h, cur, i, end, &last));
                        cur->last = last;
                        i = last + 1;
                }
        }
        *out = it;
        *n_out = n;
        return OK;
ERROR:
        for(int k = 0; k < n; k++){
                item_clear(&it[k]);
        }
        MFREE(it);
        return FAIL;
}

/* ------------------------------------------------------------------------ */
/* grouping items into documented entries                                   */
/* ------------------------------------------------------------------------ */

static int module_add_entry(module *m, entry *e)
{
        if(m->n_e == m->n_alloc){
                m->n_alloc = m->n_alloc ? m->n_alloc * 2 : 16;
                MREALLOC(m->e, sizeof(entry *) * (size_t)m->n_alloc);
        }
        m->e[m->n_e++] = e;
        return OK;
ERROR:
        return FAIL;
}

static int entry_new(entry **out, const char *doc)
{
        entry *e = NULL;
        MMALLOC(e, sizeof(entry));
        RUN(tld_strbuf_alloc(&e->code, 256));
        if(doc){
                e->doc = xstrdup(doc);
                ASSERT(e->doc != NULL, "Out of memory");
        }
        *out = e;
        return OK;
ERROR:
        entry_free(e);
        return FAIL;
}

/* Append a code line to the entry. */
static int entry_code(entry *e, const char *s)
{
        if(e->code->len){
                RUN(tld_strbuf_append_char(e->code, '\n'));
        }
        RUN(tld_strbuf_append(e->code, (char *)s));
        return OK;
ERROR:
        return FAIL;
}

/* Close the current entry: add the #endif lines held after it while conditionals it
 * opened are still open, then keep it if it declares anything. */
static int entry_close(header *h, module *m, entry **cur, item *it, int *held, int *n_held,
                       int *balance)
{
        entry *e = *cur;
        if(e == NULL){
                *n_held = 0;
                return OK;
        }
        for(int k = 0; k < *n_held && *balance > 0; k++){
                item *x = &it[held[k]];
                if(x->kind == IT_COND && x->cond < 0){
                        RUN(entry_code(e, h->l[x->first]));
                        (*balance)--;
                }else{
                        break;
                }
        }
        *n_held = 0;
        *balance = 0;
        *cur = NULL;
        if(e->names.n == 0){
                entry_free(e);
                return OK;
        }
        RUN(module_add_entry(m, e));
        return OK;
ERROR:
        return FAIL;
}

static int set_module_comment(module *m, const char *text)
{
        const char *colon = strchr(text, ':');
        const char *nl = strchr(text, '\n');
        int words = 1;
        if(colon && (nl == NULL || colon < nl) && colon - text < 40){
                for(const char *p = text; p < colon; p++){
                        words += *p == ' ';
                }
        }
        if(colon && words <= 3 && (nl == NULL || colon < nl) && colon - text < 40){
                MFREE(m->title);
                m->title = xstrndup(text, (size_t)(colon - text));
                ASSERT(m->title != NULL, "Out of memory");
                text = skip_ws(colon + 1);
        }
        m->desc = xstrdup(text);
        ASSERT(m->desc != NULL, "Out of memory");
        if(m->desc[0]){
                cap_first(m->desc);
        }
        /* one paragraph: join the lines */
        for(char *p = m->desc; *p; p++){
                if(*p == '\n'){
                        *p = ' ';
                }
        }
        return OK;
ERROR:
        return FAIL;
}

static int make_anchor(const char *prefix, const char *title, char **out)
{
        tld_strbuf *b = NULL;
        RUN(tld_strbuf_alloc(&b, 64));
        RUN(tld_strbuf_append(b, (char *)prefix));
        for(const char *p = title; *p; p++){
                if(isalnum((unsigned char)*p) || *p == '_'){
                        RUN(tld_strbuf_append_char(b, (char)tolower((unsigned char)*p)));
                }else if((*p == ' ' || *p == '-' || *p == '/' || *p == '.') && b->len &&
                         b->str[b->len - 1] != '-'){
                        RUN(tld_strbuf_append_char(b, '-'));
                }
        }
        while(b->len && b->str[b->len - 1] == '-'){
                b->len--;
                b->str[b->len] = 0;
        }
        *out = xstrdup(TLD_STR(b));
        tld_strbuf_free(b);
        return *out ? OK : FAIL;
ERROR:
        tld_strbuf_free(b);
        return FAIL;
}

/* Undefined names (#undef) in lines begin..end-1. */
static int undefs(header *h, int begin, int end, strlist *out)
{
        for(int i = begin; i < end; i++){
                char d[32];
                if(!h->skip[i] && directive(h->l[i], d, sizeof d) && strcmp(d, "undef") == 0){
                        char name[256];
                        const char *p = skip_ws(skip_ws(skip_ws(h->l[i]) + 1) + 5);
                        if(ident_at(p, name, sizeof name) == OK){
                                RUN(sl_add(out, name));
                        }
                }
        }
        return OK;
ERROR:
        return FAIL;
}

static int parse_module(header *h, const char *name, int begin, int end)
{
        module *m = NULL;
        item *it = NULL;
        int n = 0;
        entry *cur = NULL;
        const char *pending = NULL;
        int held[256];
        int n_held = 0;
        int balance = 0;
        strlist undef = {0};
        const char *v = NULL;
        const char *base;

        MMALLOC(m, sizeof(module));
        m->name = xstrdup(name);
        ASSERT(m->name != NULL, "Out of memory");
        base = strrchr(name, '/');
        base = base ? base + 1 : name;
        m->title = starts(name, "src/") ? xstrndup(base, strcspn(base, ".")) : xstrdup(name);
        ASSERT(m->title != NULL, "Out of memory");

        RUN(undefs(h, begin, end, &undef));
        RUN(lex(h, begin, end, &it, &n));

        for(int k = 0; k < n; k++){
                item *x = &it[k];
                if(k == 0 && x->kind == IT_COMMENT && x->first == begin){
                        RUN(set_module_comment(m, x->text));
                        continue;
                }
                switch(x->kind){
                case IT_BLANK:
                case IT_OTHER:
                        RUN(entry_close(h, m, &cur, it, held, &n_held, &balance));
                        pending = NULL;
                        break;
                case IT_COMMENT:
                        RUN(entry_close(h, m, &cur, it, held, &n_held, &balance));
                        pending = x->dead ? NULL : x->text;
                        break;
                case IT_COND:
                case IT_IGNORE:
                        if((cur || pending) && x->kind == IT_COND){
                                ASSERT(n_held < (int)(sizeof held / sizeof held[0]),
                                       "Too many conditionals");
                                held[n_held++] = k;
                        }
                        break;
                case IT_DECL:{
                        int fresh = cur == NULL;
                        int had = x->names.n;
                        int in_cur = 0;
                        /* names #undef'd here or already documented (in this entry: a
                         * second definition under #else) */
                        for(int j = 0; j < x->names.n;){
                                int dup = cur && sl_has(&cur->names, x->names.s[j]);
                                in_cur += dup;
                                if(dup || sl_has(&undef, x->names.s[j]) ||
                                   (h->seen && tld_hash_get(h->seen, x->names.s[j], &v) == OK)){
                                        MFREE(x->names.s[j]);
                                        MFREE(x->kinds.s[j]);
                                        for(int q = j + 1; q < x->names.n; q++){
                                                x->names.s[q - 1] = x->names.s[q];
                                                x->kinds.s[q - 1] = x->kinds.s[q];
                                        }
                                        x->names.n--;
                                        x->kinds.n--;
                                }else{
                                        j++;
                                }
                        }
                        if(x->is_static || (x->names.n == 0 && !(had && in_cur == had)) ||
                           (fresh && x->dkind == D_FWD && pending == NULL)){
                                /* consumes the comment above; not a symbol */
                                RUN(entry_close(h, m, &cur, it, held, &n_held, &balance));
                                pending = NULL;
                                break;
                        }
                        if(fresh){
                                RUN(entry_new(&cur, pending));
                                pending = NULL;
                                if(x->dkind == D_INVOKE && x->generic.n){
                                        tld_strbuf *hd = NULL;
                                        RUN(tld_strbuf_alloc(&hd, 128));
                                        for(int j = 0; j < x->generic.n; j++){
                                                if(appendf(hd, "%s`%s`", j ? ", " : "",
                                                           x->generic.s[j]) != OK){
                                                        tld_strbuf_free(hd);
                                                        ERROR_MSG("Out of memory");
                                                }
                                        }
                                        cur->heading = xstrdup(TLD_STR(hd));
                                        tld_strbuf_free(hd);
                                        ASSERT(cur->heading != NULL, "Out of memory");
                                }
                        }
                        for(int j = 0; j < n_held; j++){
                                item *c = &it[held[j]];
                                RUN(entry_code(cur, h->l[c->first]));
                                balance += c->cond;
                                if(balance < 0){
                                        balance = 0;
                                }
                        }
                        n_held = 0;
                        RUN(entry_code(cur, x->code));
                        for(int j = 0; j < x->names.n; j++){
                                RUN(sl_add(&cur->names, x->names.s[j]));
                                RUN(sl_add(&cur->kinds, x->kinds.s[j]));
                                RUN(tld_hash_add(&h->seen, x->names.s[j], m->title));
                        }
                        for(int j = 0; j < x->extra.n; j++){
                                RUN(sl_add(&cur->extra, x->extra.s[j]));
                        }
                        cur->n_decl++;
                        if(x->trail){
                                cur->n_trailed++;
                                RUN(sl_add(&cur->trails, x->trail));
                        }
                        break;
                }
                }
        }
        RUN(entry_close(h, m, &cur, it, held, &n_held, &balance));
        RUN(make_anchor("mod-", m->title, &m->anchor));

        for(int k = 0; k < n; k++){
                item_clear(&it[k]);
        }
        MFREE(it);
        sl_free(&undef);
        if(m->n_e == 0){
                module_free(m);
                return OK;
        }
        if(h->n_m == h->m_alloc){
                h->m_alloc = h->m_alloc ? h->m_alloc * 2 : 32;
                MREALLOC(h->m, sizeof(module *) * (size_t)h->m_alloc);
        }
        h->m[h->n_m++] = m;
        return OK;
ERROR:
        entry_free(cur);
        for(int k = 0; k < n; k++){
                item_clear(&it[k]);
        }
        MFREE(it);
        sl_free(&undef);
        module_free(m);
        return FAIL;
}

/* A marker comment "---- name ----" (a whole line): the name into out. */
static int marker(const char *line, char *out, size_t on)
{
        size_t n = strlen(line);
        const char *b;
        const char *e;
        int dashes = 0;
        while(n && isspace((unsigned char)line[n - 1])){
                n--;
        }
        if(!starts(line, "/* ---- ") || n < 11 || line[n - 1] != '/' || line[n - 2] != '*'){
                return 0;
        }
        b = line + 8;
        e = line + n - 2;
        while(e > b && e[-1] == ' '){
                e--;
        }
        while(e > b && e[-1] == '-'){
                e--;
                dashes++;
        }
        while(e > b && e[-1] == ' '){
                e--;
        }
        if(dashes < 2 || e == b || (size_t)(e - b) >= on){
                return 0;
        }
        memcpy(out, b, (size_t)(e - b));
        out[e - b] = 0;
        return 1;
}

static int parse_header(const char *root, const char *file, header **out)
{
        header *h = NULL;
        tld_strbuf *path = NULL;
        tld_strbuf *txt = NULL;
        int open = 0;
        int begin;
        char name[256] = "general";

        MMALLOC(h, sizeof(header));
        h->file = xstrdup(file);
        ASSERT(h->file != NULL, "Out of memory");
        RUN(tld_strbuf_alloc(&path, 256));
        RUN(tld_strbuf_append(path, (char *)root));
        RUN(tld_strbuf_append(path, "/"));
        RUN(tld_strbuf_append(path, (char *)file));
        RUN(read_file(TLD_STR(path), &txt));
        {
                const char *s = TLD_STR(txt);
                while(*s){
                        const char *nl = strchr(s, '\n');
                        size_t n = nl ? (size_t)(nl - s) : strlen(s);
                        RUN(add_line(h, s, n));
                        s += n + (nl ? 1 : 0);
                }
        }
        tld_strbuf_free(txt);
        txt = NULL;
        RUN(mark_skip(h));

        /* the top comment */
        ASSERT(h->n > 0 && starts(h->l[0], "/*"), "%s does not start with a comment", file);
        h->top_end = 0;
        while(h->top_end < h->n && !strstr(h->l[h->top_end], "*/")){
                h->top_end++;
        }
        ASSERT(h->top_end < h->n, "%s: unterminated top comment", file);
        RUN(clean_comment(h, 0, h->top_end, &h->top));

        /* modules: from marker to marker */
        begin = h->top_end + 1;
        for(int i = begin; i <= h->n; i++){
                char next[256];
                int is_marker = 0;
                if(i < h->n){
                        int was_open = open;
                        open = comment_open_after(h->l[i], open);
                        is_marker = !was_open && !h->skip[i] && marker(h->l[i], next, sizeof next);
                }
                if(i == h->n || is_marker){
                        RUN(parse_module(h, name, begin, i));
                        if(is_marker){
                                snprintf(name, sizeof name, "%s", next);
                                begin = i + 1;
                        }
                }
        }
        tld_strbuf_free(path);
        *out = h;
        return OK;
ERROR:
        tld_strbuf_free(path);
        tld_strbuf_free(txt);
        header_free(h);
        return FAIL;
}

/* ------------------------------------------------------------------------ */
/* markdown                                                                 */
/* ------------------------------------------------------------------------ */

static int is_pre_line(const char *s)
{
        const char *p = skip_ws(s);
        if(p != s && *p){
                return 1;
        }
        for(; *p; p++){
                if(p[0] == ' ' && p[1] == ' ' && p[2] && p[2] != ' '){
                        return 1;
                }
        }
        return 0;
}

/* Length of the code on line s: without a trailing // comment (outside string and
 * character literals) and without trailing white space. */
static size_t code_len(const char *s)
{
        size_t n = 0;
        char quote = 0;
        for(; s[n]; n++){
                if(quote){
                        if(s[n] == '\\' && s[n + 1]){
                                n++;
                        }else if(s[n] == quote){
                                quote = 0;
                        }
                }else if(s[n] == '"' || s[n] == '\''){
                        quote = s[n];
                }else if(s[n] == '/' && s[n + 1] == '/'){
                        break;
                }
        }
        while(n && isspace((unsigned char)s[n - 1])){
                n--;
        }
        return n;
}

static int looks_like_code(strlist *blk)
{
        for(int i = 0; i < blk->n; i++){
                const char *s = skip_ws(blk->s[i]);
                size_t n = code_len(s);
                if(starts(s, "#define") || starts(s, "#include") ||
                   (n && (s[n - 1] == ';' || s[n - 1] == '{' || s[n - 1] == '}'))){
                        return 1;
                }
        }
        return 0;
}

static int write_file(const char *path, tld_strbuf *text);

/* --examples: every block shown as C is also written to its own file in ex_dir. */
static const char *ex_dir = NULL;       /* NULL: not collecting */
static const char *ex_file = NULL;      /* header being rendered, "tld.h" */
static const char *ex_where = NULL;     /* the symbol or section the block documents */
static int ex_n = 0;                    /* examples written for ex_file */

/* A line that belongs at file scope: a preprocessor line, or a top-level macro
 * invocation such as AI_CHOICE(team, "...", TEAM) (ends in ')', no ';'). */
static int file_scope_line(const char *s)
{
        size_t n = code_len(s);
        if(s[0] == '#'){
                return 1;
        }
        return n && s[n - 1] == ')' && isupper((unsigned char)s[0]) && strchr(s, '(') != NULL;
}

/* A function definition: its first line is "type name(args)" with no ';', and a line
 * holding just "{" (or the first line ending in '{') follows. */
static int is_definition(strlist *blk, int min_ind)
{
        const char *s = blk->s[0] + min_ind;
        size_t n = code_len(s);
        if(!isalpha((unsigned char)s[0]) || n == 0 || strchr(s, '(') == NULL || strchr(s, '=')){
                return 0;
        }
        if(s[n - 1] == '{'){
                return 1;
        }
        return s[n - 1] == ')' && blk->n > 1 && strcmp(skip_ws(blk->s[1]), "{") == 0;
}

/* Write blk as DIR/<stem>-NN.c: file-scope lines first, the rest in a function body that
 * has the ERROR label RUN and ASSERT jump to, and a main so the file links. Blocks with
 * an #include (the USAGE lines) are skipped: the file already includes the header. */
static int write_example(strlist *blk, int min_ind)
{
        tld_strbuf *b = NULL;
        char path[1024];
        char stem[64];
        int def = 0;

        for(int i = 0; i < blk->n; i++){
                if(starts(skip_ws(blk->s[i]), "#include")){
                        return OK;
                }
        }
        snprintf(stem, sizeof stem, "%s", ex_file);
        if(strrchr(stem, '.')){
                *strrchr(stem, '.') = 0;
        }
        ex_n++;
        snprintf(path, sizeof path, "%s/%s-%02d.c", ex_dir, stem, ex_n);
        def = is_definition(blk, min_ind);
        RUN(tld_strbuf_alloc(&b, 1024));
        RUN(appendf(b, "/* Example from the doc comment of %s in %s (tlddoc --examples). */\n"
                    "#include \"%s\"\n\n", ex_where ? ex_where : "?", ex_file, ex_file));
        /* file scope: a whole definition, or the #define lines (with their continuation
         * lines) and top-level invocations */
        for(int i = 0, cont = 0; i < blk->n; i++){
                const char *s = blk->s[i] + min_ind;
                size_t n = strlen(s);
                int here = def || cont || file_scope_line(s);
                cont = here && s[0] == '#' ? n && s[n - 1] == '\\' : cont && n && s[n - 1] == '\\';
                if(here){
                        RUN(appendf(b, "%s\n", s));
                }
        }
        RUN(tld_strbuf_append(b, "\nstatic int example(void)\n{\n"));
        for(int i = 0, cont = 0; i < blk->n && !def; i++){
                const char *s = blk->s[i] + min_ind;
                size_t n = strlen(s);
                int here = cont || file_scope_line(s);
                cont = here && s[0] == '#' ? n && s[n - 1] == '\\' : cont && n && s[n - 1] == '\\';
                if(!here){
                        RUN(appendf(b, "        %s\n", s));
                }
        }
        RUN(tld_strbuf_append(b, "        return OK;\nERROR:\n        return FAIL;\n}\n\n"
                              "int main(void)\n{\n        return example() == OK ? 0 : 1;\n}\n"));
        RUN(write_file(path, b));
        tld_strbuf_free(b);
        return OK;
ERROR:
        tld_strbuf_free(b);
        return FAIL;
}

static int flush_block(tld_strbuf *o, strlist *blk)
{
        int pre = 0;
        int min_ind = 1 << 20;
        if(blk->n == 0){
                return OK;
        }
        for(int i = 0; i < blk->n; i++){
                int ind = (int)(skip_ws(blk->s[i]) - blk->s[i]);
                pre |= is_pre_line(blk->s[i]);
                if(ind < min_ind){
                        min_ind = ind;
                }
        }
        if(pre){
                if(ex_dir && looks_like_code(blk)){
                        RUN(write_example(blk, min_ind));
                }
                RUN(appendf(o, "```%s\n", looks_like_code(blk) ? "c" : "text"));
                for(int i = 0; i < blk->n; i++){
                        RUN(tld_strbuf_append(o, blk->s[i] + min_ind));
                        RUN(tld_strbuf_append_char(o, '\n'));
                }
                RUN(tld_strbuf_append(o, "```\n\n"));
        }else{
                for(int i = 0; i < blk->n; i++){
                        RUN(tld_strbuf_append(o, blk->s[i]));
                        RUN(tld_strbuf_append_char(o, '\n'));
                }
                RUN(tld_strbuf_append_char(o, '\n'));
        }
        sl_free(blk);
        return OK;
ERROR:
        return FAIL;
}

/* Text as markdown: paragraphs, and indented or aligned blocks verbatim. The text's
 * lines are cut at '\n'; base characters of indentation are removed from each. */
static int render_text(tld_strbuf *o, const char *text, int base)
{
        strlist blk = {0};
        const char *s = text;
        while(*s){
                const char *nl = strchr(s, '\n');
                size_t n = nl ? (size_t)(nl - s) : strlen(s);
                char *line = xstrndup(s, n);
                ASSERT(line != NULL, "Out of memory");
                if(blank(line)){
                        MFREE(line);
                        RUN(flush_block(o, &blk));
                }else{
                        const char *p = line;
                        for(int k = 0; k < base && *p == ' '; k++){
                                p++;
                        }
                        if(sl_add(&blk, p) != OK){
                                MFREE(line);
                                ERROR_MSG("Out of memory");
                        }
                        MFREE(line);
                }
                s += n + (nl ? 1 : 0);
        }
        RUN(flush_block(o, &blk));
        return OK;
ERROR:
        sl_free(&blk);
        return FAIL;
}

/* A top-comment section heading: "USAGE", "LICENCE: MIT ...". Returns the length of
 * the heading word(s), 0 if line is not a heading. */
static int heading_len(const char *line)
{
        int n = 0;
        int letters = 0;
        if(!isupper((unsigned char)line[0])){
                return 0;
        }
        while(isupper((unsigned char)line[n]) || line[n] == ' '){
                letters += line[n] != ' ';
                n++;
        }
        while(n > 0 && line[n - 1] == ' '){
                n--;
        }
        if(letters < 4 || (line[n] != 0 && line[n] != ':')){
                return 0;
        }
        return n;
}

typedef struct {
        char *title;            /* "Usage" */
        char *anchor;
        char *body;             /* text below the heading */
} section;

static void sections_free(section *s, int n)
{
        for(int i = 0; i < n; i++){
                MFREE(s[i].title);
                MFREE(s[i].anchor);
                MFREE(s[i].body);
        }
        MFREE(s);
}

/* Split the top comment into title, overview, preamble and sections. */
static int split_top(const char *top, char **title, char **overview, char **preamble,
                     section **secs, int *n_secs)
{
        strlist lines = {0};
        tld_strbuf *b = NULL;
        section *sec = NULL;
        int n_sec = 0;
        int i = 0;
        const char *s = top;
        while(*s){
                const char *nl = strchr(s, '\n');
                size_t n = nl ? (size_t)(nl - s) : strlen(s);
                char *line = xstrndup(s, n);
                ASSERT(line != NULL, "Out of memory");
                if(sl_add(&lines, line) != OK){
                        MFREE(line);
                        ERROR_MSG("Out of memory");
                }
                MFREE(line);
                s += n + (nl ? 1 : 0);
        }
        RUN(tld_strbuf_alloc(&b, 1024));
        /* overview: the first paragraph, "file - text" */
        for(; i < lines.n && !blank(lines.s[i]); i++){
                RUN(tld_strbuf_append(b, lines.s[i]));
                RUN(tld_strbuf_append_char(b, i + 1 < lines.n && !blank(lines.s[i + 1]) ? ' ' : '\n'));
        }
        {
                const char *ov = TLD_STR(b);
                const char *dash = strstr(ov, " - ");
                if(dash){
                        *title = xstrndup(ov, (size_t)(dash - ov));
                        ov = dash + 3;
                }else{
                        *title = xstrdup("manual");
                }
                *overview = xstrdup(ov);
                ASSERT(*title != NULL && *overview != NULL, "Out of memory");
                if((*overview)[0]){
                        cap_first(*overview);
                }
        }
        /* preamble: up to the first heading */
        RUN(tld_strbuf_clear(b));
        for(; i < lines.n && !heading_len(lines.s[i]); i++){
                RUN(tld_strbuf_append(b, lines.s[i]));
                RUN(tld_strbuf_append_char(b, '\n'));
        }
        *preamble = xstrdup(TLD_STR(b));
        ASSERT(*preamble != NULL, "Out of memory");
        /* sections */
        while(i < lines.n){
                int hl = heading_len(lines.s[i]);
                const char *rest = skip_ws(lines.s[i] + hl + (lines.s[i][hl] == ':'));
                section *x;
                MREALLOC(sec, sizeof(section) * (size_t)(n_sec + 1));
                x = &sec[n_sec++];
                memset(x, 0, sizeof *x);
                x->title = xstrndup(lines.s[i], (size_t)hl);
                ASSERT(x->title != NULL, "Out of memory");
                for(char *p = x->title + 1; *p; p++){
                        *p = (char)tolower((unsigned char)*p);
                }
                RUN(make_anchor("sec-", x->title, &x->anchor));
                RUN(tld_strbuf_clear(b));
                if(*rest){
                        RUN(tld_strbuf_append(b, "  "));
                        RUN(tld_strbuf_append(b, (char *)rest));
                        RUN(tld_strbuf_append_char(b, '\n'));
                }
                for(i++; i < lines.n && !heading_len(lines.s[i]); i++){
                        RUN(tld_strbuf_append(b, lines.s[i]));
                        RUN(tld_strbuf_append_char(b, '\n'));
                }
                x->body = xstrdup(TLD_STR(b));
                ASSERT(x->body != NULL, "Out of memory");
        }
        tld_strbuf_free(b);
        sl_free(&lines);
        *secs = sec;
        *n_secs = n_sec;
        return OK;
ERROR:
        tld_strbuf_free(b);
        sl_free(&lines);
        sections_free(sec, n_sec);
        return FAIL;
}

/* The smallest indentation of the non-blank lines of text. */
static int text_indent(const char *text)
{
        int min_ind = 1 << 20;
        const char *s = text;
        while(*s){
                const char *nl = strchr(s, '\n');
                size_t n = nl ? (size_t)(nl - s) : strlen(s);
                const char *p = s;
                while(p < s + n && *p == ' '){
                        p++;
                }
                if(p < s + n && (int)(p - s) < min_ind){
                        min_ind = (int)(p - s);
                }
                s += n + (nl ? 1 : 0);
        }
        return min_ind == 1 << 20 ? 0 : min_ind;
}

typedef struct {
        const char *name;
        const char *kind;
        const char *module;
        const char *mod_anchor;
        const char *anchor;
} idx_row;

static int idx_cmp(const void *a, const void *b)
{
        const idx_row *x = a;
        const idx_row *y = b;
        for(int i = 0;; i++){
                int cx = tolower((unsigned char)x->name[i]);
                int cy = tolower((unsigned char)y->name[i]);
                if(cx != cy || cx == 0){
                        if(cx != cy){
                                return cx < cy ? -1 : 1;
                        }
                        break;
                }
        }
        return strcmp(x->name, y->name);
}

static int count_symbols(header *h)
{
        int n = 0;
        for(int i = 0; i < h->n_m; i++){
                for(int j = 0; j < h->m[i]->n_e; j++){
                        n += h->m[i]->e[j]->names.n;
                }
        }
        return n;
}

static int render(header *h, const char *impl_macro, tld_strbuf *o)
{
        char *title = NULL;
        char *overview = NULL;
        char *preamble = NULL;
        section *sec = NULL;
        int n_sec = 0;
        idx_row *rows = NULL;
        int n_rows = 0;

        RUN(split_top(h->top, &title, &overview, &preamble, &sec, &n_sec));

        RUN(appendf(o, "<!-- Generated by tools/tlddoc.c from %s; do not edit. "
                    "Regenerate with: cmake --build build --target docs -->\n\n", h->file));
        RUN(appendf(o, "# %s\n\n", title));
        RUN(appendf(o, "%s\n", overview));
        ex_where = "the top comment";
        RUN(render_text(o, preamble, text_indent(preamble)));
        RUN(appendf(o, "Every public symbol of %s is documented below, module by module, with "
                    "its declaration; the [index](#index) at the end lists them "
                    "alphabetically. Implementation: define %s in one .c file (see Usage).\n\n",
                    h->file, impl_macro));

        /* contents */
        RUN(tld_strbuf_append(o, "## Contents\n\n"));
        for(int i = 0; i < n_sec; i++){
                RUN(appendf(o, "- [%s](#%s)\n", sec[i].title, sec[i].anchor));
        }
        for(int i = 0; i < h->n_m; i++){
                module *m = h->m[i];
                int n = 0;
                for(int j = 0; j < m->n_e; j++){
                        n += m->e[j]->names.n;
                }
                RUN(appendf(o, "- [%s](#%s): %d symbol%s%s%s%s\n", m->title, m->anchor, n,
                            n == 1 ? "" : "s", starts(m->name, "src/") ? " (`" : "",
                            starts(m->name, "src/") ? m->name : "",
                            starts(m->name, "src/") ? "`)" : ""));
        }
        RUN(tld_strbuf_append(o, "- [Index](#index)\n\n"));

        /* top sections */
        for(int i = 0; i < n_sec; i++){
                RUN(appendf(o, "<a id=\"%s\"></a>\n\n## %s\n\n", sec[i].anchor, sec[i].title));
                ex_where = sec[i].title;
                RUN(render_text(o, sec[i].body, text_indent(sec[i].body)));
        }

        /* modules */
        for(int i = 0; i < h->n_m; i++){
                module *m = h->m[i];
                RUN(appendf(o, "<a id=\"%s\"></a>\n\n## %s\n\n", m->anchor, m->title));
                if(starts(m->name, "src/")){
                        RUN(appendf(o, "`%s`%s", m->name, m->desc ? ": " : "\n\n"));
                }
                if(m->desc){
                        RUN(appendf(o, "%s\n\n", m->desc));
                }
                for(int j = 0; j < m->n_e; j++){
                        entry *e = m->e[j];
                        for(int k = 0; k < e->names.n; k++){
                                RUN(appendf(o, "<a id=\"%s\"></a>", e->names.s[k]));
                        }
                        RUN(tld_strbuf_append(o, "\n\n### "));
                        if(e->heading){
                                RUN(tld_strbuf_append(o, e->heading));
                        }else{
                                for(int k = 0; k < e->names.n; k++){
                                        RUN(appendf(o, "%s`%s`", k ? ", " : "", e->names.s[k]));
                                }
                        }
                        RUN(tld_strbuf_append(o, "\n\n```c\n"));
                        RUN(append_code(o, TLD_STR(e->code)));
                        RUN(tld_strbuf_append(o, "\n```\n\n"));
                        ex_where = e->names.n ? e->names.s[0] : m->title;
                        if(e->doc){
                                RUN(render_text(o, e->doc, 0));
                        }else{
                                for(int k = 0; k < e->trails.n; k++){
                                        RUN(render_text(o, e->trails.s[k], 0));
                                }
                        }
                        for(int k = 0; k < e->names.n + e->extra.n; k++){
                                idx_row *r;
                                MREALLOC(rows, sizeof(idx_row) * (size_t)(n_rows + 1));
                                r = &rows[n_rows++];
                                r->name = k < e->names.n ? e->names.s[k] : e->extra.s[k - e->names.n];
                                r->kind = k < e->names.n ? e->kinds.s[k] : "constant";
                                r->module = m->title;
                                r->mod_anchor = m->anchor;
                                r->anchor = e->names.s[k < e->names.n ? k : 0];
                        }
                }
        }

        /* index */
        RUN(tld_strbuf_append(o, "<a id=\"index\"></a>\n\n## Index\n\n"));
        RUN(tld_strbuf_append(o, "| Symbol | Kind | Module |\n|---|---|---|\n"));
        if(n_rows){
                qsort(rows, (size_t)n_rows, sizeof(idx_row), idx_cmp);
        }
        for(int i = 0; i < n_rows; i++){
                RUN(appendf(o, "| [`%s`](#%s) | %s | [%s](#%s) |\n", rows[i].name, rows[i].anchor,
                            rows[i].kind, rows[i].module, rows[i].mod_anchor));
        }
        MFREE(rows);
        MFREE(title);
        MFREE(overview);
        MFREE(preamble);
        sections_free(sec, n_sec);
        return OK;
ERROR:
        MFREE(rows);
        MFREE(title);
        MFREE(overview);
        MFREE(preamble);
        sections_free(sec, n_sec);
        return FAIL;
}

/* ------------------------------------------------------------------------ */
/* main                                                                     */
/* ------------------------------------------------------------------------ */

/* Report symbols without a doc comment and src modules without a module comment. */
static int lint(header *h, int *problems)
{
        for(int i = 0; i < h->n_m; i++){
                module *m = h->m[i];
                if(starts(m->name, "src/") && m->desc == NULL){
                        fprintf(stderr, "tlddoc: %s: module %s has no module comment\n",
                                h->file, m->name);
                        (*problems)++;
                }
                for(int j = 0; j < m->n_e; j++){
                        entry *e = m->e[j];
                        if(e->doc == NULL && e->n_trailed < e->n_decl){
                                fprintf(stderr, "tlddoc: %s: %s: no doc comment for",
                                        h->file, m->title);
                                for(int k = 0; k < e->names.n; k++){
                                        fprintf(stderr, " %s", e->names.s[k]);
                                }
                                fputc('\n', stderr);
                                (*problems)++;
                        }
                }
        }
        return OK;
}

static int same_as_file(const char *path, tld_strbuf *text, int *same)
{
        FILE *f = fopen(path, "rb");
        tld_strbuf *old = NULL;
        *same = 0;
        if(f == NULL){
                return OK;
        }
        fclose(f);
        RUN(read_file(path, &old));
        *same = old->len == text->len && memcmp(old->str, text->str, (size_t)text->len) == 0;
        tld_strbuf_free(old);
        return OK;
ERROR:
        tld_strbuf_free(old);
        return FAIL;
}

static int write_file(const char *path, tld_strbuf *text)
{
        FILE *f = fopen(path, "wb");
        if(f == NULL){
                ERROR_MSG("Cannot write %s: %s", path, strerror(errno));
        }
        if(fwrite(text->str, 1, (size_t)text->len, f) != (size_t)text->len){
                fclose(f);
                ERROR_MSG("Cannot write %s", path);
        }
        if(fclose(f) != 0){
                ERROR_MSG("Cannot write %s", path);
        }
        return OK;
ERROR:
        return FAIL;
}

static int count_lines(tld_strbuf *b)
{
        int n = 0;
        for(int i = 0; i < b->len; i++){
                n += b->str[i] == '\n';
        }
        return n;
}

static void usage(void)
{
        fprintf(stderr, "usage: tlddoc [--check] [--out DIR] [--examples DIR] ROOT\n"
                "  Writes DIR/tld.md and DIR/tld_ai.md (DIR defaults to ROOT/docs) from the\n"
                "  comments in ROOT/tld.h and ROOT/tld_ai.h. --check writes nothing and exits\n"
                "  1 if a file is out of date or a public symbol has no doc comment.\n"
                "  --examples DIR writes no manual: each code example of the doc comments goes\n"
                "  to DIR/tld-NN.c or DIR/tld_ai-NN.c, a program that compiles on its own.\n");
}

int main(int argc, char *argv[])
{
        static const char *files[2] = {"tld.h", "tld_ai.h"};
        static const char *outs[2] = {"tld.md", "tld_ai.md"};
        static const char *impl[2] = {"TLD_IMPLEMENTATION", "AI_IMPLEMENTATION"};
        const char *root = NULL;
        const char *outdir = NULL;
        int check = 0;
        int problems = 0;
        header *h = NULL;
        tld_strbuf *o = NULL;
        tld_strbuf *path = NULL;

        for(int i = 1; i < argc; i++){
                if(strcmp(argv[i], "--check") == 0){
                        check = 1;
                }else if(strcmp(argv[i], "--out") == 0 && i + 1 < argc){
                        outdir = argv[++i];
                }else if(strcmp(argv[i], "--examples") == 0 && i + 1 < argc){
                        ex_dir = argv[++i];
                }else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0){
                        usage();
                        return EXIT_SUCCESS;
                }else if(argv[i][0] == '-' || root){
                        usage();
                        return EXIT_FAILURE;
                }else{
                        root = argv[i];
                }
        }
        if(root == NULL){
                usage();
                return EXIT_FAILURE;
        }
        RUN(tld_strbuf_alloc(&path, 256));
        RUN(tld_strbuf_alloc(&o, 1 << 16));
        if(outdir == NULL){
                RUN(tld_strbuf_append(path, (char *)root));
                RUN(tld_strbuf_append(path, "/docs"));
        }else{
                RUN(tld_strbuf_append(path, (char *)outdir));
        }
        if(ex_dir && mkdir(ex_dir, 0755) != 0 && errno != EEXIST){
                ERROR_MSG("Cannot create %s: %s", ex_dir, strerror(errno));
        }
        if(!check && !ex_dir && mkdir(TLD_STR(path), 0755) != 0 && errno != EEXIST){
                ERROR_MSG("Cannot create %s: %s", TLD_STR(path), strerror(errno));
        }
        for(int f = 0; f < 2; f++){
                int same = 0;
                int len = path->len;
                RUN(parse_header(root, files[f], &h));
                RUN(tld_strbuf_clear(o));
                ex_file = files[f];
                ex_n = 0;
                if(ex_dir){
                        RUN(render(h, impl[f], o));
                        fprintf(stdout, "tlddoc: %d examples from %s in %s\n", ex_n, files[f], ex_dir);
                        header_free(h);
                        h = NULL;
                        continue;
                }
                RUN(lint(h, &problems));
                RUN(render(h, impl[f], o));
                RUN(tld_strbuf_append(path, "/"));
                RUN(tld_strbuf_append(path, (char *)outs[f]));
                if(check){
                        RUN(same_as_file(TLD_STR(path), o, &same));
                        if(!same){
                                fprintf(stderr, "tlddoc: %s is out of date; regenerate with "
                                        "cmake --build build --target docs\n", TLD_STR(path));
                                problems++;
                        }
                }else{
                        RUN(write_file(TLD_STR(path), o));
                        fprintf(stdout, "tlddoc: wrote %s (%d lines, %d symbols)\n",
                                TLD_STR(path), count_lines(o), count_symbols(h));
                }
                path->len = len;
                path->str[len] = 0;
                header_free(h);
                h = NULL;
        }
        tld_strbuf_free(o);
        tld_strbuf_free(path);
        if(check && problems == 0){
                fprintf(stdout, "tlddoc: docs are current\n");
        }
        return check && problems ? EXIT_FAILURE : EXIT_SUCCESS;
ERROR:
        header_free(h);
        tld_strbuf_free(o);
        tld_strbuf_free(path);
        return EXIT_FAILURE;
}
