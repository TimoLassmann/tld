/* unit-names: every public function and macro of tld (as of 961795c, minus the removed
 * seq/ and hdf5/ modules) must still exist. This file fails to compile or link if one is
 * lost or renamed. Generated from scratch/modernisation/baseline; edit by hand when a name
 * is deliberately ADDED (names are never removed). */
#include "tld.h"
#include "test.h"
#include <stdio.h>
#include <string.h>

#ifndef ADDFAILED
#error "tld public macro ADDFAILED is missing"
#endif
#ifndef ASSERT
#error "tld public macro ASSERT is missing"
#endif
#ifndef ERROR_MSG
#error "tld public macro ERROR_MSG is missing"
#endif
#ifndef FAIL
#error "tld public macro FAIL is missing"
#endif
#ifndef galloc
#error "tld public macro galloc is missing"
#endif
#ifndef GALLOC_ARGS
#error "tld public macro GALLOC_ARGS is missing"
#endif
#ifndef gfree
#error "tld public macro gfree is missing"
#endif
#ifndef JA
#error "tld public macro JA is missing"
#endif
#ifndef JO
#error "tld public macro JO is missing"
#endif
#ifndef JS
#error "tld public macro JS is missing"
#endif
#ifndef JSON_ARR
#error "tld public macro JSON_ARR is missing"
#endif
#ifndef JSON_OBJ
#error "tld public macro JSON_OBJ is missing"
#endif
#ifndef LOG_MSG
#error "tld public macro LOG_MSG is missing"
#endif
#ifndef MACRO_MAX
#error "tld public macro MACRO_MAX is missing"
#endif
#ifndef MACRO_MIN
#error "tld public macro MACRO_MIN is missing"
#endif
#ifndef MESSAGE_TYPE_ERROR
#error "tld public macro MESSAGE_TYPE_ERROR is missing"
#endif
#ifndef MESSAGE_TYPE_MSG
#error "tld public macro MESSAGE_TYPE_MSG is missing"
#endif
#ifndef MESSAGE_TYPE_WARNING
#error "tld public macro MESSAGE_TYPE_WARNING is missing"
#endif
#ifndef MFREE
#error "tld public macro MFREE is missing"
#endif
#ifndef MMALLOC
#error "tld public macro MMALLOC is missing"
#endif
#ifndef MMALLOC_ALIGNED
#error "tld public macro MMALLOC_ALIGNED is missing"
#endif
#ifndef MREALLOC
#error "tld public macro MREALLOC is missing"
#endif
#ifndef NARG3
#error "tld public macro NARG3 is missing"
#endif
#ifndef OK
#error "tld public macro OK is missing"
#endif
#ifndef RUN
#error "tld public macro RUN is missing"
#endif
#ifndef SHUFFLE_HEADER
#error "tld public macro SHUFFLE_HEADER is missing"
#endif
#ifndef SORT_HEADER
#error "tld public macro SORT_HEADER is missing"
#endif
#ifndef TLD_END_TIMER
#error "tld public macro TLD_END_TIMER is missing"
#endif
#ifndef tld_external
#error "tld public macro tld_external is missing"
#endif
#ifndef tld_internal
#error "tld public macro tld_internal is missing"
#endif
#ifndef TLD_SHUFFLE
#error "tld public macro TLD_SHUFFLE is missing"
#endif
#ifndef TLD_SORT_ASC
#error "tld public macro TLD_SORT_ASC is missing"
#endif
#ifndef TLD_SORT_DESC
#error "tld public macro TLD_SORT_DESC is missing"
#endif
#ifndef TLD_START_TIMER
#error "tld public macro TLD_START_TIMER is missing"
#endif
#ifndef TLD_STR
#error "tld public macro TLD_STR is missing"
#endif
#ifndef tld_stringify
#error "tld public macro tld_stringify is missing"
#endif
#ifndef TLD_STRLEN
#error "tld public macro TLD_STRLEN is missing"
#endif
#ifndef tld_tostring
#error "tld public macro tld_tostring is missing"
#endif
#ifndef tld_valid_1D
#error "tld public macro tld_valid_1D is missing"
#endif
#ifndef tld_valid_2D
#error "tld public macro tld_valid_2D is missing"
#endif
#ifndef TLSAFE_EQ
#error "tld public macro TLSAFE_EQ is missing"
#endif
#ifndef WARNING_MSG
#error "tld public macro WARNING_MSG is missing"
#endif
#ifndef XARG3
#error "tld public macro XARG3 is missing"
#endif
#ifndef XGALLOC_ARGS
#error "tld public macro XGALLOC_ARGS is missing"
#endif
#ifndef XGALLOC_ARGS_1
#error "tld public macro XGALLOC_ARGS_1 is missing"
#endif
#ifndef XGALLOC_ARGS_2
#error "tld public macro XGALLOC_ARGS_2 is missing"
#endif
#ifndef XGALLOC_ARGS_3
#error "tld public macro XGALLOC_ARGS_3 is missing"
#endif
#ifndef XXGALLOC_ARGS
#error "tld public macro XXGALLOC_ARGS is missing"
#endif

