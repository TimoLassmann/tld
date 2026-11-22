# Changelog

## modernisation branch (unreleased)

Interface promise: public function names and struct layouts are unchanged, except that the
seq/ and hdf5/ modules were removed and the small signature changes under "Safety" below,
made so the compiler catches mistakes that used to corrupt memory silently. Behaviour
changes listed here fix bugs.

### Renamed: one naming scheme
Public functions, types and variables start with `tld_` and the module word, macros and
constants with `TLD_`; the error-handling idiom keeps its short names (`OK`, `FAIL`, `RUN`, `ASSERT`,
`*_MSG`, `MMALLOC`, `MREALLOC`, `MFREE`, `galloc`, `gfree`). The `names` test holds every name of
tld.h to this. Old → new:
- core: `MACRO_MIN` → `TLD_MIN`, `MACRO_MAX` → `TLD_MAX`, `tld_term_color` → `tld_term_has_color`.
- alloc: `get_dim1` → `tld_galloc_dim1`, `get_dim2` → `tld_galloc_dim2`.
- string: `tld_append` → `tld_strbuf_append`, `tld_append_char` → `tld_strbuf_append_char`, `tld_prepend` → `tld_strbuf_prepend`, `tld_string_buffer` → `tld_strbuf`.
- path: `tld_file_exists` → `tld_path_exists`, `tld_dir_exists` → `tld_path_is_dir`, `tld_dir_create` → `tld_path_mkdir`, `tld_get_full_path` → `tld_path_full`, `tld_get_filename` → `tld_path_filename`, `tld_get_dirname` → `tld_path_dirname`, `tld_capture_cmd_line` → `tld_cmdline_capture`.
- bitfield: `tl_bitfield` → `tld_bitfield`, `tl_bitfield_alloc` → `tld_bitfield_alloc`, `tl_bitfield_free` → `tld_bitfield_free`, `tl_bitfield_reset` → `tld_bitfield_reset`, `tl_bitfield_set` → `tld_bitfield_set`, `tl_bitfield_get` → `tld_bitfield_get`, `tl_bitfield_clear` → `tld_bitfield_clear`, `tl_bitfield_copy` → `tld_bitfield_copy`, `tl_bitfield_or` → `tld_bitfield_or`, `tl_bitfield_hamming` → `tld_bitfield_hamming`.
- rng: `rng_state` → `tld_rng`, `rng_dist` → `tld_rng_dist`, `init_rng` → `tld_rng_alloc`, `init_rng_from_rng` → `tld_rng_split`, `free_rng` → `tld_rng_free`, `tl_random_double` → `tld_rng_double`, `tl_random_int` → `tld_rng_int`, `tl_random_gaussian` → `tld_rng_gaussian`, `tl_random_gamma` → `tld_rng_gamma`, `tl_random_sdist_init` → `tld_rng_dist_alloc`, `tl_random_sdist_smpl` → `tld_rng_dist_sample`, `tl_random_sdist_free` → `tld_rng_dist_free`.
- logsum: `init_logsum` → `tld_logsum_init`, `logsum` → `tld_logsum`, `nearly_equal_float` → `tld_nearly_equal_float`, `nearly_equal_double` → `tld_nearly_equal_double`, `TLSAFE_EQ` → `TLD_NEARLY_EQUAL`.
- stats: `norm_quantile` → `tld_quantile_normalise`.
- kdtree: `kdtree` → `tld_kdtree`, `kd_node` → `tld_kdtree_node`, `kddist_euclidian` → `tld_kdtree_dist_euclidean`, `kdplane_check_euclidian` → `tld_kdtree_plane_euclidean`, `kdtree_alloc` → `tld_kdtree_alloc`, `kdtree_free` → `tld_kdtree_free`, `kdtree_reset` → `tld_kdtree_reset`, `kdtree_build` → `tld_kdtree_build`, `kdtree_insert` → `tld_kdtree_insert`, `kdtree_rebalance` → `tld_kdtree_rebalance`, `kdtree_nn_search` → `tld_kdtree_nn_search`, `kdtree_bf_search` → `tld_kdtree_bf_search`.
- tpool: `tld_thread_pool_create` → `tld_thread_pool_alloc`.
- json: `JSON_OBJ` → `TLD_JSON_OBJECT`, `JSON_ARR` → `TLD_JSON_ARRAY`, `JS` → `TLD_JAUTO`, `JO` → `TLD_JOBJ`, `JA` → `TLD_JARR`, `JSTR` → `TLD_JSTR`, `JNUM` → `TLD_JNUM`, `JINT` → `TLD_JINT`, `JBOOL` → `TLD_JBOOL`, `JNULL` → `TLD_JNULL`, `TLD_JSON_UNDEF` → `TLD_JSON_NULL`.
- hash: `tld_hash_iterator_next` → `tld_hash_iter_next`.
- template: `tld_template_extract_var` → `tld_template_vars`.
- sort: `sort_asc_<type>` / `sort_desc_<type>` → `tld_sort_asc_<type>` / `tld_sort_desc_<type>`.
- json internals: `tld_json_obj_chk_space`, `tld_json_obj_realloc`, `tld_json_arr_chk_space` and
  `tld_json_arr_realloc` are `tld__` (not for direct use).
