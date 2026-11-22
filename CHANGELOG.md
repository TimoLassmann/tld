# Changelog

## modernisation branch (unreleased)

Interface promise: public function names and struct layouts are unchanged, except that the
seq/ and hdf5/ modules were removed and the small signature changes under "Safety" below,
made so the compiler catches mistakes that used to corrupt memory silently. Behaviour
changes listed here fix bugs.

### Safety: interface changes that turn silent memory errors into compile errors
- core: every message macro (LOG_MSG, WARNING_MSG, ERROR_MSG, ASSERT, SECTION_MSG, KV_MSG,
  OK_MSG, DEBUG_MSG) requires a string-literal format, and the format is checked against the
  arguments (`format(printf)` on tld_message and friends). `LOG_MSG(text)` is a compile error;
  write `LOG_MSG("%s", text)`. A `%` in text used as a format reads or writes random memory.
- core: MACRO_MIN and MACRO_MAX evaluate each argument once (statement expressions, gcc and
  clang); `MACRO_MAX(i++, n)` used to increment twice. They no longer work in constant
  expressions (array sizes, case labels, static initialisers).
- alloc: `gfree(p)` sets p to NULL, so a second gfree is harmless instead of a double free.
  p must be the pointer variable itself: `gfree(&p)` is now a compile error (it freed the
  address of a local).
- hash: `tld_hash_get` and `tld_hash_iterator_next` hand out `const char *` (they point into
  the table); `tld_hash_add`, `_del` and `_rename` take `const char *`. Declare
  `const char *v` at call sites.
- string: `tld_append` and `tld_prepend` take `const char *`.
- tpool: `tld_thread_pool_wait` returns int: OK when every job ran, FAIL when a time limit or
  `tld_global_stop_run` refused or abandoned jobs (it returned void, and jobs vanished
  silently). `tld_thread_pool_add` returns FAIL after a stop; the data is still the caller's.
- tld_ai.h: `ai_text`, `ai_text_with` and `ai_fmt` return `const char *`, and string fields of
  AI_STRUCT types are `const char *`, so passing them to free() warns. Prompts and `ai_fmt`
  require a literal, checked printf format, like the message macros (`(ai_text)(fmt, ...)`
  calls the function directly).
- tld_ai.h: 100000 results kept without `ai_reset()` print one warning to stderr (memory grows
  with every call until ai_reset).

### Fixed: memory errors found by the safety audit
- string: a buffer growing past about 1 GB overflowed its int size, returned OK and wrote
  past the end of the block. Sizes are computed in size_t; a buffer holds at most
  INT32_MAX - 1 bytes and an append past that fails, leaving the buffer unchanged.
- tld_ai.h: the chunked-reply decoder could wrap a size check on 32-bit systems and read past
  the reply.

### Tests
- `misuse-rejected`: the compiler rejects text as a format, gfree(&p), free() on tld_ai.h
  results, writable pointers into a hash and mismatched format arguments.
- `regress-str limit`, `regress-alloc minmax`, `ai-arena`, and new tpool checks for refused
  and abandoned jobs.
- tests/fuzz: libFuzzer targets for JSON, templates and tld_ai.h replies (decoding and the
  whole HTTP exchange), with seeds; `-DTLD_FUZZ=ON`, run with `tests/fuzz/run.sh`.
- CI: MemorySanitizer, ThreadSanitizer and 60 s per fuzz target on every push.
- unit-alloc allocates 64 MB instead of 4.7 GB, so it runs under the sanitizers;
  tests/ai/compile.sh uses python3 instead of perl.

### Removed
- core: `H1_MSG`, `H2_MSG`, `H3_MSG` and `tld_heading` (unused; they wrote colour codes to stdout even when it was not a terminal).
- seq/ (FASTA/FASTQ/SAM IO, alphabets, reverse complement) and hdf5/. tld now depends only on
  libc, libm and pthreads.

### Changed
- Licence: MIT (was BSD-style).

