"""Rewrite C code written for the old tld to the names and calls of the new tld.h.

    uv run python tools/migrate.py PATH...            # report what would change
    uv run python tools/migrate.py --write PATH...    # rewrite the files in place
    uv run python tools/migrate.py --diff PATH...     # show the changes as a diff

PATH is a .c/.h file or a directory (searched for .c and .h files). Directories named
.git or build* are skipped, and so is every path matching an --exclude glob (for a
program that carries tld as a submodule: --exclude 'lib/tld/*').

What it does, in code only (comments and string literals are left alone):
  - renames every old name listed in tools/migrate.map (whole identifiers; a member
    access such as p->logsum is not renamed). A name the migrated code defines itself
    (a #define, typedef, struct or function of its own) is not renamed, and the
    report says so.
  - rewrites calls whose arguments changed:
      tld_file_exists(..) == OK / != FAIL / == 0      -> tld_path_exists(..)
      tld_file_exists(..) == FAIL / != OK / != 0      -> !tld_path_exists(..)
      (the same for tld_dir_exists -> tld_path_is_dir)
      tld_auc_calculate(Y, Yh, n, &a)                 -> tld_auc(Y, Yh, n, NULL, &a)
      tld_auc_best_threshold(Y, Yh, n, &t)            -> tld_auc(Y, Yh, n, &t, NULL)
      tl_random_sdist_init(&d, w, n, seed)            -> tld_rng_dist_alloc(&d, w, n)
      kdtree_insert(t, x, id, size)                   -> tld_kdtree_insert(t, x, id)
      kdtree_build(t, x, ids, n, size)                -> tld_kdtree_build(t, x, ids, n)
      tld_json_parse(b, &o)                           -> tld_json_parse(TLD_STR(b), &o)
      tld_global_stop_run = 1                         -> tld_thread_pool_stop_all()
      #include "string/str.h" (a moved module header)  -> #include "string/tld-string.h"
      RUN(f(x)) / ASSERT / galloc / gfree / MMALLOC / MREALLOC / MFREE with no ';'
                                                      -> RUN(f(x));
      LOG_MSG(text) (any *_MSG, one non-literal arg)  -> LOG_MSG("%s", text)
      (not inside a #define, where the format is a parameter the compiler checks
      at each use)
  - flags, with file and line, every place that needs a person: a path predicate used
    any other way (its answer is now 1 = yes, 0 = no), tld_get_filename/dirname (they
    now write a tld_strbuf **), tl_random_sdist_smpl (it now needs a generator), other
    uses of tld_global_stop_run, gfree(&p), a message whose format is not a literal,
    and every name that was removed (seq/, hdf5/, tld_str, debug printers, ...).

Running it twice changes nothing the second time. Afterwards build the program and fix
what the compiler reports (const char * from tld_hash_get, for example); CHANGELOG.md
lists every change.
"""

import argparse
import difflib
import fnmatch
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
MAP = ROOT / "tools" / "migrate.map"

IDENT = re.compile(r"[A-Za-z_]\w*")