- galloc takes any element type malloc can align (structs, pointers, `long double`); the
  `tld_valid_1D`/`tld_valid_2D` checks are gone, and an over-aligned type is a compile error.
- Source layout: every module is `src/<module>/tld-<module>.{c,h}` (`misc` is `path`, `data/bitfield`
  is `bitfield`, `stats/basic` is `stats/tld-stats`, `norm_quantile` is `stats/tld-quantile`).
- tests: `unit-names` is replaced by `names` (tests/names: symbols, macros and documented names follow
  the rules, and a program may reuse common names); `regress-alloc galloc-any`; misuse
  `err_galloc_align.c`.

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
- tld_ai.h: running out of memory while writing a cache entry no longer frees the caller's request or the reply (double free or use after free in `ai_text`, `ai_text_with`, `ai_choose`, `ai_rate`, `ai_yesno`, `ai_fill` and `ai_embed_all`). The entry is now written as text and the request and reply are never attached to it. The warning then says "cannot write a cache entry: out of memory"; it said "cannot append to cache : Inappropriate ioctl for device" (no path, unrelated errno). A failed append names the file and its own error.
- core: no message (LOG_MSG, WARNING_MSG, ERROR_MSG, SECTION_MSG, KV_MSG, OK_MSG, DEBUG_MSG, tld_progress) changes errno. They did whenever stderr was not a terminal (isatty set ENOTTY), so code that logged and then reported `strerror(errno)` printed "Inappropriate ioctl for device".
- tld_ai.h: running out of memory while parsing JSON is reported as AI_ENOMEM; it was taken for "not JSON", so a JSON state for `ai_yesno`/`ai_choose`/`ai_rate` was quietly sent as a string, a cache line was skipped as damaged, and a reply was called "not a JSON object". Running out of memory while recording `ai_last()` fails the call; it went on and `ai_last()` had no model or host.
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
- tests/oom: `oom-tld` and `ai-oom` fail every allocation of each scenario in turn (14 tld.h
  scenarios, 8 tld_ai.h scenarios against the mock) and fail on a leak, a crash, or a
  failure reported as success. `oom-tld NAME:INDEX` reruns one index; `OOM_TRACE=1` prints
  the failing allocation's stack in an ASan build. The mock takes `--gen SECONDS`.
- unit-alloc allocates 64 MB instead of 4.7 GB, so it runs under the sanitizers;
  tests/ai/compile.sh uses python3 instead of perl.

### Removed
- core: `H1_MSG`, `H2_MSG`, `H3_MSG` and `tld_heading` (unused; they wrote colour codes to stdout even when it was not a terminal).
- Unused names with no users in tld, agp, kalign or samstat: the `tld_str` type with `tld_strmk`,
  `tld_str_alloc`, `tld_str_free`, `tld_strbuf_to_str`, `tld_char_to_str`, `tld_prefix_match` and
  `tld_suffix_match`; the debug printers `tld_hash_print`, `tld_json_obj_print`,
  `tld_json_arr_print`, `kdtree_debug_chk` and `kdtree_debug_print`; the JSON token tests
  `is_open_obj`, `is_close_obj`, `is_open_arr`, `is_close_arr`, `is_colon` and `is_comma` (left from the
  old tokenising parser); `prob2scaledprob` and `scaledprob2prob` (use `logf` and `expf`); and the
  rng's unused `long_jump`. Commented-out code is gone from src/ and the tests.
