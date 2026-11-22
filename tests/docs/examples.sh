#!/bin/sh
# tests/docs/examples.sh TLDDOC CC SRC_DIR WORK_DIR: every code example in the doc
# comments of tld.h and tld_ai.h compiles and links against the current headers.
# tlddoc --examples writes each example as its own program (WORK_DIR/tld-NN.c,
# WORK_DIR/tld_ai-NN.c); the programs are compiled, not run (the tld_ai.h ones would
# need a server).
set -u
TLDDOC=$1
CC=$2
SRC=$3
W=$4
rm -rf "$W"
mkdir -p "$W"
"$TLDDOC" --examples "$W" "$SRC" || exit 1

F="-std=c11 -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-but-set-variable \
   -Wno-unused-label -Wno-unused-function -I$SRC"
printf '#define TLD_IMPLEMENTATION\n#include "tld.h"\n' >"$W/impl_tld.c"
printf '#define TLD_IMPLEMENTATION\n#define AI_IMPLEMENTATION\n#include "tld_ai.h"\n' >"$W/impl_ai.c"
$CC -std=c11 -I"$SRC" -c "$W/impl_tld.c" -o "$W/impl_tld.o" || exit 1
$CC -std=c11 -I"$SRC" -c "$W/impl_ai.c" -o "$W/impl_ai.o" || exit 1

n=0
fail=0
for f in "$W"/tld-*.c "$W"/tld_ai-*.c; do
        [ -f "$f" ] || continue
        case $f in
        */tld_ai-*) impl=$W/impl_ai.o ;;
        *) impl=$W/impl_tld.o ;;
        esac
        n=$((n + 1))
        # shellcheck disable=SC2086
        if ! $CC $F "$f" "$impl" -o "${f%.c}" -lm -lpthread 2>"${f%.c}.err"; then
                echo "FAIL  $(head -1 "$f" | sed 's|^/\* Example from the doc comment of \(.*\) (tlddoc.*|\1|'):"
                echo "      $f"
                sed 's/^/      /' "${f%.c}.err" | head -20
                fail=$((fail + 1))
        fi
done
[ "$n" -gt 0 ] || { echo "FAIL  no examples found"; exit 1; }
[ "$fail" = 0 ] || { echo "doc-examples: $fail of $n examples do not compile"; exit 1; }
echo "doc-examples: all $n examples compile"