# Removed public names, with what to do instead.
REMOVED = {}
for names, why in [
    ("tld_string tld_strmk tld_str_alloc tld_str_free tld_strbuf_to_str tld_char_to_str",
     "the tld_str type is gone: use tld_strbuf"),
    ("tld_prefix_match tld_suffix_match", "removed: compare with strncmp"),
    ("tld_hash_print tld_json_obj_print tld_json_arr_print kdtree_debug_chk kdtree_debug_print",
     "debug printer removed (tld_json_dump writes JSON into a tld_strbuf)"),
    ("prob2scaledprob scaledprob2prob", "removed: use logf / expf"),
    ("H1_MSG H2_MSG H3_MSG tld_heading", "removed: use SECTION_MSG"),
    ("tld_valid_1D tld_valid_2D", "removed: galloc takes any element type"),
    ("tld_internal", "removed: make the function static"),
    ("kd_node kddist_euclidian kdplane_check_euclidian",
     "the k-d tree is opaque and Euclidean; its node type and distance hooks are gone"),
    ("tld_global_cond tld_global_cond_mutex", "removed (tld never used them)"),
    ("tld_sigmoid_safe", None),  # renamed; listed so it is never flagged
    ("tl_seq tl_seq_buffer alloc_tl_seq alloc_tl_seq_buffer free_tl_seq free_tl_seq_buffer "
     "reset_tl_seq_buffer resize_tl_seq resize_tl_seq_buffer rev_comp_tl_seq reverse_comp "
     "open_fasta_fastq_file read_fasta_fastq_file close_seq_file write_seq_buf open_sam_bam "
     "create_alphabet free_alphabet tlalphabet_get_code convert_to_internal "
     "TLSEQIO_READ TLSEQIO_WRITE TLSEQIO_APPEND TLSEQIO_WRITE_GZIPPED TLSEQIO_APPEND_GZIPPED "
     "TL_SEQ_BUFFER_DNA TL_SEQ_BUFFER_PROTEIN TL_SEQ_MAX_NAME_LEN FILE_TYPE_FASTA "
     "FILE_TYPE_FASTQ TLALPHABET_DEFAULT_DNA TLALPHABET_DEFAULT_PROTEIN "
     "TLALPHABET_NOAMBIGUOUS_DNA TLALPHABET_NOAMBIGIOUS_PROTEIN TLALPHABET_REDUCED_PROTEIN",
     "the seq/ module (FASTA/FASTQ/SAM, alphabets) was removed from tld"),
    ("tld_hdf5_open_file tld_hdf5_close_file tld_hdf5_open_group tld_hdf5_close_group "
     "tld_hdf5_add_attribute_string tld_hdf5_read_attribute_string hdf5wrap_search "
     "HDF_READ_DATA HDF_WRITE_DATA HDF_READ_VALUE HDF_READ_ATTRIBUTE HDF_WRITE_ATTRIBUTE",
     "the hdf5/ module was removed from tld"),
]:
    for n in names.split():
        if why:
            REMOVED[n] = why

# module headers that moved (src/<module>/tld-<module>.h)
HEADERS = {
    "data/bitfield.h": "bitfield/tld-bitfield.h",
    "hash/hash.h": "hash/tld-hash.h",
    "hash/hash_struct.h": "hash/tld-hash_struct.h",
    "misc/misc.h": "path/tld-path.h",
    "stats/auc.h": "stats/tld-auc.h",
    "stats/basic.h": "stats/tld-stats.h",
    "stats/norm_quantile.h": "stats/tld-quantile.h",
    "string/str.h": "string/tld-string.h",
    "template/template.h": "template/tld-template.h",
    "tpool/tpool.h": "tpool/tld-tpool.h",
}
GONE_HEADERS = {
    "tpool/tpool_internal.h": "the thread pool's internals are private: include tld.h",
    "tpool/tpool_sig.h": "tld installs no signal handlers: call tld_thread_pool_stop_all() "
                         "from your own",
    "seq/tld-seq.h": "the seq/ module was removed from tld",
    "hdf5/tld-hdf5.h": "the hdf5/ module was removed from tld",
}
INCLUDE = re.compile(r'^([ \t]*#[ \t]*include[ \t]*")([^"]+)(")', re.M)

PREDICATES = {"tld_file_exists", "tld_dir_exists"}
# single statements now (do { } while (0)): a use must end with ';'
STATEMENT_MACROS = {"RUN", "ASSERT", "MMALLOC", "MMALLOC_ALIGNED", "MREALLOC", "MFREE",
                    "galloc", "gfree"}
MSG_MACROS = {"LOG_MSG", "WARNING_MSG", "ERROR_MSG", "SECTION_MSG", "OK_MSG", "DEBUG_MSG"}


def load_map():
    pairs = {}
    for line in MAP.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        old, new = line.split()
        pairs[old] = new
    return pairs


def code_mask(text):
    """mask[i] is True where text[i] is code (not a comment or a string/char literal)."""
    mask = [True] * len(text)
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
        elif c == "/" and text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
        else:
            i += 1
            continue
        for k in range(i, j):
            mask[k] = False
        i = j
    return mask


def directive_mask(text):
    """mask[i] is True inside a preprocessor directive (with its continuation lines)."""
    mask = [False] * len(text)
    for m in re.finditer(r"^[ \t]*#(?:[^\n]*\\\n)*[^\n]*", text, re.M):
        for k in range(m.start(), m.end()):
            mask[k] = True
    return mask