- seq/ (FASTA/FASTQ/SAM IO, alphabets, reverse complement) and hdf5/. tld now depends only on
  libc, libm and pthreads.

### Changed
- Licence: MIT (was BSD-style).
- Internal names no longer use the program's namespace. Every static function, type, enum
  constant and macro of the implementation is `tld__`/`TLD__`, and implementation-only macros are
  `#undef`'d after use, so a file with `TLD_IMPLEMENTATION` may define `next`, `search`, `jump`,
  `Parser`, `NODE_ROOT`, `AT`, `SCALE`, `EXTERN`, `NARG3` and the like (each clashed before). The
  helpers the public macros call are `tld__` too: `tld__message`, `tld__message_at`, `tld__section`,
  `tld__kv`, `tld__ok`, `tld__debug`, `tld__gfree`, `tld__timer` (was `set_exec_time`),
  `tld__json_obj_build`/`tld__json_arr_build` (were `json_obj_add_multiple`/`json_arr_add_multiple`),
  `TLD__MSG_*` (were `MESSAGE_TYPE_*`), `TLD__FAILED` (was `ADDFAILED`), `TLD__STRINGIFY`/`TLD__TOSTRING`
  and the galloc argument counters; none is for direct use.
- One linkage macro, `TLD_API` (`extern`, `extern "C"` in C++), replaces `tld_external`, `tld_internal`
  and the per-module `EXTERN`/`*_IMPORT` pairs. Include guards are `TLD_<MODULE>_H`. `detect_type` and
  the thread pool's work queue are private; the unused `work_queue_expand` is gone.

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
- kdtree: out of memory no longer corrupts a tree. A rebalance moved every point buffer out of its node and back; when it failed halfway, the buffers leaked and later inserts adopted the caller's point array, which `kdtree_free` then freed. Rebuilding now only re-links the nodes (median by quickselect, no allocation), and a failed `kdtree_insert` or `kdtree_build` leaves the tree unchanged.
- kdtree: searches only read the tree, so several threads may search one tree at once; they kept their recursion depth in the tree. Searching an empty tree fails (with an error) in both `kdtree_nn_search` and `kdtree_bf_search`; one left `*id` unchanged and the other returned 0.
- kdtree: points shorter than the tree's length are zero-padded and split on the same coordinates by insert, build and search, which disagreed before (wrong neighbours). `kdtree_build` adds to the points already in the tree and no longer reorders `x` and `ids`; it used to replace the root, so earlier points were found by `kdtree_bf_search` but not by `kdtree_nn_search`.
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
- json: a JO/JA entry holding NULL makes JSON_OBJ/JSON_ARR fail (NULL, error printed). A nested JSON_OBJ/JSON_ARR returns NULL only when it failed, and the outer build used to skip it and report success without the nested part. JS/JSTR with a NULL string are still skipped.
- json: `json_*_add_multiple` with a caller-supplied container return NULL on failure (they returned the container, so failure looked like success) and free the JO/JA children not yet attached (they leaked). The container keeps the entries added so far and stays the caller's.
- json: `tld_json_search` compares whole keys; it compared at most 256 bytes, so a longer key matched a stored key that is its prefix. `tld_json_dump` indents every level; indentation stopped at 31 spaces (level 8).
- json: JSON_OBJ/JSON_ARR (`json_*_add_multiple`) make room before writing an entry and stop at the first failure. A container created by the call is then freed, and NULL is returned. The containers' realloc counts only slots that were really allocated, and always grows (it was stuck at size 1).
- template: a false IF no longer renders an enclosing IF's ELSE. IF nesting deeper than 512 fails, and parse errors no longer leak.
- template: `{{}}`, `{{IF}}` and `{{IF }}` are errors (they used to become a variable with no name, a variable called `IF`, and an IF on the empty name). Every syntax error prints its reason and byte offset; errors inside a tag used to print only "Function ... failed". Running out of memory while parsing or rendering fails cleanly; before, a failed allocation for a node's text was ignored and the template rendered with pieces missing.

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
