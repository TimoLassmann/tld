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

### Known limitation
- 2D galloc sizes its data block with `sizeof(T*)` rather than `sizeof(T)`. That is harmless on 64-bit, but too small for 8-byte element types on 32-bit platforms. Fixing it needs a new helper.
