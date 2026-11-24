# tests

| Folder         | ctest names                         | What                                                        |
|----------------|-------------------------------------|-------------------------------------------------------------|
| `unit/`        | `unit-<module>`, `unit-doc`         | one program per library module (`unit_<module>.c`); `unit-doc` checks `tld_doc.h`: its corpus against `docs/tld.md`, `docs/tld_ai.md` and the headers, the search on fixed questions, the answer text and its byte budget, edge cases (built from the single headers) |
| `regression/`  | `regress-<name>`                    | tests written when a bug was fixed (`regress_<name>.c`)     |
| `integration/` | `int-kde`, `int-noise`, `single-header`, `single-header-strict`, `single-header-current`, `single-header-doc`, `single-header-doc-strict` | several modules together; the single-header `./tld.h` built in two translation units, in strict C11, and checked to be current with `src/`; a program using the single headers `tld_doc.h` and `tld.h` in two translation units, run and built in strict C11; `tld-ask-run` runs `examples/tld_ask.c` once |
| `ai/`          | `ai-*`                              | `tld_ai.h` against `ai/mock_ollama.py`, driven by `ai/run.sh`; never contacts a real Ollama |
| `doc/`         | `doc-eval`, `doc-eval-unknown-name` | `tld_doc.h` on the evaluation set `doc/questions.tsv` (`doc/eval.c`): prints top 1, top 3, top 8, shown and MRR for the covered and partial questions and, for the off questions, how many answers name a word not in the manuals (`-v`: every question); fails on a miss the set does not mark as known, and (`doc-eval-unknown-name`, on `doc/unknown_name.tsv`) on an expected name that is not in the corpus |
| `oom/`         | `oom-tld`, `ai-oom`                 | every allocation of each scenario fails in turn; a leak, a crash or a failure reported as success fails the test (`oom-tld -h`, with a `tld_doc.h` scenario `doc`; `ai-oom` runs `oom-ai` through `ai/run.sh oom`) |
| `null/`        | `null-args`, `null-args-doc`, `null-args-covered` | every public function with NULL in each pointer argument, each call in a child process: the failure value and an ERROR line, or a normal return where NULL is allowed (`null-args -h`); `null-args-doc` is the same program for every `tld_doc.h` function (built with `TLD_NULL_DOC`); `covered.sh` fails when a function has no case |
| `names/`       | `names`                             | every symbol, macro and documented name of `tld.h` is `tld_`/`TLD_` (internals `tld__`/`TLD__`) or an idiom name such as `RUN`, and what `tld_doc.h` adds is `tld_doc_`/`TLD_DOC_` (internals `tld__doc_`/`TLD__DOC_`); `clash.c`, a program reusing common names after the implementation, compiles (`names/check.sh CC SRC_DIR`) |
| `misuse/`      | `misuse-rejected`                   | the compiler rejects the mistakes the headers are designed to catch (`err_*.c`, `warn_*.c`); `ok_usage.c` compiles cleanly |
| `docs/`        | `doc-examples`, `doc-examples-ai-mock`, `doc-examples-doc` | every code example in the doc comments of `tld.h`, `tld_ai.h` and `tld_doc.h` compiles and runs without failing: `tlddoc --examples` writes each as its own program, `docs/examples.sh` builds and runs them. The `tld_ai.h` examples run against `ai/mock_ollama.py`, a **mock** server with canned answers: this checks the C, not what a real model answers |
| `migrate/`     | `migrate`                           | `tools/migrate.py` turns `old.c` (code for the old tld) into `expected.c`, changes nothing on a second run, and flags what needs a hand edit |
| `fuzz/`        | none (`fuzz-json`, `fuzz-template`, `fuzz-ai`, `fuzz-http` programs) | libFuzzer targets for JSON, templates and tld_ai.h replies, built with clang and `-DTLD_FUZZ=ON`, run by `fuzz/run.sh SECONDS BUILD_DIR` from the seeds in `fuzz/seeds/` |

`test.h` is the small command-line helper every test program uses (not part of the
library). `CMakeLists.txt` builds all programs into `build/tests/`; `ai.cmake` holds the
`tld_ai.h` tests. `docs-current` and the `doc-examples*` tests are defined in
`tools/CMakeLists.txt`.

## Running

```sh
cmake -S . -B build && cmake --build build -j
ctest --test-dir build                         # everything
ctest --test-dir build -R '^unit-'             # one group (regex on the test name)
ctest --test-dir build -R regress-json --output-on-failure
```

Every program in `unit/`, `regression/` and `integration/` runs its checks when given no
arguments (that is what ctest does) and is quiet unless something fails. By hand:

```sh
build/tests/unit-sort -h        # usage: what it checks and which arguments it takes
build/tests/unit-sort -v        # the checks, with sections, inputs, results and passes
build/tests/unit-sort -vv       # also debug detail
build/tests/unit-sort -- 5 -3 9 # the module on your own input (results on stdout;
                                # -- before negative numbers)
```

`-v` sets `TLD_VERBOSE=1`, `-vv` sets 2; without either, `TLD_VERBOSE` from the
environment is used, and if that is unset the program runs with `TLD_VERBOSE=0` (only
errors). Regression tests take the names of the sub-checks to run (`-h` lists them).
The `ai/` programs take the mode arguments `run.sh` gives them; they also accept `-h`/`-v`.