### Fixed: behaviour changes (bugs; no interface change)
- Duplicated code removed: one prime-size table in hash (growing past the largest prime now refuses instead of overflowing int), one parse step shared by the two template functions, the dead `error()`/`verror()` in core; in tld_ai.h `strcasecmp`/`strncasecmp` replace two hand-written compares, and image and review files are read by one reader (a review file containing a NUL byte is now an error instead of being cut short).
- json: `tld_json_set_val(obj, path, v)` takes a path in the getters' syntax (`"a.b[2].c"`, `"[0]"`) and sets exactly that value; a missing last key is added and index n appends. It used to replace same-named keys anywhere in the tree, skipping the rest of a level after the first match, and silently did nothing for a missing key.
- json: `tld_json_obj_alloc` and `tld_json_arr_alloc` check every allocation and clean up on failure.
- alloc: 2D `galloc` sizes the data block by the element type (it used the pointer size: 8x too much memory for char matrices, 2x for int/float, too little for 8-byte types on 32-bit).
- kdtree: `kdtree_debug_print` and `kdtree_debug_chk` accept NULL and print to stderr.
- Internal helpers no longer pollute the caller's namespace: `galloc_hlp`, `get_ptr` and `galloc_too_few` (needed by the galloc macro) are now `tld__galloc`, `tld__galloc_data` and `tld__galloc_too_few`; `auc_pt_sort` is static; the unused `print_arr` is removed.
- core: `ERROR_MSG`, `RUN` and `ASSERT` report the caller's file and line (they reported a fixed line inside tld-core.c); `tld_message_at()` added for this.
- core: RUN and ASSERT are now single statements (`do { } while (0)`). A trailing `;` is required, and they can be the body of an unbraced if/else.
- core: LOG_MSG, WARNING_MSG and ERROR_MSG write to stderr instead of stdout, so stdout stays free for program output.
- core: the internal `error()` function is static. It no longer clashes with glibc's `error()` when linking statically.
- alloc: MMALLOC, MMALLOC_ALIGNED, MREALLOC and MFREE are single statements; a trailing `;` is required. Size messages use `%zu`. MFREE(NULL) no longer warns, and still sets the pointer to NULL.
- alloc: 2D galloc computes sizes in `size_t` and fails cleanly on overflow or out-of-memory. A caller variable named `i` no longer breaks the row setup, and gfree(NULL) is a no-op.
- string: `tld_strmk` no longer writes one byte past its buffer, and `len` excludes the terminating NUL. `tld_strbuf_resize` updates `alloc_len`, so appends grow geometrically.
- string: `tld_append(b, TLD_STR(b))` and `tld_prepend(b, TLD_STR(b))` (text from the buffer itself, or a suffix of it) work; when the buffer had to grow they read freed memory. `tld_strbuf_copy(b, b)` is a no-op.
- hash: `tld_hash_add` with a value just read by `tld_hash_get` for the same key keeps it; the value used to be cleared before it was copied and became "".
- misc: `tld_get_full_path` hands back the buffer it allocates through `*full`, and on error never frees a buffer the caller supplied. `tld_dir_create` returns FAIL when mkdir fails. `tld_get_dirname("/file")` returns "/".
- hash: add, rename and alloc check every allocation and return code and no longer leak. `tld_hash_rename` keeps the old entry if adding the new key fails. `tld_hash_alloc` fails for sizes above the largest supported prime.
- bitfield: `tl_bitfield_hamming` returns FAIL when the mask length differs from the bitfields.
- sort, kdtree, stats: qsort comparators return 0 for equal elements.
- shuffle: shuffles are uniform (`j = i + tl_random_int(rng, n - i)`); seeded output changes.
- logsum: `nearly_equal_float`/`nearly_equal_double` compare relative to |a|+|b|; they used to behave like ==. The lookup table is set up thread-safely, and on first use if `init_logsum()` was never called. NaN input returns NaN.
- rng: seed 0 (`init_rng(&r, 0)`, `tl_random_sdist_init(..., 0)`) takes 64 bits from /dev/urandom, mixed with the time in nanoseconds, the process id and a counter. It was made from the time in seconds and `clock()` only, in 32 bits (the process id was never mixed in), so generators made in one loop, or jobs started in the same second, often got the same stream.
- rng: `tl_random_double` returns 53-bit values in the open interval (0,1). `tl_random_int` is unbiased (rejection sampling) and returns 0 for a <= 1. Seeded sequences change.
- rng: `tl_random_sdist_init` no longer prints its table, rejects n <= 0 and NaN or negative weights, and frees on error. `init_rng_from_rng` no longer copies the parent's cached gaussian.
- tpool: `tld_thread_pool_wait` waits until the queue is empty and no job is running; it could return with jobs still queued. `tld_thread_pool_free` runs every queued job before stopping; it used to drop them.
- tpool: reaching maxtime or setting `tld_global_stop_run` stops every worker, and wait/free no longer hang.
- tpool: the library no longer installs SIGINT/SIGABRT handlers, and pool creation no longer resets `tld_global_stop_run`. Programs set the flag from their own handler. Nothing is logged to stdout.
- kdtree: `kdtree_insert` no longer prints to stdout. Internal helpers are static, and a failed yard realloc returns FAIL instead of using freed memory.
- stats: `tld_auc`, `tld_auc_calculate` and `tld_auc_best_threshold` return FAIL for single-class labels or NaN scores; they used to return NaN with OK. The best threshold is no longer clamped to [0,1], so logit scores work, and it equals the threshold `tld_auc` uses.
- stats: KDE uses a two-pass variance and a non-zero fallback bandwidth for constant data; constant data used to give an all-zero density. It returns FAIL for `n_steps <= 0`, non-finite data, negative counts or a zero total count.
- stats: `tld_sample_variance`, `tld_sample_covariance` and `tld_standardise` return FAIL for n < 2, and `tld_mean` for n < 1. `tld_normal_pdf` returns FAIL for sigma <= 0. `tld_shannon` returns FAIL for negative or NaN values and 0 for an all-zero vector.
- stats: quantile normalisation gives tied values the mean of the reference quantiles over their positions; NaN input returns FAIL.
- single header: the file with `TLD_IMPLEMENTATION` now asks for POSIX.1-2008 itself (`_POSIX_C_SOURCE`, `_DEFAULT_SOURCE`, `_DARWIN_C_SOURCE`, at the top of tld.h). A strict `-std=c11` build on glibc used `realpath`, `localtime_r` and `strnlen` undeclared (an implicit `realpath` truncates its pointer result on 64-bit). New test: `single-header-strict`.

