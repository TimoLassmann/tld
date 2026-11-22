# Changelog

## modernisation branch (unreleased)

Interface promise: public function names and signatures, struct layouts and macro names are
unchanged, except that the seq/ and hdf5/ modules were removed. Behaviour changes listed
here fix bugs.

### Removed
- seq/ (FASTA/FASTQ/SAM IO, alphabets, reverse complement) and hdf5/. tld now depends only on
  libc, libm and pthreads.

### Changed
- Licence: MIT (was BSD-style).

### Fixed: behaviour changes (bugs; no interface change)
- core: RUN and ASSERT are now single statements (`do { } while (0)`). A trailing `;` is required, and they can be the body of an unbraced if/else.
- core: LOG_MSG, WARNING_MSG and ERROR_MSG write to stderr instead of stdout, so stdout stays free for program output.
- core: the internal `error()` function is static. It no longer clashes with glibc's `error()` when linking statically.
- alloc: MMALLOC, MMALLOC_ALIGNED, MREALLOC and MFREE are single statements; a trailing `;` is required. Size messages use `%zu`. MFREE(NULL) no longer warns, and still sets the pointer to NULL.
- alloc: 2D galloc computes sizes in `size_t` and fails cleanly on overflow or out-of-memory. A caller variable named `i` no longer breaks the row setup, and gfree(NULL) is a no-op.
- string: `tld_strmk` no longer writes one byte past its buffer, and `len` excludes the terminating NUL. `tld_strbuf_resize` updates `alloc_len`, so appends grow geometrically.
- misc: `tld_get_full_path` hands back the buffer it allocates through `*full`, and on error never frees a buffer the caller supplied. `tld_dir_create` returns FAIL when mkdir fails. `tld_get_dirname("/file")` returns "/".
- hash: add, rename and alloc check every allocation and return code and no longer leak. `tld_hash_rename` keeps the old entry if adding the new key fails. `tld_hash_alloc` fails for sizes above the largest supported prime.
- bitfield: `tl_bitfield_hamming` returns FAIL when the mask length differs from the bitfields.
- sort, kdtree, stats: qsort comparators return 0 for equal elements.
- shuffle: shuffles are uniform (`j = i + tl_random_int(rng, n - i)`); seeded output changes.
- logsum: `nearly_equal_float`/`nearly_equal_double` compare relative to |a|+|b|; they used to behave like ==. The lookup table is set up thread-safely, and on first use if `init_logsum()` was never called. NaN input returns NaN.
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
- 2D galloc sizes its data block with `sizeof(T*)` rather than `sizeof(T)`. That is harmless on 64-bit, but too small for 8-byte element types on 32-bit platforms. Fixing it needs a new helper.
- json: `tld_json_parse` returns FAIL on malformed, truncated or too-deep (>512) input, on trailing characters and on non-JSON literals (`hello`, `True`, `NaN`, `01`). It used to return OK with a NULL or partial tree.
- json: quoted values are always strings ("123", "true", "{" no longer change type). String escapes, including `\uXXXX` and surrogate pairs, are decoded to UTF-8, and `\u0000` is rejected. Unquoted `null` parses to TLD_JSON_UNDEF and is written as `null`.
- json: integers outside the int32 range are stored as TLD_JSON_DBL, for example Ollama durations in nanoseconds. Integers beyond 2^53 lose precision.
- json: the writer escapes strings and keys and writes doubles so they read back exactly; inf and nan are written as `null`.
- json: the type guessing in `JS` only matches whole numbers and the exact words true/false (in lower, Capital or UPPER case). "", "truex" and "2024-11" stay strings.
- json: `tld_json_search` finds all matches; it used to overflow with ≥5 matches and skip later siblings. `tld_json_val_copy` and `tld_json_set_val` release the target's old contents, so changing a value's type is safe.
- json: JSON_OBJ/JSON_ARR (`json_*_add_multiple`) make room before writing an entry and stop at the first failure. A container created by the call is then freed, and NULL is returned. The containers' realloc counts only slots that were really allocated, and always grows (it was stuck at size 1).
- template: a false IF no longer renders an enclosing IF's ELSE. IF nesting deeper than 512 fails, and parse errors no longer leak.

### Added
- tld_ai.h: single header (needs tld.h) that calls the LLMs, decision models and embedding models of the local Ollama server as C functions: `ai_text`, `ai_yesno`, `ai_choose`/`AI_CHOICE`, `ai_rate`/`AI_SCALE`, `ai_fill`/`AI_STRUCT`, `ai_embed*`, `ai_nearest`, `ai_parallel`, `ai_review`. Public API as in aic-project's tld_ai.h, plus `ai_status()` with `AI_E*` codes, the `max_retries` option, and `ai_defaults()`/`AI_CONFIG` for defaults set in code (per-call options > environment > ai_defaults > built-in; each call replaces the previous one, NULL resets).
- tld_ai.h: no libcurl, SQLite or cJSON. Own HTTP/1.1 client over sockets (Content-Length, chunked, read-to-close, TLD_AI_TIMEOUT, SIGPIPE-safe; https gives AI_EHTTP), JSON through tld_json, an append-only JSONL cache (`TLD_AI_DB`, flock-shared between processes) and one JSON file per review (`TLD_AI_REVIEWS`).
- tld_ai.h: the cache is off unless a file is named with `ai_defaults(AI_CONFIG(.db = "..."))` or `TLD_AI_DB` (which wins), so a plain run writes no files. Environment variables are named `TLD_AI_*` (`TLD_AI_HOST`, `TLD_AI_LLM`, `TLD_AI_DECIDER`, `TLD_AI_EMBEDDER`, `TLD_AI_DB`, `TLD_AI_REVIEWS`, `TLD_AI_NUM_CTX`, `TLD_AI_TIMEOUT`, `TLD_AI_FRESH`).
- tld_ai.h: server probe (`AI_ENOSERVER`, re-probed at most every 5 s). No model guard: on a machine that holds one model, start Ollama with `OLLAMA_MAX_LOADED_MODELS=1`.
- tld_ai.h: `TLD_AI_FRESH` takes 1/0, true/false, yes/no, on/off in any case; `TLD_AI_NUM_CTX` and `TLD_AI_TIMEOUT` must be whole positive integers; other values are ignored with a warning. Calls made after the exit cleanup fail with `AI_EARG` instead of crashing. Each resolved address gets its share of the connect timeout. `extra_options` keys replace built-in option keys of the same name.
- tld_ai.h: defaults `inkling-2-mini:latest`, `autotrust-jev:latest`, `embeddinggemma-2:latest`. Fixed relative to aic-project: a failed call no longer shifts replay occurrence numbers, a failed /api/tags is not cached as digest "unknown", every allocation is checked, `ai_fill` leaves the struct zeroed when every attempt fails, decision calls fill `ai_last()` tokens, `ai_yesno` rejects values outside [0,1], review and cache keys separate their parts. Existing SQLite caches (`ai.db`) are not read.
- tld_ai.h tests: `tests/ai.cmake` (mock server, compile checks, ASan/UBSan in ASAN builds, TSan otherwise), examples in `examples/`.
- json: typed decorations `JSTR`, `JNUM`, `JINT`, `JBOOL` and `JNULL`, plus path getters `tld_json_get_{str,num,int,bool,obj,arr}` (e.g. `"message.content"`, `"embeddings[0][5]"`).
- Single-header build: `./tld.h`, generated by `tools/amalgamate.py`. Put `#define TLD_IMPLEMENTATION` before the include in one .c file.