/* public global variables */
static const void *const vars[] = { &tld_global_cond, &tld_global_cond_mutex, &tld_global_stop_run };

/* work_queue_* (tpool_internal.h) are internal and not part of the interface. */
typedef void (*tld_any_fn)(void);
struct name_fn { const char *name; tld_any_fn fn; };
#define F(x) { #x, (tld_any_fn)x }
static const struct name_fn fns[] = {
        F(detect_type),
        F(free_rng),
        F(get_dim1),
        F(get_dim2),
        F(init_logsum),
        F(init_rng),
        F(init_rng_from_rng),
        F(is_close_arr),
        F(is_close_obj),
        F(is_colon),
        F(is_comma),
        F(is_open_arr),
        F(is_open_obj),
        F(json_arr_add_multiple),
        F(json_obj_add_multiple),
        F(kddist_euclidian),
        F(kdplane_check_euclidian),
        F(kdtree_alloc),
        F(kdtree_bf_search),
        F(kdtree_build),
        F(kdtree_debug_chk),
        F(kdtree_debug_print),
        F(kdtree_free),
        F(kdtree_insert),
        F(kdtree_nn_search),
        F(kdtree_rebalance),
        F(kdtree_reset),
        F(logsum),
        F(nearly_equal_double),
        F(nearly_equal_float),
        F(norm_quantile),
        F(prob2scaledprob),
        F(scaledprob2prob),
        F(set_exec_time),
        F(sort_asc_char),
        F(sort_asc_double),
        F(sort_asc_float),
        F(sort_asc_int16_t),
        F(sort_asc_int32_t),
        F(sort_asc_int64_t),
        F(sort_asc_int8_t),
        F(sort_asc_uint16_t),
        F(sort_asc_uint32_t),
        F(sort_asc_uint64_t),
        F(sort_asc_uint8_t),
        F(sort_desc_char),
        F(sort_desc_double),
        F(sort_desc_float),
        F(sort_desc_int16_t),
        F(sort_desc_int32_t),
        F(sort_desc_int64_t),
        F(sort_desc_int8_t),
        F(sort_desc_uint16_t),
        F(sort_desc_uint32_t),
        F(sort_desc_uint64_t),
        F(sort_desc_uint8_t),
        F(tl_bitfield_alloc),
        F(tl_bitfield_clear),
        F(tl_bitfield_copy),
        F(tl_bitfield_free),
        F(tl_bitfield_get),
        F(tl_bitfield_hamming),
        F(tl_bitfield_or),
        F(tl_bitfield_reset),
        F(tl_bitfield_set),
        F(tl_random_double),
        F(tl_random_gamma),
        F(tl_random_gaussian),
        F(tl_random_int),
        F(tl_random_sdist_free),
        F(tl_random_sdist_init),
        F(tl_random_sdist_smpl),
        F(tld_append),
        F(tld_append_char),
        F(tld_auc),
        F(tld_auc_best_threshold),
        F(tld_auc_calculate),
        F(tld_capture_cmd_line),
        F(tld_char_to_str),
        F(tld_dir_create),
        F(tld_dir_exists),
        F(tld_file_exists),
        F(tld_free),
        F(tld_get_dirname),
        F(tld_get_filename),
        F(tld_get_full_path),
        F(tld_hash_add),
        F(tld_hash_alloc),
        F(tld_hash_del),
        F(tld_hash_free),
        F(tld_hash_get),
        F(tld_hash_iter_init),
        F(tld_hash_iterator_next),
        F(tld_hash_print),
        F(tld_hash_rename),
        F(tld_json_arr_alloc),
        F(tld_json_arr_chk_space),
        F(tld_json_arr_copy),
        F(tld_json_arr_free),
        F(tld_json_arr_print),
        F(tld_json_arr_realloc),
        F(tld_json_dump),
        F(tld_json_dump_arr),
        F(tld_json_obj_alloc),
        F(tld_json_obj_chk_space),
        F(tld_json_obj_copy),
        F(tld_json_obj_free),
        F(tld_json_obj_print),
        F(tld_json_obj_realloc),
        F(tld_json_parse),
        F(tld_json_search),
        F(tld_json_set_val),
        F(tld_json_val_alloc),
        F(tld_json_val_copy),
        F(tld_json_val_free),
        F(tld_kde_count_pdf),
        F(tld_kde_pdf),
        F(tld_mean),
        F(tld_message),
        F(tld_normal_pdf),
        F(tld_pearson_corr),
        F(tld_pop_covariance),
        F(tld_pop_variance),
        F(tld_prefix_match),
        F(tld_prepend),
        F(tld_rng_set_seed),
        F(tld_sample_covariance),
        F(tld_sample_variance),
        F(tld_shannon),
        F(tld_shuffle_char),
        F(tld_shuffle_double),
        F(tld_shuffle_float),
        F(tld_shuffle_int16_t),
        F(tld_shuffle_int32_t),
        F(tld_shuffle_int64_t),
        F(tld_shuffle_int8_t),
        F(tld_shuffle_uint16_t),
        F(tld_shuffle_uint32_t),
        F(tld_shuffle_uint64_t),
        F(tld_shuffle_uint8_t),
        F(tld_sigmoid),
        F(tld_sigmoid_safe),
        F(tld_standardise),
        F(tld_str_alloc),
        F(tld_str_free),
        F(tld_strbuf_alloc),
        F(tld_strbuf_clear),
        F(tld_strbuf_copy),
        F(tld_strbuf_free),
        F(tld_strbuf_to_str),
        F(tld_strmk),
        F(tld_suffix_match),
        F(tld_template_apply),
        F(tld_template_extract_var),
        F(tld_thread_pool_add),
        F(tld_thread_pool_create),
        F(tld_thread_pool_free),
        F(tld_thread_pool_wait),
};

