#!/bin/sh
# covered.sh SRC: every public function of SRC/tld.h with a pointer argument (tld_*, not
# the tld__ internals) has a case in SRC/tests/null/null_args.c, and so has every public
# function of SRC/tld_doc.h (tld_doc_*, declared before TLD_DOC_IMPLEMENTATION; its
# cases are the DOC_CASES, run by null-args-doc).
src=$1
cases=$src/tests/null/null_args.c
missing=0
for f in $(sed -n '1,/^#ifdef TLD_IMPLEMENTATION/p' "$src/tld.h" |
           grep '^TLD_API' | grep '\*' | grep -o 'tld_[a-z0-9_]*[[:space:]]*(' | tr -d ' (' | grep -v '^tld__' | sort -u); do
        if ! grep -q "$f(" "$cases"; then
                echo "null-args-covered: $f has no case in tests/null/null_args.c"
                missing=$((missing + 1))
        fi
done
n_doc=0
for f in $(sed -n '1,/^#ifdef TLD_DOC_IMPLEMENTATION/p' "$src/tld_doc.h" |
           grep -E '^[a-z].*[ *]tld_doc_[a-z0-9_]*\(' | grep -o 'tld_doc_[a-z0-9_]*(' | tr -d '(' | sort -u); do
        n_doc=$((n_doc + 1))
        if ! sed -n '/^#define DOC_CASES/,/^$/p' "$cases" | grep -q "$f("; then
                echo "null-args-covered: $f (tld_doc.h) has no case in DOC_CASES of tests/null/null_args.c"
                missing=$((missing + 1))
        fi
done
[ "$n_doc" -gt 0 ] || { echo "null-args-covered: no tld_doc_ function found in $src/tld_doc.h"; missing=$((missing + 1)); }
[ "$missing" -eq 0 ] || exit 1
