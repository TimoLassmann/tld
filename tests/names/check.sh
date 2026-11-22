#!/bin/sh
# tests/names/check.sh CC SRC_DIR [NM]: every name tld.h gives a program follows the
# naming rules, so it cannot collide with the program's own names:
#   global symbols                     tld_...   (internals tld__...)
#   static symbols                     tld__...
#   macros (user view and TLD_IMPLEMENTATION view)
#                                      TLD_... / TLD__..., galloc, gfree, or one of the
#                                      idiom names OK FAIL RUN ASSERT *_MSG MMALLOC ...
#   types, constants and variables     tld_... / TLD_... (from the index of docs/tld.md)
# and a program that defines common names (next, search, Parser, NODE_ROOT, AT, ...)
# after the implementation compiles (tests/names/clash.c).
set -u
CC=$1
SRC=$2
NM=${3:-nm}
T=$(mktemp -d "${TMPDIR:-/tmp}/tld-names.XXXXXX")
trap 'rm -rf "$T"' EXIT
fail=0
bad() { echo "FAIL  $*"; fail=1; }

IDIOM='^(OK|FAIL|RUN|ASSERT|ERROR_MSG|WARNING_MSG|LOG_MSG|DEBUG_MSG|SECTION_MSG|KV_MSG|OK_MSG|MMALLOC|MMALLOC_ALIGNED|MREALLOC|MFREE|galloc|gfree)$'

printf '#define TLD_IMPLEMENTATION\n#include "tld.h"\n' > "$T/impl.c"
printf '#include "tld.h"\n' > "$T/use.c"
# baselines: the system headers tld.h includes, as a plain file sees them (use view)
# and with the feature macros tld.h requests for the implementation (impl view)
grep -h '^[[:space:]]*#[[:space:]]*include[[:space:]]*<' "$SRC/tld.h" | sort -u > "$T/sys_use.c"
{ printf '#define _POSIX_C_SOURCE 200809L\n#define _DEFAULT_SOURCE\n#define _DARWIN_C_SOURCE\n'
  cat "$T/sys_use.c"; } > "$T/sys_impl.c"

# symbols
$CC -std=c11 -c -I"$SRC" "$T/impl.c" -o "$T/impl.o" || { bad "tld.h implementation does not compile"; exit 1; }
$NM "$T/impl.o" | awk 'NF == 3 && $2 ~ /^[TDBSRCtdbsrc]$/ {print $2, $3}' | sed 's/ _/ /' > "$T/syms"
n=$(wc -l < "$T/syms" | tr -d ' ')
awk '$1 ~ /^[TDBSRC]$/ && $2 !~ /^tld_/ {print "global " $2}' "$T/syms" > "$T/badsym"
awk '$1 ~ /^[tdbsrc]$/ && $2 !~ /^tld__/ && $2 !~ /[.$]/ && $2 !~ /^(l_|ltmp|L)/ {print "static " $2}' "$T/syms" >> "$T/badsym"
if [ -s "$T/badsym" ]; then bad "symbols outside tld_ / tld__:"; sed 's/^/        /' "$T/badsym"; else echo "ok    $n symbols: globals tld_, statics tld__"; fi

# macros
macros() { $CC -std=c11 -dM -E -I"$SRC" "$1" | awk '$1 == "#define" {sub(/\(.*/, "", $2); print $2}' | sort -u; }
for view in use impl; do
        macros "$T/sys_$view.c" > "$T/m_sys_$view"
        macros "$T/$view.c" | comm -23 - "$T/m_sys_$view" > "$T/m_$view"
        grep -vE "^(TLD_|TLD__)" "$T/m_$view" | grep -vE "$IDIOM" > "$T/badmac_$view"
        if [ -s "$T/badmac_$view" ]; then bad "macros outside TLD_ in the $view view:"; sed 's/^/        /' "$T/badmac_$view"
        else echo "ok    $(wc -l < "$T/m_$view" | tr -d ' ') macros in the $view view: TLD_, TLD__ or idiom"; fi
done
# TLD__FEATURES and TLD__IMPLEMENTED stay defined on purpose (they guard the feature
# macros and the implementation against a second inclusion)
grep -E '^TLD__' "$T/m_impl" | comm -23 - "$T/m_use" | grep -vE '^TLD__(FEATURES|IMPLEMENTED)$' > "$T/leak"
if [ -s "$T/leak" ]; then bad "implementation-only macros not #undef'd:"; sed 's/^/        /' "$T/leak"; else echo "ok    implementation-only macros are #undef'd"; fi

# types, constants, variables and functions from the manual's index
if [ -f "$SRC/docs/tld.md" ]; then
        sed -n 's/^| \[`\([^`]*\)`\]([^)]*) | \([a-z]*\) |.*/\1 \2/p' "$SRC/docs/tld.md" > "$T/index"
        awk '($2 == "macro" || $2 == "constant") && $1 !~ /^TLD_[A-Z0-9_]+$/ {print}
             ($2 == "type" || $2 == "function" || $2 == "variable") && $1 !~ /^tld_[a-z0-9_]+$/ {print}' "$T/index" |
                grep -vE "^($(echo "$IDIOM" | tr -d '^$()')) " > "$T/badidx"
        if [ -s "$T/badidx" ]; then bad "documented names outside the rules:"; sed 's/^/        /' "$T/badidx"
        else echo "ok    $(wc -l < "$T/index" | tr -d ' ') documented names: tld_ / TLD_ or idiom"; fi
fi

# a program reusing common names
if $CC -std=c11 -Wall -Wextra -Werror -I"$SRC" "$SRC/tests/names/clash.c" -o "$T/clash" -lm -lpthread >"$T/clash.err" 2>&1 && "$T/clash"; then
        echo "ok    a program may define next, search, Parser, NODE_ROOT, AT, EXTERN, ..."
else
        bad "tests/names/clash.c:"; head -20 "$T/clash.err"
fi
exit $fail