int main(int argc, char *argv[])
{
        int n = 0;
        const size_t nf = sizeof fns / sizeof fns[0];
        (void)vars;
        test_init(&argc, argv, "[PATTERN ...]",
                  "With no arguments: checks that all 158 public functions and 48 public macros\n"
                  "of tld exist (a lost or renamed one fails the compile or the link).\n"
                  "With PATTERNs: lists the public functions whose name contains any PATTERN.\n"
                  "Example: unit-names hash json_parse");
        if (argc > 1) {
                int hits = 0;
                for (size_t i = 0; i < nf; i++) {
                        for (int a = 1; a < argc; a++) {
                                if (strstr(fns[i].name, argv[a])) {
                                        fprintf(stdout, "%s\n", fns[i].name);
                                        hits++;
                                        break;
                                }
                        }
                }
                KV_MSG("matches", "%d of %zu functions", hits, nf);
                return hits ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        SECTION_MSG("public names");
        for (size_t i = 0; i < nf; i++) {
                n += fns[i].fn != NULL;
                DEBUG_MSG("%s", fns[i].name);
        }
        KV_MSG("functions", "%d", n);
        KV_MSG("macros", "48");
        if (n != 158) {
                fprintf(stderr, "unit-names: %d functions, expected 158\n", n);
                return EXIT_FAILURE;
        }
        OK_MSG("%d functions and 48 macros present", n);
        return EXIT_SUCCESS;
}
