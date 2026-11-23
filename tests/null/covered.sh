#!/bin/sh
# covered.sh SRC: every public function of SRC/tld.h with a pointer argument (tld_*, not
# the tld__ internals) has a case in SRC/tests/null/null_args.c.
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
[ "$missing" -eq 0 ] || exit 1
