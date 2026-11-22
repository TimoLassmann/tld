# Changelog

## modernisation branch (unreleased)

This branch rebuilds tld as two single-header libraries: `tld.h`, generated from `src/`,
and `tld_ai.h`, which builds on it. Both now follow one naming scheme and one set of
rules for errors, NULL arguments and memory. Programs written for the old tld need
changes. Nearly every public name was renamed ("Renamed" below; `tools/migrate.map`
lists every pair), some signatures changed ("Changed"), and the seq/ and hdf5/ modules
are gone. Bugs fixed without an interface change are listed under "Fixed".

### Added

- Single-header build: `./tld.h`, generated from `src/` by `tools/amalgamate.py`. Put
  `#define TLD_IMPLEMENTATION` before the include in one .c file. The top comment of
  tld.h is the top comment of `src/tld.h`, and the licence at its end is `COPYING`.
- tld_ai.h: a single header (needs tld.h) that calls the LLMs, decision models and
  embedding models of the local Ollama server as C functions: `ai_text`, `ai_yesno`,
  `ai_choose`/`AI_CHOICE`, `ai_rate`/`AI_SCALE`, `ai_fill`/`AI_STRUCT`, `ai_embed*`,
  `ai_nearest`, `ai_parallel`, `ai_review`. Its public API is that of the tld_ai.h it
  grew out of (in the aic project), plus `ai_status()` with `AI_E*` codes, the
  `max_retries` option, `ai_last()` with `model` and `host`, and
  `ai_defaults()`/`AI_CONFIG` for defaults set in code (per-call options > environment
  > `ai_defaults` > built-in; each call replaces the previous one, NULL resets).
- tld_ai.h needs no libcurl, SQLite or cJSON. It has its own HTTP/1.1 client over
  sockets (Content-Length, chunked, read-to-close, `TLD_AI_TIMEOUT`, SIGPIPE-safe; https
  gives `AI_EHTTP`). JSON goes through tld.h. The cache is an append-only JSON-lines
  file (`TLD_AI_DB`, shared between processes with flock), and each review is one JSON
  file (`TLD_AI_REVIEWS`).
- tld_ai.h: the cache is off unless a file is named with
  `ai_defaults(AI_CONFIG(.db = "..."))` or `TLD_AI_DB` (which wins), so a plain run
  writes no files. Environment variables are `TLD_AI_HOST`, `TLD_AI_LLM`,
  `TLD_AI_DECIDER`, `TLD_AI_EMBEDDER`, `TLD_AI_DB`, `TLD_AI_REVIEWS`, `TLD_AI_NUM_CTX`,
  `TLD_AI_TIMEOUT` and `TLD_AI_FRESH`. The defaults are `inkling-2-mini:latest`,
  `autotrust-jev:latest` and `embeddinggemma-2:latest`. Every call that needs the
  server probes it first (`AI_ENOSERVER`, re-probed at most every 5 s). There is no
  model guard: on a machine that holds one model, start Ollama with
  `OLLAMA_MAX_LOADED_MODELS=1`.
- tld_ai.h: `TLD_AI_FRESH` takes 1/0, true/false, yes/no, on/off in any case;
  `TLD_AI_NUM_CTX` and `TLD_AI_TIMEOUT` must be whole positive integers; other values are
  ignored with a warning. `extra_options` keys replace built-in option keys of the same
  name. At `TLD_VERBOSE=2` every request and its outcome is logged. `examples/hello.c`
  shows prompt, host, model, time and tokens on stderr and only the answer on stdout.
- core: rich terminal output on stderr: `SECTION_MSG`, `KV_MSG`, `OK_MSG`, `DEBUG_MSG`
  and `tld_progress()`, plus `tld_verbosity()` and `tld_term_has_color()`. Colour appears
  only when stderr is a terminal and `NO_COLOR` is unset; `TLD_VERBOSE` is 0 (errors
  only), 1 (default) or 2 (debug). WARNING and ERROR tags are coloured on a terminal.
- json: typed entries `TLD_JSTR`, `TLD_JNUM`, `TLD_JINT`, `TLD_JBOOL` and `TLD_JNULL`; path
  getters `tld_json_get_{str,num,int,bool,obj,arr}` (`"message.content"`,
  `"embeddings[0][5]"`); `tld_json_parse_n(text, len, &obj)` parses a byte range;
  `tld_json_obj_add(&obj, entry)` and `tld_json_arr_add(&arr, entry)` add one entry,
  creating the container when it is NULL (a NULL `TLD_JSTR`/`TLD_JAUTO` string is
  skipped); `tld_json_dump(n, b, TLD_JSON_COMPACT)` writes no whitespace outside strings.
