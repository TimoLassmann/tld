[![CMake](https://github.com/TimoLassmann/tld/actions/workflows/cmake.yml/badge.svg)](https://github.com/TimoLassmann/tld/actions/workflows/cmake.yml)

# tld

Two single-header C libraries by Timo Lassmann.

- **`tld.h`**: allocation and error-handling macros, terminal messages, strings, a hash
  map, strict JSON, random numbers, statistics, sorting, a k-d tree, a thread pool, text
  templates and file helpers.
- **`tld_ai.h`**: local LLMs, decision models and embeddings through the Ollama server
  on the same machine, called as ordinary C functions. It builds on `tld.h`.

Both need only C11, POSIX, libm and pthreads, and run on macOS and Linux. Ideas come from
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
too. Failures return
NULL, -1 or `.ok = 0`; `ai_status()` gives the code (`AI_ENOSERVER`, `AI_ENOMODEL`, ...)
and `ai_error()` the message. With no server running, every call fails at once with
`AI_ENOSERVER`. Returned strings and vectors belong to the library and stay valid until
`ai_reset()`.

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

## Documentation

The manuals are generated from the header comments by `tools/tlddoc.c`:

- [docs/tld.md](docs/tld.md): every public function, macro and type of `tld.h`, module by
  module, with an index.
- [docs/tld_ai.md](docs/tld_ai.md): the same for `tld_ai.h`.

`CHANGELOG.md` lists what changed on the `modernisation` branch, including every renamed
function. To move a program from the old tld, run
`uv run python tools/migrate.py --write src/` on it, then build it and fix what the
migrator and the compiler report.

## Building and testing

`tld.h` is generated from the modules in `src/`; edit `src/`, never `tld.h`. After a
change, regenerate the header and the manuals:

```sh
uv run python tools/amalgamate.py          # src/ -> tld.h
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --build build --target docs          # tld.h, tld_ai.h -> docs/*.md
ctest --test-dir build -j 8
```

The test `docs-current` fails while a manual is out of date or a public symbol has no
doc comment; the comment convention is in [docs/README.md](docs/README.md). The
`tld_ai.h` tests run against `tests/ai/mock_ollama.py` and never contact a real server.
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