def idents(text, mask):
    """(start, end, name) of every identifier in code."""
    for m in IDENT.finditer(text):
        s = m.start()
        if mask[s] and (s == 0 or not (text[s - 1].isalnum() or text[s - 1] == "_")):
            yield s, m.end(), m.group(0)


def member_access(text, s):
    j = s - 1
    while j >= 0 and text[j] in " \t":
        j -= 1
    return j >= 0 and (text[j] == "." or (text[j] == ">" and j > 0 and text[j - 1] == "-"))


def call_args(text, mask, open_paren):
    """For the '(' at open_paren: (close_paren, [(start, end) of each argument])."""
    depth, start, args = 0, open_paren + 1, []
    for i in range(open_paren, len(text)):
        if not mask[i]:
            continue
        c = text[i]
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
            if depth == 0:
                args.append((start, i))
                return i, args
        elif c == "," and depth == 1:
            args.append((start, i))
            start = i + 1
    return None, []


def skip_ws(text, i):
    while i < len(text) and text[i] in " \t\n":
        i += 1
    return i


class File:
    def __init__(self, path, text):
        self.path, self.text = path, text
        self.flags, self.rewrites = [], []

    def line(self, pos):
        return self.text.count("\n", 0, pos) + 1

    def snippet(self, s, e):
        return " ".join(self.text[s:e].split())[:100]

    def flag(self, pos, why, end=None):
        self.flags.append((self.line(pos), why, self.snippet(pos, end or pos + 60)))