- string: `tld_strbuf_append_n(b, s, n)` appends n bytes (NUL bytes included; s may point
  into b).
- path: `tld_path_read(path, &buf)` reads a whole file into a `tld_strbuf`, with an error
  naming the file and the reason when it fails.
- tpool: `tld_thread_pool_stop_all()` stops every pool for the rest of the program; it
  only sets an atomic flag, so a program may call it from its own signal handler.
- docs: `docs/tld.md` and `docs/tld_ai.md`, generated from the header comments by
  `tools/tlddoc.c` (`cmake --build build --target docs`). Every public symbol has a doc
  comment.
- CI: GitHub Actions builds with `-Wall -Wextra -Werror` and runs every test on Ubuntu
  x86-64 (gcc, clang), Ubuntu arm64 (gcc), macOS (clang) and Alpine (musl), plus
  ASan+UBSan (with leak checking), MemorySanitizer, ThreadSanitizer and 60 s per fuzz
  target on every push.

### Renamed: one naming scheme

Public functions, types and variables start with `tld_` and the module word, macros and
constants with `TLD_`. The error-handling idiom keeps its short names (`OK`, `FAIL`,
`RUN`, `ASSERT`, `*_MSG`, `MMALLOC`, `MMALLOC_ALIGNED`, `MREALLOC`, `MFREE`, `galloc`,
`gfree`). The `names` test holds every name of tld.h to these rules. Old → new:

- core: `MACRO_MIN` → `TLD_MIN`, `MACRO_MAX` → `TLD_MAX`, `tld_term_color` →
  `tld_term_has_color`, `tld_external` → `TLD_API`.
- alloc: `get_dim1` → `tld_galloc_dim1`, `get_dim2` → `tld_galloc_dim2`.
- string: `tld_append` → `tld_strbuf_append`, `tld_append_char` →
  `tld_strbuf_append_char`, `tld_prepend` → `tld_strbuf_prepend`, `tld_string_buffer` →
  `tld_strbuf`.
- path: `tld_file_exists` → `tld_path_exists`, `tld_dir_exists` → `tld_path_is_dir`,
  `tld_dir_create` → `tld_path_mkdir`, `tld_get_full_path` → `tld_path_full`,
  `tld_get_filename` → `tld_path_filename`, `tld_get_dirname` → `tld_path_dirname`,
  `tld_capture_cmd_line` → `tld_cmdline_capture`.
- bitfield: `tl_bitfield` → `tld_bitfield`, and every `tl_bitfield_*` function →
  `tld_bitfield_*` (`alloc`, `free`, `reset`, `set`, `get`, `clear`, `copy`, `or`,
  `hamming`).
- rng: `rng_state` → `tld_rng`, `rng_dist` → `tld_rng_dist`, `init_rng` → `tld_rng_alloc`,
  `init_rng_from_rng` → `tld_rng_split`, `free_rng` → `tld_rng_free`, `tl_random_double` →
  `tld_rng_double`, `tl_random_int` → `tld_rng_int`, `tl_random_gaussian` →
  `tld_rng_gaussian`, `tl_random_gamma` → `tld_rng_gamma`, `tl_random_sdist_init` →
  `tld_rng_dist_alloc`, `tl_random_sdist_smpl` → `tld_rng_dist_sample`,
  `tl_random_sdist_free` → `tld_rng_dist_free` (the alias table also changed its
  arguments; see "Changed").
- logsum: `init_logsum` → `tld_logsum_init`, `logsum` → `tld_logsum`, `nearly_equal_float`
  → `tld_nearly_equal_float`, `nearly_equal_double` → `tld_nearly_equal_double`,
  `TLSAFE_EQ` → `TLD_NEARLY_EQUAL`.
- stats: `norm_quantile` → `tld_quantile_normalise`, `tld_sigmoid_safe` → `tld_sigmoid`.
- kdtree: `kdtree` → `tld_kdtree`, and `kdtree_alloc`, `_free`, `_reset`, `_build`,
  `_insert`, `_rebalance`, `_nn_search`, `_bf_search` → `tld_kdtree_*` (insert and build
  also changed their arguments; see "Changed").
