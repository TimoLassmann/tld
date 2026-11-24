[![Build and test](https://github.com/TimoLassmann/tld/actions/workflows/build.yml/badge.svg)](https://github.com/TimoLassmann/tld/actions/workflows/build.yml)
[![Sanitizers and fuzzing](https://github.com/TimoLassmann/tld/actions/workflows/checks.yml/badge.svg)](https://github.com/TimoLassmann/tld/actions/workflows/checks.yml)

# tld

Two single-header C libraries by Timo Lassmann, and a third that answers questions about
them.

- **`tld.h`**: allocation and error-handling macros, terminal messages, strings, a hash
  map, strict JSON, random numbers, statistics, sorting, a k-d tree, approximate string
  search (bit-parallel, AVX2/NEON), a thread pool, text templates and file helpers.
- **`tld_ai.h`**: local LLMs, decision models and embeddings through the Ollama server
  on the same machine, called as ordinary C functions. It builds on `tld.h`.
- **`tld_doc.h`**: live documentation. A program asks a question in plain language and
  gets back the manual entries of `tld.h` and `tld_ai.h` whose words match it, ranked and
  copied word for word, with the question's words that the manuals do not contain. A
  deterministic lookup with no model. It builds on `tld.h`.

All three need only C11, POSIX, libm and pthreads, and run on macOS and Linux. Ideas come from
[easel](https://github.com/EddyRivasLab/easel) (Eddy/Rivas laboratory),
[klib](https://github.com/attractivechaos/klib) and
[rayfork](https://github.com/SasLuca/rayfork).

## Using tld.h

Copy `tld.h` into your project. In exactly one `.c` file, before any other `#include`:

```c
#define TLD_IMPLEMENTATION
#include "tld.h"
```

Every other file uses a plain `#include "tld.h"`. Build with
`cc -std=c11 prog.c -lm -lpthread`.

A function that can fail returns `OK` or `FAIL` and passes its results through pointer
arguments. `RUN`, `ASSERT`, `ERROR_MSG` and the allocation macros print an ERROR line
(time, message, file and line) and jump to a label named `ERROR`, where the function
frees what it holds. Initialise every pointer to NULL; every free function accepts NULL.

```c
#define TLD_IMPLEMENTATION
#include "tld.h"

/* Count the lines of a file and print the count as JSON. */
static int report(const char *path)
{
        tld_strbuf *text = NULL;
        tld_strbuf *out = NULL;
        tld_json_obj *j = NULL;
        int lines = 0;

        ASSERT(path != NULL, "No path");
        RUN(tld_path_read(path, &text));
        for(int i = 0; i < TLD_STRLEN(text); i++){
                lines += TLD_STR(text)[i] == '\n';
        }
        j = TLD_JSON_OBJECT(TLD_JSTR("file", path), TLD_JINT("lines", lines));
        ASSERT(j != NULL, "Could not build the report");
        RUN(tld_strbuf_alloc(&out, 64));
        RUN(tld_json_dump(j, out, 0));
        printf("%s\n", TLD_STR(out));
        tld_json_obj_free(j);
        tld_strbuf_free(out);
        tld_strbuf_free(text);
        return OK;
ERROR:
        tld_json_obj_free(j);
        tld_strbuf_free(out);
        tld_strbuf_free(text);
        return FAIL;
}

int main(int argc, char *argv[])
{
        RUN(report(argc > 1 ? argv[1] : NULL));
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}
```

`galloc` allocates a zeroed array or matrix of any arithmetic type, and `gfree` frees
it:

```c
double *v = NULL;
uint32_t **m = NULL;
galloc(&v, 100);        /* 100 doubles */
galloc(&m, 10, 20);     /* a 10 x 20 matrix: m[i][j] */
...
gfree(v);
gfree(m);
```

Like the other allocation macros, `galloc` jumps to `ERROR` on failure. It is meant
for a pointer that is still NULL.

### Terminal output

Messages go to stderr, so stdout holds only a program's results. Colour and symbols
appear only when stderr is a terminal and `NO_COLOR` is unset. `TLD_VERBOSE` sets how
much is shown: `0` errors only, `1` normal (the default), `2` also `DEBUG_MSG`.

```c
SECTION_MSG("Aligning %d sequences", n);   /* ── Aligning 40 sequences ────── */
KV_MSG("model", "%s", name);               /*    model     inkling-2-mini     */
OK_MSG("done in %.1f s", t);               /*  ✓ done in 1.9 s                */
DEBUG_MSG("seed %u", seed);                /*  · seed 42   (TLD_VERBOSE=2)    */
for (long i = 0; i < n; i++) { work(i); tld_progress(i + 1, n, "aligning"); }
LOG_MSG(...); WARNING_MSG(...); ERROR_MSG(...);   /* timestamped lines         */
```

The formats are printf formats checked by the compiler; print text from users or files
with `"%s"`: `LOG_MSG("%s", line)`, not `LOG_MSG(line)`.

## Using tld_ai.h

`tld_ai.h` talks to an [Ollama](https://ollama.com) server: install Ollama and start it
(`ollama serve`). Out of the box it uses three models from the Ollama model library,
pulled once:

```sh
ollama pull inkling-2-mini       # text: ai_text, ai_fill
ollama pull autotrust-jev        # decisions: ai_yesno, ai_choose, ai_rate
ollama pull embeddinggemma-2     # embeddings: ai_embed, ai_embed_all
```

These are defaults, not requirements. Any text or embedding model your server has can
take their place: name it with `TLD_AI_LLM` or `TLD_AI_EMBEDDER`, with `ai_defaults()`,
or per call with `.model` (see [Settings](#settings)). Decisions go to the server's
`/v1/systemone` endpoint, so `TLD_AI_DECIDER` must name a decision model that answers it,
such as `autotrust-jev`.

```c
#define TLD_IMPLEMENTATION          /* in one .c file, before any other #include */
#define AI_IMPLEMENTATION
#include "tld_ai.h"

#define TEAM(X) X(bug, "A software defect") X(billing, "Payments and refunds")
AI_CHOICE(team, "Which team should handle this ticket?", TEAM)
#define NAME(label, description) #label,
static const char *team_name[] = {TEAM(NAME)};      /* "bug", "billing" */

int main(void) {
    struct team_answer t = ai_choose(team, "The app crashes on start");
    const char *reply = ai_text("Write a one-line apology for: %s", "a crash");
    if (!t.ok || !reply) { fprintf(stderr, "ai: %s\n", ai_error()); return 1; }
    printf("team %s (%.2f): %s\n", team_name[t.value], t.confidence, reply);
    return 0;
}
```

Build with `cc -std=c11 -I. prog.c -lpthread -lm`. Larger examples:
`examples/triage.c` (choice, scale, yes/no, struct filling, human review) and
`examples/par.c` (parallel calls, embeddings, nearest neighbours). The build makes
them as `build/tests/ai-hello`, `ai-triage` and `ai-par`; each takes `-h`, shows every
step (answer, confidence, model, time, tokens) on stderr, and with `-vv` each request
too.

A program that learns its records and labels at run time (a workflow file, say) builds
an `ai_type` or `ai_labels` itself and calls `ai_fill_dyn` (the answer comes back as
typed JSON text), `ai_choose_dyn` or `ai_rate_dyn`; they send the same requests as the
macro forms.

Failures return NULL, -1 or `.ok = 0`; `ai_status()` gives the code (`AI_ENOSERVER`,
`AI_ENOMODEL`, ...) and `ai_error()` the message. With no server running, every call
fails at once with `AI_ENOSERVER`. Returned strings and vectors belong to the library
and stay valid until `ai_reset()`.

### Settings

Each setting is decided on its own, highest first: per-call options (`AI_OPTS(...)`),
environment variable, `ai_defaults()`, built-in. The environment is read once, at the
first call; a value that does not parse is ignored with a warning (`TLD_AI_FRESH` takes
1/0, true/false, yes/no, on/off; `TLD_AI_NUM_CTX` and `TLD_AI_TIMEOUT` take positive
integers). Each `ai_defaults()` call replaces the previous one; `ai_defaults(NULL)`
resets to the built-ins.

| Variable          | Built-in default          | Meaning                                        |
|-------------------|---------------------------|------------------------------------------------|
| `TLD_AI_HOST`     | `http://localhost:11434`  | Ollama server (plain http only)                |
| `TLD_AI_LLM`      | `inkling-2-mini:latest`   | text model: `ai_text`, `ai_fill`               |
| `TLD_AI_DECIDER`  | `autotrust-jev:latest`    | decision model: `ai_yesno`, `ai_choose`, `ai_rate` |
| `TLD_AI_EMBEDDER` | `embeddinggemma-2:latest` | embedding model: `ai_embed`, `ai_embed_all`    |
| `TLD_AI_NUM_CTX`  | not sent                  | context length for LLM and embedding calls     |
| `TLD_AI_TIMEOUT`  | `300`                     | seconds per request                            |
| `TLD_AI_FRESH`    | `0`                       | `1` = ignore the cache                         |
| `TLD_AI_DB`       | no cache                  | cache file (JSON lines); `off` = no cache      |
| `TLD_AI_REVIEWS`  | `ai-reviews`              | directory of review files (`ai_review`)        |

With a cache file set (`ai_defaults(AI_CONFIG(.db = "ai.cache"))` or `TLD_AI_DB`), every
answered call is appended to it, so a rerun replays finished calls in order and costs
nothing. A review is a file `TLD_AI_REVIEWS/<key>.json`: set its `"status"` to `"done"`,
fill in `"answer"` and rerun. Vectors from different embedding models are not comparable;
store the embedder's name next to any vectors you save.

On a machine that can hold only one model, start Ollama with
`OLLAMA_MAX_LOADED_MODELS=1`; the server then swaps models safely between requests.

Cloud models (those whose `/api/tags` entry has `remote_host`) are reached through the
local server and are opt-in only: the defaults are local, so a prompt leaves the machine
only when a cloud model is named in `TLD_AI_LLM`, `TLD_AI_DECIDER`, `TLD_AI_EMBEDDER`,
`ai_defaults()` or `.model`. A server that is not signed in answers with an HTTP error,
reported as `AI_EHTTP`.

## Using tld_doc.h

`tld_doc.h` holds a copy of every entry of the `tld.h` and `tld_ai.h` manuals (the corpus).
`tld_doc_ask` finds the entries whose words match a question and returns them as text
written for a coding agent. It does not write code or advice of its own, and it does not
need `tld_ai.h` or a server.

```c
#define TLD_IMPLEMENTATION          /* in one .c file, before any other #include */
#define TLD_DOC_IMPLEMENTATION
#include "tld_doc.h"

int main(void) {
    tld_doc_answer *a = NULL;
    if (tld_doc_ask("how do I read a JSON string?", NULL, &a) != OK) return 1;
    fputs(a->text, stdout);
    tld_doc_answer_free(a);
    return 0;
}
```

The search is a word lookup over the corpus. The question's words (common words dropped,
endings such as -ing taken off) are matched where words of the manuals start, and the
entries are ranked by BM25, a match in a name counting three times. An identifier
(`tld_json_get_str`, or `tld_jsn_get_str` with a typo) puts its entry first, and a long
word found nowhere is searched again allowing an edit or two. The same question always
gives the same answer. The text starts with the facts of the search: which words of
the question occur in the manuals and which do not (`Not found: weather, paris`), so
the reader can judge whether the entries are relevant. It stays within a byte budget
(`.max_bytes`, 4096 by default), and declarations are never cut.

`examples/tld_ask.c` (built as `build/tests/tld-ask`) is a command-line front end:
`tld-ask "how do I read a JSON string?"`, and `tld-ask -b 1500 -n 3 "..."` for the first
3 entries within 1500 bytes. The corpus is generated into `tld_doc.h` by the same command
that writes the manuals, and the test `docs-current` fails while it is out of date. The
design is in [docs/design/tld_doc-prd.md](docs/design/tld_doc-prd.md); `doc-eval` prints
the retrieval figures on the questions of `tests/doc/questions.tsv`.

## Documentation

The manuals are generated from the header comments by `tools/tlddoc.c`:

- [docs/tld.md](docs/tld.md): every public function, macro and type of `tld.h`, module by
  module, with an index.
- [docs/tld_ai.md](docs/tld_ai.md): the same for `tld_ai.h`.
- [docs/tld_doc.md](docs/tld_doc.md): the same for `tld_doc.h`.

`CHANGELOG.md` lists what changed from the old tld, including every renamed function. To move a program from the old tld, run
`uv run python tools/migrate.py --write src/` on it, then build it and fix what the
migrator and the compiler report.

## Building and testing

`tld.h` is generated from the modules in `src/`; edit `src/`, never `tld.h`. After a
change, regenerate the header and the manuals:

```sh
uv run python tools/amalgamate.py          # src/ -> tld.h
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --build build --target docs          # headers -> docs/*.md, corpus in tld_doc.h
ctest --test-dir build -j 8
```

The test `docs-current` fails while a manual or the corpus in `tld_doc.h` is out of date
or a public symbol has no doc comment; the comment convention is in
[docs/README.md](docs/README.md). The `tld_ai.h` tests run against
`tests/ai/mock_ollama.py` and never contact a real server.
`tests/README.md` describes every test.

A run against the real models is manual, with no other model loaded (`ollama ps` shows
nothing):

```sh
ollama ps                                   # must list no model
build/tests/ai-hello                        # one call to the default LLM
TLD_AI_DB=build/live.cache build/tests/ai-triage -vv tests/ai/ticket.txt
TLD_AI_DB=build/live.cache build/tests/ai-par 4
ollama ps                                   # models unload after keep_alive (5 min)
```

## Licence

MIT; see `COPYING`.
