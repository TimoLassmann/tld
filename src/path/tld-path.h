/* path: file and directory checks, absolute paths, file and directory names of a
 * path, and capturing the command line. */
#ifndef TLD_PATH_H
#define TLD_PATH_H

#include "../core/tld-core.h"
#include "../string/tld-string.h"

/* Predicates: 1 or 0, never a message (a NULL path prints an error and gives 0).
 * tld_path_exists: 1 if path exists (a file, directory or anything else stat() sees).
 * tld_path_is_dir: 1 if path is a directory (following symlinks). */
TLD_API int tld_path_exists(const char *path);
TLD_API int tld_path_is_dir(const char *path);

/* Create the directory dir (mode 0700; its parent must exist). OK when it was created
 * or already exists as a directory (silently); FAIL with an error otherwise, also when
 * a file of that name exists. */
TLD_API int tld_path_mkdir(const char *dir);

/* Functions with a tld_strbuf ** result share one rule: a non-NULL *out is cleared and
 * reused, a NULL *out gets a new buffer (free it with tld_strbuf_free). On failure (with
 * an error printed) a buffer the call allocated is freed and *out is left unchanged; a
 * caller's buffer stays the caller's, possibly cleared. */

/* The absolute, symlink-free path of the existing file or directory path into *full.
 * FAIL when it does not exist. */
TLD_API int tld_path_full(tld_strbuf **full, const char *path);

/* The part of path after the last '/' into *out. FAIL when that part is empty (path is
 * empty or ends in '/'). */
TLD_API int tld_path_filename(const char *path, tld_strbuf **out);

/* The part of path before the last '/' into *out ("/" for "/name"). FAIL when path
 * contains no '/'. */
TLD_API int tld_path_dirname(const char *path, tld_strbuf **out);

/* The whole contents of the file at path into *out; the bytes may include NUL. FAIL
 * when it cannot be opened or read (the error names the file and the reason) or is
 * larger than a buffer holds (INT32_MAX - 1 bytes). */
TLD_API int tld_path_read(const char *path, tld_strbuf **out);

/* argv[0] .. argv[argc - 1], each followed by a space, into *cmdline: for recording
 * how a program was run. */
TLD_API int tld_cmdline_capture(tld_strbuf **cmdline, const int argc, char *const argv[]);

#endif
