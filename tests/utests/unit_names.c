/* unit-names: every public function and macro of tld (as of 961795c, minus the removed
 * seq/ and hdf5/ modules) must still exist. This file fails to compile or link if one is
 * lost or renamed. Generated from scratch/modernisation/baseline; edit by hand when a name
 * is deliberately ADDED (names are never removed). */
#include "tld.h"
#include <stdio.h>

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
#ifndef H1_MSG
#error "tld public macro H1_MSG is missing"
#endif
#ifndef H2_MSG
#error "tld public macro H2_MSG is missing"
#endif
#ifndef H3_MSG
#error "tld public macro H3_MSG is missing"
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
static tld_any_fn const fns[] = {
        (tld_any_fn)detect_type,
        (tld_any_fn)free_rng,
        (tld_any_fn)galloc_hlp,
        (tld_any_fn)galloc_too_few,
        (tld_any_fn)get_dim1,
        (tld_any_fn)get_dim2,
        (tld_any_fn)get_ptr,
        (tld_any_fn)init_logsum,
        (tld_any_fn)init_rng,
        (tld_any_fn)init_rng_from_rng,
        (tld_any_fn)is_close_arr,
        (tld_any_fn)is_close_obj,
        (tld_any_fn)is_colon,
        (tld_any_fn)is_comma,
        (tld_any_fn)is_open_arr,
        (tld_any_fn)is_open_obj,
        (tld_any_fn)json_arr_add_multiple,
        (tld_any_fn)json_obj_add_multiple,
        (tld_any_fn)kddist_euclidian,
        (tld_any_fn)kdplane_check_euclidian,
        (tld_any_fn)kdtree_alloc,
        (tld_any_fn)kdtree_bf_search,
        (tld_any_fn)kdtree_build,
        (tld_any_fn)kdtree_debug_chk,
        (tld_any_fn)kdtree_debug_print,
        (tld_any_fn)kdtree_free,
        (tld_any_fn)kdtree_insert,
        (tld_any_fn)kdtree_nn_search,
        (tld_any_fn)kdtree_rebalance,
        (tld_any_fn)kdtree_reset,
        (tld_any_fn)logsum,
        (tld_any_fn)nearly_equal_double,
        (tld_any_fn)nearly_equal_float,
        (tld_any_fn)norm_quantile,
        (tld_any_fn)prob2scaledprob,
        (tld_any_fn)scaledprob2prob,
        (tld_any_fn)set_exec_time,
        (tld_any_fn)sort_asc_char,
        (tld_any_fn)sort_asc_double,
        (tld_any_fn)sort_asc_float,
        (tld_any_fn)sort_asc_int16_t,
        (tld_any_fn)sort_asc_int32_t,
        (tld_any_fn)sort_asc_int64_t,
        (tld_any_fn)sort_asc_int8_t,
        (tld_any_fn)sort_asc_uint16_t,
        (tld_any_fn)sort_asc_uint32_t,
        (tld_any_fn)sort_asc_uint64_t,
        (tld_any_fn)sort_asc_uint8_t,
        (tld_any_fn)sort_desc_char,
        (tld_any_fn)sort_desc_double,
        (tld_any_fn)sort_desc_float,
        (tld_any_fn)sort_desc_int16_t,
        (tld_any_fn)sort_desc_int32_t,
        (tld_any_fn)sort_desc_int64_t,
        (tld_any_fn)sort_desc_int8_t,
        (tld_any_fn)sort_desc_uint16_t,
        (tld_any_fn)sort_desc_uint32_t,
        (tld_any_fn)sort_desc_uint64_t,
        (tld_any_fn)sort_desc_uint8_t,
        (tld_any_fn)tl_bitfield_alloc,
        (tld_any_fn)tl_bitfield_clear,
        (tld_any_fn)tl_bitfield_copy,
        (tld_any_fn)tl_bitfield_free,
        (tld_any_fn)tl_bitfield_get,
        (tld_any_fn)tl_bitfield_hamming,
        (tld_any_fn)tl_bitfield_or,
        (tld_any_fn)tl_bitfield_reset,
        (tld_any_fn)tl_bitfield_set,
        (tld_any_fn)tl_random_double,
        (tld_any_fn)tl_random_gamma,
        (tld_any_fn)tl_random_gaussian,
        (tld_any_fn)tl_random_int,
        (tld_any_fn)tl_random_sdist_free,
        (tld_any_fn)tl_random_sdist_init,
        (tld_any_fn)tl_random_sdist_smpl,
        (tld_any_fn)tld_append,
        (tld_any_fn)tld_append_char,
        (tld_any_fn)tld_auc,
        (tld_any_fn)tld_auc_best_threshold,
        (tld_any_fn)tld_auc_calculate,
        (tld_any_fn)tld_capture_cmd_line,
        (tld_any_fn)tld_char_to_str,
        (tld_any_fn)tld_dir_create,
        (tld_any_fn)tld_dir_exists,
        (tld_any_fn)tld_file_exists,
        (tld_any_fn)tld_free,
        (tld_any_fn)tld_get_dirname,
        (tld_any_fn)tld_get_filename,
        (tld_any_fn)tld_get_full_path,
        (tld_any_fn)tld_hash_add,
        (tld_any_fn)tld_hash_alloc,
        (tld_any_fn)tld_hash_del,
        (tld_any_fn)tld_hash_free,
        (tld_any_fn)tld_hash_get,
        (tld_any_fn)tld_hash_iter_init,
        (tld_any_fn)tld_hash_iterator_next,
        (tld_any_fn)tld_hash_print,
        (tld_any_fn)tld_hash_rename,
        (tld_any_fn)tld_heading,
        (tld_any_fn)tld_json_arr_alloc,
        (tld_any_fn)tld_json_arr_chk_space,
        (tld_any_fn)tld_json_arr_copy,
        (tld_any_fn)tld_json_arr_free,
        (tld_any_fn)tld_json_arr_print,
        (tld_any_fn)tld_json_arr_realloc,
        (tld_any_fn)tld_json_dump,
        (tld_any_fn)tld_json_dump_arr,
        (tld_any_fn)tld_json_obj_alloc,
        (tld_any_fn)tld_json_obj_chk_space,
        (tld_any_fn)tld_json_obj_copy,
        (tld_any_fn)tld_json_obj_free,
        (tld_any_fn)tld_json_obj_print,
        (tld_any_fn)tld_json_obj_realloc,
        (tld_any_fn)tld_json_parse,
        (tld_any_fn)tld_json_search,
        (tld_any_fn)tld_json_set_val,
        (tld_any_fn)tld_json_val_alloc,
        (tld_any_fn)tld_json_val_copy,
        (tld_any_fn)tld_json_val_free,
        (tld_any_fn)tld_kde_count_pdf,
        (tld_any_fn)tld_kde_pdf,
        (tld_any_fn)tld_mean,
        (tld_any_fn)tld_message,
        (tld_any_fn)tld_normal_pdf,
        (tld_any_fn)tld_pearson_corr,
        (tld_any_fn)tld_pop_covariance,
        (tld_any_fn)tld_pop_variance,
        (tld_any_fn)tld_prefix_match,
        (tld_any_fn)tld_prepend,
        (tld_any_fn)tld_rng_set_seed,
        (tld_any_fn)tld_sample_covariance,
        (tld_any_fn)tld_sample_variance,
        (tld_any_fn)tld_shannon,
        (tld_any_fn)tld_shuffle_char,
        (tld_any_fn)tld_shuffle_double,
        (tld_any_fn)tld_shuffle_float,
        (tld_any_fn)tld_shuffle_int16_t,
        (tld_any_fn)tld_shuffle_int32_t,
        (tld_any_fn)tld_shuffle_int64_t,
        (tld_any_fn)tld_shuffle_int8_t,
        (tld_any_fn)tld_shuffle_uint16_t,
        (tld_any_fn)tld_shuffle_uint32_t,
        (tld_any_fn)tld_shuffle_uint64_t,
        (tld_any_fn)tld_shuffle_uint8_t,
        (tld_any_fn)tld_sigmoid,
        (tld_any_fn)tld_sigmoid_safe,
        (tld_any_fn)tld_standardise,
        (tld_any_fn)tld_str_alloc,
        (tld_any_fn)tld_str_free,
        (tld_any_fn)tld_strbuf_alloc,
        (tld_any_fn)tld_strbuf_clear,
        (tld_any_fn)tld_strbuf_copy,
        (tld_any_fn)tld_strbuf_free,
        (tld_any_fn)tld_strbuf_to_str,
        (tld_any_fn)tld_strmk,
        (tld_any_fn)tld_suffix_match,
        (tld_any_fn)tld_template_apply,
        (tld_any_fn)tld_template_extract_var,
        (tld_any_fn)tld_thread_pool_add,
        (tld_any_fn)tld_thread_pool_create,
        (tld_any_fn)tld_thread_pool_free,
        (tld_any_fn)tld_thread_pool_wait,
};

int main(void)
{
        int n = 0;
        (void)vars;
        for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++) n += fns[i] != NULL;
        fprintf(stdout, "unit-names: %d functions and 51 macros present\n", n);
        return n == 162 ? EXIT_SUCCESS : EXIT_FAILURE;
}
