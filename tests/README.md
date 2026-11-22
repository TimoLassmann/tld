# tests

| Folder         | ctest names                         | What                                                        |
|----------------|-------------------------------------|-------------------------------------------------------------|
| `unit/`        | `unit-<module>`                     | one program per library module (`unit_<module>.c`)          |
| `regression/`  | `regress-<name>`                    | tests written when a bug was fixed (`regress_<name>.c`)     |
| `integration/` | `int-kde`, `int-noise`, `single-header`, `single-header-strict`, `single-header-current` | several modules together; the single-header `./tld.h` built in two translation units, in strict C11, and checked to be current with `src/` |
| `ai/`          | `ai-*`                              | `tld_ai.h` against `ai/mock_ollama.py`, driven by `ai/run.sh`; never contacts a real Ollama |
| `oom/`         | `oom-tld`, `ai-oom`                 | every allocation of each scenario fails in turn; a leak, a crash or a failure reported as success fails the test (`oom-tld -h`; `ai-oom` runs `oom-ai` through `ai/run.sh oom`) |

`test.h` is the small command-line helper every test program uses (not part of the
library). `CMakeLists.txt` builds all programs into `build/tests/`; `ai.cmake` holds the
`tld_ai.h` tests.

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
build/tests/unit-sort 5 3 9 -1  # the module on your own input (results on stdout)
```

`-v` sets `TLD_VERBOSE=1`, `-vv` sets 2; without either, `TLD_VERBOSE` from the
environment is used, and if that is unset the program runs with `TLD_VERBOSE=0` (only
errors). Regression tests take the names of the sub-checks to run (`-h` lists them).
The `ai/` programs take the mode arguments `run.sh` gives them; they also accept `-h`/`-v`.