### Known limitation
- json: `tld_json_parse` returns FAIL on malformed, truncated or too-deep (>512) input, on trailing characters and on non-JSON literals (`hello`, `True`, `NaN`, `01`). It used to return OK with a NULL or partial tree.
- json: quoted values are always strings ("123", "true", "{" no longer change type). String escapes, including `\uXXXX` and surrogate pairs, are decoded to UTF-8, and `\u0000` is rejected. Unquoted `null` parses to TLD_JSON_UNDEF and is written as `null`.
- json: integers outside the int32 range are stored as TLD_JSON_DBL, for example Ollama durations in nanoseconds. Integers beyond 2^53 lose precision.
- json: the writer escapes strings and keys and writes doubles so they read back exactly; inf and nan are written as `null`.
- json: the type guessing in `JS` only matches whole numbers and the exact words true/false (in lower, Capital or UPPER case). "", "truex" and "2024-11" stay strings.
- json: `tld_json_search` finds all matches; it used to overflow with ≥5 matches and skip later siblings. `tld_json_val_copy` and `tld_json_set_val` release the target's old contents, so changing a value's type is safe.
- json: JSON_OBJ/JSON_ARR (`json_*_add_multiple`) make room before writing an entry and stop at the first failure. A container created by the call is then freed, and NULL is returned. The containers' realloc counts only slots that were really allocated, and always grows (it was stuck at size 1).
- template: a false IF no longer renders an enclosing IF's ELSE. IF nesting deeper than 512 fails, and parse errors no longer leak.

