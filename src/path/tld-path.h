/* path: file and directory checks, absolute paths, file and directory names of a
 * path, and capturing the command line. */
#ifndef TLD_PATH_H
#define TLD_PATH_H

#include "../core/tld-core.h"
#include "../string/tld-string.h"

/* OK if name exists (a file, directory or anything else stat() sees), FAIL otherwise,
 * without a message. Test with == OK. */
TLD_API int tld_path_exists(const char* name);

/* OK if name is a directory that can be opened; otherwise prints an error and returns
 * FAIL. */
TLD_API int tld_path_is_dir(const char *name);

/* Create the directory dir (mode 0700; its parent must exist). OK when it was created
 * or already exists as a directory (with a warning); FAIL otherwise. */
TLD_API int tld_path_mkdir(char *dir);

/* The absolute, symlink-free path of the existing file filename into *full. A non-NULL
 * *full is cleared and reused; otherwise a buffer is allocated (free with
 * tld_strbuf_free). FAIL when the file does not exist. */
TLD_API int tld_path_full(tld_strbuf **full, char *filename);

/* The whole contents of the file at path in a new buffer in *out (free with
 * tld_strbuf_free); the bytes may include NUL. FAIL, with an error naming the file and
 * the reason, when it cannot be opened or read or is larger than a buffer holds
 * (INT32_MAX - 1 bytes); *out is then unchanged. */
TLD_API int tld_path_read(const char *path, tld_strbuf **out);

/* Append argv[0] .. argv[argc - 1], each followed by a space, to *cmdline (allocated when
 * NULL; free with tld_strbuf_free). For recording how a program was run. */
TLD_API int tld_cmdline_capture(tld_strbuf **cmdline, const int argc, char *const argv[]);

/* A new string in *out (free with MFREE or free) with the part of path after the last
 * '/'. FAIL when that part is empty (path is empty or ends in '/'). */
TLD_API int tld_path_filename(char* path, char** out);

/* A new string in *out (free with MFREE or free) with the part of path before the last
 * '/' ("/" for "/name"). FAIL when path contains no '/'. */
TLD_API int tld_path_dirname(char* path, char** out);

#endif