- tpool: `tld_thread_pool_create` → `tld_thread_pool_alloc`.
- json: `JSON_OBJ` → `TLD_JSON_OBJECT`, `JSON_ARR` → `TLD_JSON_ARRAY`, `JS` → `TLD_JAUTO`,
  `JO` → `TLD_JOBJ`, `JA` → `TLD_JARR`, `JSTR` → `TLD_JSTR`, `JNUM` → `TLD_JNUM`, `JINT`
  → `TLD_JINT`, `JBOOL` → `TLD_JBOOL`, `JNULL` → `TLD_JNULL`, `TLD_JSON_UNDEF` →
  `TLD_JSON_NULL`. The entry kinds of `tld_json_decoration` are `TLD__JVAL_*` (were
  `VAL_STRING`, `VAL_OBJ`, ...), so a program may define its own `VAL_*` names.
- hash: `tld_hash_iterator_next` → `tld_hash_iter_next`.
- template: `tld_template_extract_var` → `tld_template_vars`.
- sort: `sort_asc_<type>` / `sort_desc_<type>` → `tld_sort_asc_<type>` /
  `tld_sort_desc_<type>`.
- Source layout: every module is `src/<module>/tld-<module>.{c,h}` (`misc` is `path`,
  `data/bitfield` is `bitfield`, `stats/basic` is `stats/tld-stats`, `norm_quantile` is
  `stats/tld-quantile`).

### Changed

Interface changes, by module. Each one either removes a way to corrupt memory silently
or makes a call read the way it is written.

- core: every message macro (`LOG_MSG`, `WARNING_MSG`, `ERROR_MSG`, `ASSERT`,
  `SECTION_MSG`, `KV_MSG`, `OK_MSG`, `DEBUG_MSG`) requires a string-literal format, and
  the format is checked against the arguments. `LOG_MSG(text)` is a compile error; write
  `LOG_MSG("%s", text)`. A `%` in text used as a format reads or writes random memory.
- core: `LOG_MSG`, `WARNING_MSG` and `ERROR_MSG` write to stderr (was stdout), so stdout
  stays free for program output. `ERROR_MSG`, `RUN` and `ASSERT` report the caller's file
  and line (they reported a fixed line inside tld-core.c).
- core: `RUN`, `ASSERT`, `MMALLOC`, `MMALLOC_ALIGNED`, `MREALLOC` and `MFREE` are single
  statements (`do { } while (0)`): a trailing `;` is required, and they can be the body
  of an unbraced if/else.
- core: `TLD_MIN` and `TLD_MAX` evaluate each argument once (statement expressions, gcc and
  clang); `MACRO_MAX(i++, n)` used to increment twice. They no longer work in constant
  expressions (array sizes, case labels, static initialisers).
- alloc: `gfree(p)` sets p to NULL, so a second gfree is harmless instead of a double free.
  p must be the pointer variable itself: `gfree(&p)` is a compile error (it freed the
  address of a local).
- alloc: `galloc` takes any element type malloc can align (structs, pointers,
  `long double`); the `tld_valid_1D`/`tld_valid_2D` checks are gone, and an over-aligned
  type is a compile error.
- string: `tld_strbuf_append` and `tld_strbuf_prepend` take `const char *`.
- hash: `tld_hash_get` and `tld_hash_iter_next` hand out `const char *` (they point into
  the table); `tld_hash_add`, `_del` and `_rename` take `const char *`. Declare
  `const char *v` at call sites.
- path: `tld_path_exists` and `tld_path_is_dir` return 1 (yes) or 0 (no) instead of OK
  (0) / FAIL (1), so `if(tld_path_exists(f))` means what it says. Neither prints anything
  (`tld_dir_exists` printed an error for "no"). `tld_path_is_dir` uses stat() (it checked
  that opendir() succeeds). Migrate `X(..) == OK` to `X(..)` and `X(..) == FAIL` or
  `!= OK` to `!X(..)`.
- path: `tld_path_mkdir` is silent when the directory already exists (it warned).
- path: `tld_path_filename` and `tld_path_dirname` return a `tld_strbuf` (was a `char *`
  to MFREE). Every `tld_strbuf **` result of the module (`tld_path_full`,
  `tld_path_filename`, `tld_path_dirname`, `tld_path_read`, `tld_cmdline_capture`)
  follows one rule: a non-NULL `*out` is cleared and reused, NULL gets a new buffer, and
  on failure a buffer the call made is freed. So `tld_cmdline_capture` now replaces the
  text of a reused buffer (it appended). Path arguments are `const char *`.
