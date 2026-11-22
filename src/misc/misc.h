/* misc: file and directory checks, absolute paths, file and directory names of a
 * path, and capturing the command line. */
#ifndef TLD_MISC_H
#define TLD_MISC_H

#include "../core/tld-core.h"
#include "../string/str.h"

/* OK if name exists (a file, directory or anything else stat() sees), FAIL otherwise,
 * without a message. Test with == OK. */
TLD_API int tld_file_exists(const char* name);

/* OK if name is a directory that can be opened; otherwise prints an error and returns
 * FAIL. */
TLD_API int tld_dir_exists(const char *name);

/* Create the directory dir (mode 0700; its parent must exist). OK when it was created
 * or already exists as a directory (with a warning); FAIL otherwise. */
TLD_API int tld_dir_create(char *dir);

/* The absolute, symlink-free path of the existing file filename into *full. A non-NULL
 * *full is cleared and reused; otherwise a buffer is allocated (free with
 * tld_strbuf_free). FAIL when the file does not exist. */
TLD_API int tld_get_full_path(tld_strbuf **full, char *filename);

/* Append argv[0] .. argv[argc - 1], each followed by a space, to *cmdline (allocated when
 * NULL; free with tld_strbuf_free). For recording how a program was run. */
TLD_API int tld_capture_cmd_line(tld_strbuf **cmdline, const int argc, char *const argv[]);

/* A new string in *out (free with MFREE or free) with the part of path after the last
 * '/'. FAIL when that part is empty (path is empty or ends in '/'). */
TLD_API int tld_get_filename(char* path, char** out);

/* A new string in *out (free with MFREE or free) with the part of path before the last
 * '/' ("/" for "/name"). FAIL when path contains no '/'. */
TLD_API int tld_get_dirname(char* path, char** out);

#endif