def rewrite_calls(f, pairs):
    """Rewrites and flags that depend on the old names; applied before the renames."""
    text = f.text
    mask = code_mask(text)
    directive = directive_mask(text)
    edits = []  # (start, end, replacement)
    for s, e, name in idents(text, mask):
        if member_access(text, s):
            continue
        p = skip_ws(text, e)
        is_call = p < len(text) and text[p] == "(" and mask[p]
        if name == "tld_global_stop_run":
            m = re.compile(r"\s*=\s*1\s*;").match(text, e)
            if m:
                edits.append((s, m.end(), "tld_thread_pool_stop_all();"))
                f.rewrites.append((f.line(s), "tld_global_stop_run = 1 -> tld_thread_pool_stop_all()"))
            else:
                f.flag(s, "tld_global_stop_run is gone: call tld_thread_pool_stop_all() to stop "
                       "every pool; nothing replaces reading or clearing it")
            continue
        if name in REMOVED and name not in pairs:
            f.flag(s, REMOVED[name])
            continue
        if not is_call:
            continue
        close, args = call_args(text, mask, p)
        if close is None:
            continue
        argtext = [text[a:b].strip() for a, b in args]
        if name in STATEMENT_MACROS and not directive[s]:
            nxt = skip_ws(text, close + 1)
            if nxt < len(text) and text[nxt] not in ";),":
                edits.append((close + 1, close + 1, ";"))
                f.rewrites.append((f.line(s), f"{name}(..) -> {name}(..); (a single statement "
                                   "now, so the ';' is required)"))
        if name in PREDICATES:
            m = re.compile(r"\s*(==|!=)\s*(OK|FAIL|0)\b").match(text, close + 1)
            before = text[:s].rstrip()
            if m and not before.endswith("!"):
                yes = (m.group(1) == "==") == (m.group(2) in ("OK", "0"))
                call = text[s:close + 1]
                edits.append((s, m.end(), call if yes else "!" + call))
                f.rewrites.append((f.line(s), f"{name}(..) {m.group(1)} {m.group(2)} -> "
                                   f"{'' if yes else '!'}{pairs[name]}(..)"))
            else:
                f.flag(s, f"{name} -> {pairs[name]} now answers 1 (yes) or 0 (no), the opposite "
                       "sense of OK/FAIL: check this use", close + 1 + 20)
        elif name in ("tld_get_filename", "tld_get_dirname"):
            f.flag(s, f"{pairs[name]} writes a tld_strbuf ** (was char **): declare "
                   "tld_strbuf *x = NULL, read TLD_STR(x), free with tld_strbuf_free", close + 1)
        elif name in ("tld_auc_calculate", "tld_auc_best_threshold") and len(args) == 4:
            a = argtext
            new = (f"tld_auc({a[0]}, {a[1]}, {a[2]}, NULL, {a[3]})" if name == "tld_auc_calculate"
                   else f"tld_auc({a[0]}, {a[1]}, {a[2]}, {a[3]}, NULL)")
            edits.append((s, close + 1, new))
            f.rewrites.append((f.line(s), f"{name}(..) -> {new}"))
        elif name in ("tl_random_sdist_init", "tld_rng_dist_alloc") and len(args) == 4:
            edits.append((args[2][1], args[3][1], ""))
            f.rewrites.append((f.line(s), f"{name}: seed argument {argtext[3]} dropped (the "
                               "table holds no generator)"))
        elif name in ("tl_random_sdist_smpl", "tld_rng_dist_sample") and len(args) == 1:
            f.flag(s, "tld_rng_dist_sample(d, rng) draws from a generator the caller passes: "
                   "add one (the table no longer holds a generator)", close + 1)
        elif name in ("kdtree_insert", "tld_kdtree_insert") and len(args) == 4 or \
                name in ("kdtree_build", "tld_kdtree_build") and len(args) == 5:
            edits.append((args[-2][1], args[-1][1], ""))
            f.rewrites.append((f.line(s), f"{name}: size argument {argtext[-1]} dropped (the "
                               "tree's dimension comes from tld_kdtree_alloc)"))
        elif name == "tld_json_parse" and len(args) == 2:
            a = argtext[0]
            if not a.startswith("TLD_STR(") and not a.startswith('"'):
                edits.append((args[0][0], args[0][1], f"TLD_STR({a})"))
                f.rewrites.append((f.line(s), f"tld_json_parse({a}, ..) -> "
                                   f"tld_json_parse(TLD_STR({a}), ..)"))
        elif name == "gfree" and len(args) == 1 and argtext[0].startswith("&"):
            f.flag(s, "gfree takes the pointer variable itself: gfree(p), not gfree(&p)", close + 1)
        elif (name in MSG_MACROS or name == "ASSERT") and not directive[s]:
            # inside a #define the format is usually a parameter filled with a literal
            # at each use, which the compiler then checks
            fmt = 1 if name == "ASSERT" else 0
            if len(args) <= fmt:
                continue
            a = text[args[fmt][0]:args[fmt][1]]
            lit = a.strip()
            if lit.startswith('"') or (lit and not mask[args[fmt][0] + a.index(lit[0])]):
                continue
            if len(args) == fmt + 1 and lit:
                lead = a[:len(a) - len(a.lstrip())]
                edits.append((args[fmt][0], args[fmt][1], f'{lead}"%s", {lit}'))
                f.rewrites.append((f.line(s), f'{name}({lit}) -> {name}("%s", {lit}) (the '
                                   "format must be a literal)"))
            else:
                f.flag(s, f"{name}: the format must be a string literal", close + 1)
    for m in INCLUDE.finditer(text):
        path = m.group(2)
        for old, new in HEADERS.items():
            if path == old or path.endswith("/" + old):
                edits.append((m.start(2), m.end(2), path[:len(path) - len(old)] + new))
                f.rewrites.append((f.line(m.start()), f'#include "{path}" -> "{path[:len(path) - len(old)] + new}"'))
        for old, why in GONE_HEADERS.items():
            if path == old or path.endswith("/" + old):
                f.flag(m.start(), why, m.end())
    for s, e, new in sorted(edits, reverse=True):
        text = text[:s] + new + text[e:]
    f.text = text


def defined_names(text, mask, names):
    """Old names this file defines itself (so they are the program's, not tld's)."""
    found = set()
    for m in re.finditer(r"^\s*#\s*define\s+([A-Za-z_]\w*)", text, re.M):
        if m.group(1) in names:
            found.add(m.group(1))
    for m in re.finditer(r"\btypedef\b[^;{]*(?:\{[^}]*\}[^;]*)?\b([A-Za-z_]\w*)\s*;", text):
        if m.group(1) in names and mask[m.start()]:
            found.add(m.group(1))
    for m in re.finditer(r"\b(?:struct|enum|union)\s+([A-Za-z_]\w*)\s*\{", text):
        if m.group(1) in names and mask[m.start()]:
            found.add(m.group(1))
    for m in re.finditer(r"^[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*\{", text, re.M):
        if m.group(1) in names and mask[m.start()]:
            found.add(m.group(1))
    return found