- timing: `TLD_END_TIMER` writes a LOG line on stderr ("exec time: 1.23s"); it printed to
  stdout.
- json: `tld_json_parse` takes the NUL-terminated text (`const char *`) instead of a
  `tld_strbuf *`; for a buffer b pass `TLD_STR(b)`.
- json: `tld_json_set_val(obj, path, v)` takes a `const char *` path in the getters'
  syntax (`"a.b[2].c"`, `"[0]"`) and sets exactly that value; a missing last key is added
  and index n appends. It used to replace same-named keys anywhere in the tree, skipping
  the rest of a level after the first match, and did nothing for a missing key.
- json: a parsed top-level array dumps back as the array (was `{"Top Level Arr":[...]}`).
  The new `top_level_arr` field of `tld_json_obj` marks the wrapper; copies keep it, and
  once the wrapper holds anything else it is written as an object.
- stats: `tld_auc(Y, Y_hat, n, &t, &auc)` takes NULL for an output that is not wanted and
  replaces `tld_auc_calculate` (`tld_auc(Y, Y_hat, n, NULL, &auc)`) and
  `tld_auc_best_threshold` (`tld_auc(Y, Y_hat, n, &t, NULL)`).
- stats: one sigmoid. `tld_sigmoid` is the former `tld_sigmoid_safe` (NaN gives NaN, +inf
  1, -inf 0); the old `tld_sigmoid`, which failed on those, is gone.
- rng: one Gaussian generator. `tld_rng_gaussian` uses the polar method that
  `tld_rng_gamma` already used, with one shared cache (the separate Box-Muller code and
  its state are gone). The draws for a given seed differ from before, so results that
  depend on `tld_rng_gaussian` are not bit-identical to earlier runs.
- rng: the alias table holds no generator: `tld_rng_dist_alloc(&d, w, n)` (no seed) and
  `tld_rng_dist_sample(d, rng)` draw from the caller's generator, so one table serves
  several threads.
- kdtree: `tld_kdtree` is opaque. A tree holds points of the dimension given to
  `tld_kdtree_alloc`, so `tld_kdtree_insert(t, x, id)` and `tld_kdtree_build(t, x, ids, n)`
  lose their size argument (shorter points used to be zero-padded). Distance is
  Euclidean; see "Removed" for the hooks that are gone.
- tpool: `tld_thread_pool` is opaque. `tld_thread_pool_stop_all()` replaces setting
  `tld_global_stop_run`, and the library installs no signal handlers (it installed
  SIGINT/SIGABRT handlers, and creating a pool reset the flag).
- tpool: `tld_thread_pool_wait` returns int: OK when every job ran, FAIL when a time limit
  or a stop refused or abandoned jobs (it returned void, and jobs vanished silently).
  `tld_thread_pool_add` returns FAIL after a stop; the data is still the caller's.
- NULL arguments: every public function checks its pointer arguments. A NULL that the
  function's comment does not allow prints an ERROR line and returns the failure value:
  FAIL, NULL, 0 for a 1/0 answer, NaN for `tld_rng_double`, `tld_rng_gaussian` and
  `tld_rng_gamma`, and -1 for `tld_rng_int` and `tld_rng_dist_sample`. 72 of these calls
  used to crash, among them every bitfield function, the stats outputs,
  `tld_strbuf_append`/`prepend`/`copy` and the rng draws. The free functions still
  ignore NULL silently. `tld_template_apply(txt, NULL)` and `tld_json_search(NULL, ...)`
  returned OK, `tld_thread_pool_add(pool, NULL, ...)` queued a NULL job that a worker then
  called, and `tld_galloc_dim1`/`dim2` and the `tld_json_get_*` getters failed silently on
  a NULL pointer (a missing path is still a silent FAIL).
- Internal names stay out of the program's namespace. Every static function, type, enum
  constant and macro of the implementation is `tld__`/`TLD__`, and implementation-only
  macros are `#undef`'d after use, so a file with `TLD_IMPLEMENTATION` may define `next`,
  `search`, `jump`, `Parser`, `NODE_ROOT`, `AT`, `SCALE`, `EXTERN`, `NARG3` and the like
  (each clashed before). The helpers the public macros call are `tld__` too and are not
  for direct use: `tld__message`, `tld__message_at`, `tld__section`, `tld__kv`, `tld__ok`,
  `tld__debug`, `tld__gfree` (was `tld_free`), `tld__galloc` (was `galloc_hlp`),
  `tld__timer` (was `set_exec_time`), `tld__json_obj_build`/`tld__json_arr_build` (were
  `json_obj_add_multiple`/`json_arr_add_multiple`), the JSON container growth functions,
  `TLD__MSG_*` (were `MESSAGE_TYPE_*`), `TLD__FAILED` (was `ADDFAILED`),
  `TLD__STRINGIFY`/`TLD__TOSTRING` and the galloc argument counters.