### Added
- docs: `docs/tld.md` and `docs/tld_ai.md`, generated from the header comments by `tools/tlddoc.c` (`cmake --build build --target docs`). The `docs-current` test fails when the manual is stale or a public symbol has no comment. Every public symbol now has a doc comment (comments only; the compiled code is unchanged).
- CI: GitHub Actions builds with `-Wall -Wextra -Werror` and runs every test on Ubuntu x86-64 (gcc, clang), Ubuntu arm64 (gcc), macOS (clang) and Alpine (musl), plus an ASan+UBSan job (with leak checking).
- core: rich terminal output on stderr: `SECTION_MSG`, `KV_MSG`, `OK_MSG`, `DEBUG_MSG` and `tld_progress()`, plus `tld_verbosity()`/`tld_term_color()`. Colour only when stderr is a terminal and `NO_COLOR` is unset; `TLD_VERBOSE` = 0 (errors only), 1 (default) or 2 (debug). WARNING/ERROR tags are coloured on a terminal.
- tld_ai.h: `ai_last()` also reports `model` and `host`; at `TLD_VERBOSE=2` every request and its outcome is logged. `examples/hello.c` shows prompt, host, model, time and tokens on stderr and only the answer on stdout.
- tld_ai.h: single header (needs tld.h) that calls the LLMs, decision models and embedding models of the local Ollama server as C functions: `ai_text`, `ai_yesno`, `ai_choose`/`AI_CHOICE`, `ai_rate`/`AI_SCALE`, `ai_fill`/`AI_STRUCT`, `ai_embed*`, `ai_nearest`, `ai_parallel`, `ai_review`. Public API as in aic-project's tld_ai.h, plus `ai_status()` with `AI_E*` codes, the `max_retries` option, and `ai_defaults()`/`AI_CONFIG` for defaults set in code (per-call options > environment > ai_defaults > built-in; each call replaces the previous one, NULL resets).
- tld_ai.h: no libcurl, SQLite or cJSON. Own HTTP/1.1 client over sockets (Content-Length, chunked, read-to-close, TLD_AI_TIMEOUT, SIGPIPE-safe; https gives AI_EHTTP), JSON through tld_json, an append-only JSONL cache (`TLD_AI_DB`, flock-shared between processes) and one JSON file per review (`TLD_AI_REVIEWS`).
- tld_ai.h: the cache is off unless a file is named with `ai_defaults(AI_CONFIG(.db = "..."))` or `TLD_AI_DB` (which wins), so a plain run writes no files. Environment variables are named `TLD_AI_*` (`TLD_AI_HOST`, `TLD_AI_LLM`, `TLD_AI_DECIDER`, `TLD_AI_EMBEDDER`, `TLD_AI_DB`, `TLD_AI_REVIEWS`, `TLD_AI_NUM_CTX`, `TLD_AI_TIMEOUT`, `TLD_AI_FRESH`).
- tld_ai.h: server probe (`AI_ENOSERVER`, re-probed at most every 5 s). No model guard: on a machine that holds one model, start Ollama with `OLLAMA_MAX_LOADED_MODELS=1`.
- tld_ai.h: `TLD_AI_FRESH` takes 1/0, true/false, yes/no, on/off in any case; `TLD_AI_NUM_CTX` and `TLD_AI_TIMEOUT` must be whole positive integers; other values are ignored with a warning. Calls made after the exit cleanup fail with `AI_EARG` instead of crashing. Each resolved address gets its share of the connect timeout. `extra_options` keys replace built-in option keys of the same name.
- tld_ai.h: defaults `inkling-2-mini:latest`, `autotrust-jev:latest`, `embeddinggemma-2:latest`. Fixed relative to aic-project: a failed call no longer shifts replay occurrence numbers, a failed /api/tags is not cached as digest "unknown", every allocation is checked, `ai_fill` leaves the struct zeroed when every attempt fails, decision calls fill `ai_last()` tokens, `ai_yesno` rejects values outside [0,1], review and cache keys separate their parts. Existing SQLite caches (`ai.db`) are not read.
- tld_ai.h tests: `tests/ai.cmake` (mock server, compile checks, ASan/UBSan in ASAN builds, TSan otherwise), examples in `examples/`.
- json: typed decorations `JSTR`, `JNUM`, `JINT`, `JBOOL` and `JNULL`, plus path getters `tld_json_get_{str,num,int,bool,obj,arr}` (e.g. `"message.content"`, `"embeddings[0][5]"`).
- Single-header build: `./tld.h`, generated by `tools/amalgamate.py`. Put `#define TLD_IMPLEMENTATION` before the include in one .c file.