DECL = re.compile(r"(?:\b(?:char|short|int|long|float|double|signed|unsigned|_Bool|bool|"
                  r"size_t|ssize_t|\w+_t|struct\s+\w+|union\s+\w+|enum\s+\w+)\b[\s\*]*|\*\s*)"
                  r"([A-Za-z_]\w*)\s*(?=[;\[,:=)])")


def declared_vars(text, mask, names):
    """Old names this file declares as a variable, parameter or struct field."""
    found = {}
    for m in DECL.finditer(text):
        if m.group(1) in names and mask[m.start(1)]:
            found.setdefault(m.group(1), m.start(1))
    return found


def rename(f, pairs, skip, counts):
    text = f.text
    mask = code_mask(text)
    own_vars = {n: pos for n, pos in declared_vars(text, mask, set(pairs)).items()
                if n not in skip}
    for n, pos in sorted(own_vars.items()):
        f.flag(pos, f"{n} is declared here as a variable or field, so it is not "
               f"renamed in this file; a call to tld's {n} needs {pairs[n]} by hand")
    edits = []
    for s, e, name in idents(text, mask):
        new = pairs.get(name)
        if new and name not in skip and name not in own_vars and not member_access(text, s):
            edits.append((s, e, new))
            counts[name] = counts.get(name, 0) + 1
    for s, e, new in sorted(edits, reverse=True):
        text = text[:s] + new + text[e:]
    f.text = text


def sources(paths, excludes):
    for p in map(pathlib.Path, paths):
        files = [p] if p.is_file() else sorted(q for q in p.rglob("*") if q.suffix in (".c", ".h"))
        for q in files:
            rel = str(q)
            parts = q.parts
            if any(x == ".git" or x.startswith("build") for x in parts[:-1]):
                continue
            if any(fnmatch.fnmatch(rel, g) or fnmatch.fnmatch(str(q.relative_to(p)) if p.is_dir()
                   else rel, g) for g in excludes):
                continue
            if q.is_file():
                yield q


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("paths", nargs="+")
    ap.add_argument("--write", action="store_true", help="rewrite the files in place")
    ap.add_argument("--diff", action="store_true", help="print a unified diff")
    ap.add_argument("--exclude", action="append", default=[], metavar="GLOB",
                    help="skip paths matching GLOB (relative to a directory argument)")
    opt = ap.parse_args()
    pairs = load_map()

    files = []
    for q in sources(opt.paths, opt.exclude):
        try:
            text = q.read_text()
        except UnicodeDecodeError:
            text = q.read_text(encoding="latin-1")
        files.append(File(q, text))

    own = {}
    for f in files:
        for n in defined_names(f.text, code_mask(f.text), set(pairs) | set(REMOVED)):
            own.setdefault(n, []).append(str(f.path))

    counts, changed = {}, 0
    for f in files:
        before = f.text
        rewrite_calls(f, pairs)
        rename(f, pairs, set(own), counts)
        if f.text != before:
            changed += 1
            if opt.diff:
                sys.stdout.writelines(difflib.unified_diff(
                    before.splitlines(True), f.text.splitlines(True), str(f.path), str(f.path)))
            if opt.write:
                f.path.write_text(f.text)

    print(f"# {len(files)} files read, {changed} {'changed' if opt.write else 'would change'}")
    if own:
        print("# not renamed: the code defines these names itself")
        for n, where in sorted(own.items()):
            print(f"  {n}: {', '.join(where[:3])}{' ...' if len(where) > 3 else ''}")
    if counts:
        print("# renamed (uses)")
        for n, c in sorted(counts.items(), key=lambda x: (-x[1], x[0])):
            print(f"  {n} -> {pairs[n]}: {c}")
    rewrites = [(f, r) for f in files for r in f.rewrites]
    if rewrites:
        print(f"# rewritten calls ({len(rewrites)})")
        for f, (line, what) in rewrites:
            print(f"  {f.path}:{line}: {what}")
    flags = [(f, x) for f in files for x in f.flags]
    if flags:
        print(f"# needs a hand edit ({len(flags)})")
        for f, (line, why, snip) in flags:
            print(f"  {f.path}:{line}: {why}\n      {snip}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