- One linkage macro, `TLD_API` (`extern`, `extern "C"` in C++), replaces `tld_external`,
  `tld_internal` and the per-module `EXTERN`/`*_IMPORT` pairs. Include guards are
  `TLD_<MODULE>_H`.
- Licence: MIT (was BSD-style).

#### tld_ai.h

- `ai_text`, `ai_text_with` and `ai_fmt` return `const char *`, and string fields of
  `AI_STRUCT` types are `const char *`, so passing them to free() warns. Prompts and
  `ai_fmt` require a literal, checked printf format, like the message macros
  (`(ai_text)(fmt, ...)` calls the function directly).
- Messages: the warnings (an ignored environment value, a damaged or unwritable cache
  line, 100000 results kept without `ai_reset()`) are tld.h `WARNING_MSG` lines and the
  review notices `LOG_MSG` lines, all prefixed `tld_ai:`, so `TLD_VERBOSE=0` silences
  them. The undocumented `AI_LOG` override hook and the `ai:` prefix are gone.
- `AI_CHOICE` and `AI_SCALE` no longer put a `name__count` constant into the caller's
  enum, so a `switch` over the labels needs no `case name__count:` to satisfy -Wswitch.
- No silent truncation: a host whose name, path or port is longer than its buffer is a
  bad host (`AI_EHTTP`, "expected http://host:port") instead of a shortened one; a model
  name or decision name over 291 bytes, and a review directory too long for a path, fail
  with `AI_EARG` instead of using a cut-off name.
- `extra_options` names that are empty or contain `.` or `[` are rejected with `AI_EARG`.
- Header sections: MEMORY means who owns returned values, as in tld.h (it was RETURNED
  MEMORY); the note on model RAM on the server is SERVER MEMORY.
- tld_ai.h uses tld.h's helpers instead of its own copies: the compact JSON dump, text
  parsing, the file reader and the entry adders, plus `TLD__LITERAL`, `TLD_MIN` and plain
  format attributes (were `AI__LITERAL`, `AI__MIN`, `AI__PRINTF`). `strdup` and
  `gmtime_r` + `strftime` replace hand-written copies, and chunked replies are copied
  with `tld_strbuf_append_n` instead of writing a NUL into the received buffer.

### Removed

- seq/ (FASTA/FASTQ/SAM IO, alphabets, reverse complement) and hdf5/. tld now depends only
  on libc, libm and pthreads.
- core: `H1_MSG`, `H2_MSG`, `H3_MSG` and `tld_heading` (unused; they wrote colour codes to
  stdout even when it was not a terminal).
- Names with no users in tld, agp, kalign or samstat: the `tld_str` type with `tld_strmk`,
  `tld_str_alloc`, `tld_str_free`, `tld_strbuf_to_str`, `tld_char_to_str`,
  `tld_prefix_match` and `tld_suffix_match`; the debug printers `tld_hash_print`,
  `tld_json_obj_print`, `tld_json_arr_print`, `kdtree_debug_chk` and `kdtree_debug_print`;
  the JSON token tests `is_open_obj`, `is_close_obj`, `is_open_arr`, `is_close_arr`,
  `is_colon` and `is_comma` (left from the old tokenising parser); `prob2scaledprob` and
  `scaledprob2prob` (use `logf` and `expf`); the rng's `long_jump`; the thread pool's
  `work_queue_expand`.
- stats: `tld_auc_calculate` and `tld_auc_best_threshold` (use `tld_auc`), and the old
  `tld_sigmoid` (see "Changed").
- kdtree: the node type (`kd_node`), the `fptr_cmp`/`fptr_plane` distance hooks,
  `kddist_euclidian`, `kdplane_check_euclidian` and the settable `rebalance_at`.
- tpool: `tld_global_stop_run` (use `tld_thread_pool_stop_all()`), and
  `tld_global_cond_mutex` and `tld_global_cond`, which tld never used.
- tld_ai.h: `AI_LOG`.
- Commented-out code in src/ and the tests.

### Fixed

