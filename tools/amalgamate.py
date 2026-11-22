"""Build the single-header tld.h from the module sources in src/.

    uv run python tools/amalgamate.py           # write ./tld.h
    uv run python tools/amalgamate.py --check   # exit 1 if ./tld.h is out of date

tld.h starts with the top comment of src/tld.h (title, USAGE, ERRORS, ...). The
declaration half is the rest of src/tld.h with every local header inlined (each once).
The implementation half is src/tld.c with every local .c inlined, wrapped in
#ifdef TLD_IMPLEMENTATION. The licence at the end is COPYING. src/ stays the source of
truth; never edit ./tld.h by hand.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "src"
OUT = ROOT / "tld.h"
INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"')
TOP_COMMENT = re.compile(r"\A/\*.*?\*/\n", re.S)


def top_comment():
    """The leading comment of src/tld.h: the top comment of tld.h."""
    m = TOP_COMMENT.match((SRC / "tld.h").read_text())
    if not m or not m.group(0).startswith("/* tld.h - "):
        sys.exit("src/tld.h must start with the top comment (/* tld.h - ...)")
    return m.group(0)


def licence():
    return "\n/*\n" + (ROOT / "COPYING").read_text().rstrip() + "\n*/\n"




def inline(path, seen, out):
    """Append path's lines to out, replacing local #include "x" with x's contents.
    Headers are inlined at most once (they are include-guarded anyway); a quoted include
    that does not resolve to a file in src/ (e.g. "stdio.h") becomes a system include."""
    rel = path.relative_to(ROOT)
    out.append(f"/* ---- {rel} ---- */\n")
    text = path.read_text()
    if path == SRC / "tld.h":
        text = TOP_COMMENT.sub("", text, count=1)  # emitted first by build()
    for line in text.splitlines(keepends=True):
        m = INCLUDE.match(line)
        if not m:
            out.append(line)
            continue
        target = (path.parent / m.group(1)).resolve()
        if not target.is_file() or SRC not in target.parents:
            out.append(f"#include <{m.group(1)}>\n")
            continue
        if target in seen:
            continue
        seen.add(target)
        inline(target, seen, out)
    if out and not out[-1].endswith("\n"):
        out.append("\n")


def build():
    seen = {(SRC / "tld.h").resolve(), (SRC / "tld.c").resolve()}
    decl, impl = [], []
    inline(SRC / "tld.h", seen, decl)
    # Every header is now in `seen`, so the implementation half inlines only .c files.
    inline(SRC / "tld.c", seen, impl)
    return (
        top_comment()
        + "".join(decl)
        + "\n#ifdef TLD_IMPLEMENTATION\n#ifndef TLD__IMPLEMENTED\n#define TLD__IMPLEMENTED\n"
        + "".join(impl)
        + "#endif /* TLD__IMPLEMENTED */\n#endif /* TLD_IMPLEMENTATION */\n"
        + licence()
    )


def main(argv):
    text = build()
    if "--check" in argv:
        if not OUT.is_file() or OUT.read_text() != text:
            print("tld.h is out of date: run uv run python tools/amalgamate.py", file=sys.stderr)
            return 1
        print("tld.h is up to date")
        return 0
    OUT.write_text(text)
    print(f"wrote {OUT.relative_to(ROOT)} ({text.count(chr(10))} lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