#### Memory errors

- string: a buffer growing past about 1 GB overflowed its int size, returned OK and wrote
  past the end of the block. Sizes are computed in size_t; a buffer holds at most
  INT32_MAX - 1 bytes, and an append past that fails and leaves the buffer unchanged.
- string: `tld_strbuf_append(b, TLD_STR(b))` and `tld_strbuf_prepend(b, TLD_STR(b))` (text
  from the buffer itself, or a suffix of it) work; when the buffer had to grow they read
  freed memory. `tld_strbuf_copy(b, b)` is a no-op. Buffers grow geometrically (the
  allocated length was not updated).
- alloc: 2D `galloc` sizes the data block by the element type (it used the pointer size:
  8x too much memory for char matrices, 2x for int/float, too little for 8-byte types on
  32-bit). It computes sizes in `size_t` and fails cleanly on overflow or out of memory,
  and a caller variable named `i` no longer breaks the row setup.
- kdtree: out of memory no longer corrupts a tree. A rebalance moved every point buffer
  out of its node and back; when it failed halfway, the buffers leaked and later inserts
  adopted the caller's point array, which `tld_kdtree_free` then freed. Rebuilding now
  only re-links the nodes (median by quickselect, no allocation), and a failed
  `tld_kdtree_insert` or `tld_kdtree_build` leaves the tree unchanged. A failed yard
  realloc returns FAIL instead of using freed memory.
- template: running out of memory while parsing or rendering fails cleanly; a failed
  allocation for a node's text was ignored and the template rendered with pieces missing.
- json: `tld_json_search` finds all matches; it overflowed its result with 5 or more matches
  and skipped later siblings. `tld_json_val_copy` and `tld_json_set_val` release the
  target's old contents, so changing a value's type is safe. `tld_json_obj_alloc` and
  `tld_json_arr_alloc` check every allocation and clean up on failure.
- json: building with `TLD_JSON_OBJECT`/`TLD_JSON_ARRAY` makes room before writing an entry
  and stops at the first failure; a container created by the call is then freed and NULL
  returned. Given a container, the back ends return NULL on failure (they returned the
  container, so failure looked like success) and free the `TLD_JOBJ`/`TLD_JARR` children
  not yet attached (they leaked); the container keeps the entries added so far. The
  containers grow (they were stuck at size 1), counting only slots really allocated.
- tld_ai.h: running out of memory while writing a cache entry no longer frees the caller's
  request or the reply (double free or use after free in `ai_text`, `ai_text_with`,
  `ai_choose`, `ai_rate`, `ai_yesno`, `ai_fill` and `ai_embed_all`). The entry is written
  as text, and the request and reply are never attached to it. The warning says "cannot
  write a cache entry: out of memory" (it said "cannot append to cache : Inappropriate
  ioctl for device": no path, unrelated errno); a failed append names the file and its
  own error.
- tld_ai.h: the chunked-reply decoder could wrap a size check on 32-bit systems and read
  past the reply.

#### Wrong results

- rng: seed 0 (`tld_rng_alloc(&r, 0)`) takes 64 bits from /dev/urandom, mixed with the
  time in nanoseconds, the process id and a counter. It was made from the time in seconds
  and `clock()` only, in 32 bits (the process id was never mixed in), so generators made
  in one loop, or jobs started in the same second, often got the same stream.
- rng: `tld_rng_double` returns 53-bit values in the open interval (0,1). `tld_rng_int` is
  unbiased (rejection sampling) and returns 0 for a <= 1. Seeded sequences change.
- rng: `tld_rng_dist_alloc` no longer prints its table, rejects n <= 0 and NaN or negative
  weights, and frees on error. `tld_rng_split` no longer copies the parent's cached gaussian.
- shuffle: shuffles are uniform (`j = i + tld_rng_int(rng, n - i)`); seeded output changes.
- logsum: `tld_nearly_equal_float`/`tld_nearly_equal_double` compare relative to |a|+|b|;
  they behaved like ==. The lookup table is set up thread-safely, and on first use if
  `tld_logsum_init()` was never called. NaN input returns NaN.
- stats: `tld_auc` returns FAIL for single-class labels or NaN scores; it returned NaN with
  OK. The best threshold is no longer clamped to [0,1], so logit scores work.
- stats: KDE uses a two-pass variance and a non-zero fallback bandwidth for constant data
  (which gave an all-zero density). It returns FAIL for `n_steps <= 0`, non-finite data,
  negative counts or a zero total count.
- stats: `tld_sample_variance`, `tld_sample_covariance` and `tld_standardise` return FAIL
  for n < 2, and `tld_mean` for n < 1. `tld_normal_pdf` returns FAIL for sigma <= 0.
  `tld_shannon` returns FAIL for negative or NaN values and 0 for an all-zero vector.
- stats: quantile normalisation gives tied values the mean of the reference quantiles over
  their positions; NaN input returns FAIL.
- sort, kdtree, stats: qsort comparators return 0 for equal elements.
- kdtree: searches only read the tree, so several threads may search one tree at once
  (they kept their recursion depth in the tree). Searching an empty tree fails with an
  error in both `tld_kdtree_nn_search` and `tld_kdtree_bf_search`; one left `*id`
  unchanged and the other returned 0. `tld_kdtree_build` adds to the points already in
  the tree and no longer reorders `x` and `ids`; it replaced the root, so earlier points
  were found by `tld_kdtree_bf_search` but not by `tld_kdtree_nn_search`. Inserts no
  longer print to stdout.
- json: `tld_json_parse` is strict (RFC 8259). It returns FAIL on malformed, truncated or
  too-deep (>512) input, on trailing characters, on non-JSON literals (`hello`, `True`,
  `NaN`, `01`), on raw control characters (below 0x20) in strings, on malformed UTF-8
  (stray bytes, truncated, overlong and surrogate sequences, code points above U+10FFFF)
  and on numbers too large for a double. It returned OK with a NULL or partial tree, and
  read `1e999` as infinity. Underflow still gives 0, and a lone surrogate escape
  (`\ud800`) becomes U+FFFD.
- json: quoted values are always strings ("123", "true" and "{" no longer change type).
  String escapes, including `\uXXXX` and surrogate pairs, are decoded to UTF-8, and
  `\u0000` is rejected. Unquoted `null` parses to `TLD_JSON_NULL` and is written as
  `null`. Integers outside the int32 range are stored as `TLD_JSON_DBL` (for example
  Ollama durations in nanoseconds); integers beyond 2^53 lose precision.
- json: the writer escapes strings and keys, writes a byte that is not well-formed UTF-8
  as `�` (so text built from, say, Latin-1 still dumps as valid JSON), writes
  doubles so they read back exactly, and writes inf and nan as `null`. `tld_json_dump`
  indents every level; indentation stopped at 31 spaces (level 8).
- json: the type guessing in `TLD_JAUTO` only matches whole numbers and the exact words
  true/false (in lower, Capital or UPPER case); "", "truex" and "2024-11" stay strings.
- json: a `TLD_JOBJ`/`TLD_JARR` entry holding NULL makes `TLD_JSON_OBJECT`/`TLD_JSON_ARRAY`
  fail (NULL, error printed). A nested build returns NULL only when it failed, and the
  outer build used to skip it and report success without the nested part.
- json: `tld_json_search` compares whole keys; it compared at most 256 bytes, so a longer
  key matched a stored key that is its prefix.
- template: a false IF no longer renders an enclosing IF's ELSE. IF nesting deeper than
  512 fails, and parse errors no longer leak. `{{}}`, `{{IF}}` and `{{IF }}` are errors
  (they became a variable with no name, a variable called `IF`, and an IF on the empty
  name). Every syntax error prints its reason and byte offset; errors inside a tag
  printed only "Function ... failed".
- hash: `tld_hash_add` with a value just read by `tld_hash_get` for the same key keeps it;
  the value was cleared before it was copied and became "".
- hash: add, rename and alloc check every allocation and return code and no longer leak.
  `tld_hash_rename` keeps the old entry if adding the new key fails. `tld_hash_alloc`
  fails for sizes above the largest prime in its table, and growing past that prime
  doubles the table while the size fits in an int, then fails (it overflowed int).
- path: `tld_path_full` hands back the buffer it allocates through `*full`, and on error
  never frees a buffer the caller supplied. `tld_path_mkdir` returns FAIL when mkdir
  fails. `tld_path_dirname("/file")` returns "/".
- bitfield: `tld_bitfield_hamming` returns FAIL when the mask length differs from the
  bitfields.
- tpool: `tld_thread_pool_wait` waits until the queue is empty and no job is running; it
  could return with jobs still queued. `tld_thread_pool_free` runs every queued job before
  stopping (it dropped them). Reaching maxtime or a stop stops every worker, and wait and
  free no longer hang. Nothing is logged to stdout.
- core: no message (`LOG_MSG`, `WARNING_MSG`, `ERROR_MSG`, `SECTION_MSG`, `KV_MSG`,
  `OK_MSG`, `DEBUG_MSG`, `tld_progress`) changes errno. They did whenever stderr was not
  a terminal (isatty set ENOTTY), so code that logged and then reported
  `strerror(errno)` printed "Inappropriate ioctl for device".
- core: the internal `error()`/`verror()`, which clashed with glibc's `error()` when
  linking statically, are gone.
- alloc: `MFREE(NULL)` no longer warns and still sets the pointer to NULL; `gfree(NULL)`
  is a no-op. Size messages use `%zu`.
- single header: the file with `TLD_IMPLEMENTATION` asks for POSIX.1-2008 itself
  (`_POSIX_C_SOURCE`, `_DEFAULT_SOURCE`, `_DARWIN_C_SOURCE`, at the top of tld.h). A
  strict `-std=c11` build on glibc used `realpath`, `localtime_r` and `strnlen` undeclared
  (an implicit `realpath` truncates its pointer result on 64-bit).
- tld_ai.h: running out of memory while parsing JSON is reported as `AI_ENOMEM`. It was
  taken for "not JSON", so a JSON state for `ai_yesno`/`ai_choose`/`ai_rate` was quietly
  sent as a string, a cache line was skipped as damaged, and a reply was called "not a
  JSON object". Running out of memory while recording `ai_last()` fails the call; the
  call went on, and `ai_last()` had no model or host.
- tld_ai.h, relative to the version it grew out of: a failed call no longer shifts replay
  occurrence numbers, a failed /api/tags is not cached as digest "unknown", every
  allocation is checked, `ai_fill` leaves the struct zeroed when every attempt fails,
  decision calls fill `ai_last()` tokens, `ai_yesno` rejects values outside [0,1], review
  and cache keys separate their parts, and calls made after the exit cleanup fail with
  `AI_EARG` instead of crashing. Each resolved address gets its share of the connect
  timeout. A review file containing a NUL byte is an error (it was cut short). Existing
  SQLite caches (`ai.db`) are not read.

### Tests and tooling

- `names`: the symbols, macros and documented names of tld.h follow the naming rules, and
  a program may reuse common names (it replaces `unit-names`).
- `null-args` (193 calls, each in its own process) and `null-args-covered` (every public
  function with a pointer argument has a case).
- `misuse-rejected`: the compiler rejects text as a format, `gfree(&p)`, free() on tld_ai.h
  results, writable pointers into a hash, mismatched format arguments and over-aligned
  galloc types.
- tests/oom: `oom-tld` and `ai-oom` fail every allocation of each scenario in turn (tld.h
  scenarios, and tld_ai.h scenarios against the mock) and fail on a leak, a crash, or a
  failure reported as success. `oom-tld NAME:INDEX` reruns one index; `OOM_TRACE=1` prints
  the failing allocation's stack in an ASan build. The mock takes `--gen SECONDS`.
- tests/fuzz: libFuzzer targets for JSON, templates and tld_ai.h replies (decoding and the
  whole HTTP exchange), with seeds; `-DTLD_FUZZ=ON`, run with `tests/fuzz/run.sh`.
- Regression checks for each fix (`regress-*`), including `regress-json strict` and
  `top-level-roundtrip`, `regress-str limit` and `append-n`, `regress-alloc minmax` and
  `galloc-any`, `regress-misc predicates` and `read`, and `single-header-strict`; tpool
  checks for refused and abandoned jobs; `ai-arena`; tld_ai.h checks for over-long host
  parts, model names and review directories.
- `docs-current` fails when a manual is stale or a public symbol has no doc comment. The
  manual shows declarations without `TLD_API`, and a module comment that starts with an
  identifier (`tld_strbuf, ...`) keeps its spelling.
- unit-alloc allocates 64 MB instead of 4.7 GB, so it runs under the sanitizers;
  tests/ai/compile.sh uses python3 instead of perl.
- Internal: duplicated code collapsed with no change in results (checked bit for bit
  against the previous build): one splitmix64 seeding loop, one deviation-sum helper
  behind the variance and covariance functions, one kde evaluator, one array of structs
  (not one allocation per element) in quantile normalisation and AUC, one add helper per
  JSON container, one prime-size table in hash, one parse step shared by the two template
  functions, and a timer without the hand-written timeval arithmetic.
