#!/bin/sh
# tests/names/check.sh CC SRC_DIR [NM]: every name tld.h gives a program follows the
# naming rules, so it cannot collide with the program's own names:
#   global symbols                     tld_...   (internals tld__...)
#   static symbols                     tld__...
#   macros (user view and TLD_IMPLEMENTATION view)
#                                      TLD_... / TLD__..., galloc, gfree, or one of the
#                                      idiom names OK FAIL RUN ASSERT *_MSG MMALLOC ...
#   types, constants and variables     tld_... / TLD_... (from the index of docs/tld.md)
# the same rules for what tld_doc.h adds to tld.h, with the prefixes
# tld_doc_ / tld__doc_ / TLD_DOC_ / TLD__DOC_ (index of docs/tld_doc.md),
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
# The SIMD headers of the bpm module are included only on their own architecture, so
# they keep the same conditions here as in src/bpm/tld-bpm.c.
grep -h '^[[:space:]]*#[[:space:]]*include[[:space:]]*<' "$SRC/tld.h" | grep -vE '<(immintrin|arm_neon)\.h>' |
        sort -u > "$T/sys_use.c"
cat >> "$T/sys_use.c" <<'EOF'
#if (defined(__x86_64__) || defined(__amd64__)) && (defined(__GNUC__) || defined(__clang__))
#include <immintrin.h>
#elif defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
#endif
EOF
{ printf '#define _POSIX_C_SOURCE 200809L\n#define _DEFAULT_SOURCE\n#define _DARWIN_C_SOURCE\n'
  cat "$T/sys_use.c"; } > "$T/sys_impl.c"

# symbols
$CC -std=c11 -c -I"$SRC" "$T/impl.c" -o "$T/impl.o" || { bad "tld.h implementation does not compile"; exit 1; }
$NM "$T/impl.o" | awk 'NF == 3 && $2 ~ /^[TDBSRCtdbsrc]$/ {print $2, $3}' | sed 's/ _/ /' > "$T/syms"
n=$(wc -l < "$T/syms" | tr -d ' ')
awk '$1 ~ /^[TDBSRC]$/ && $2 !~ /^tld_/ {print "global " $2}' "$T/syms" > "$T/badsym"
# a static whose name starts with __ (still _ after the strip above) is the C library's,
# e.g. musl's math.h inline helpers __DOUBLE_BITS / __FLOAT_BITS when not inlined
awk '$1 ~ /^[tdbsrc]$/ && $2 !~ /^tld__/ && $2 !~ /[.$]/ && $2 !~ /^(l_|ltmp|L|_)/ {print "static " $2}' "$T/syms" >> "$T/badsym"
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

# tld_doc.h: what it adds to tld.h follows the same rules with its own
# prefix: global symbols tld_doc_ (internals tld__doc_), statics tld__doc_, macros
# TLD_DOC_ / TLD__DOC_ (implementation-only ones #undef'd, except TLD__DOC_IMPLEMENTED),
# and the names in the index of docs/tld_doc.md tld_doc_ / TLD_DOC_.
if [ -f "$SRC/tld_doc.h" ]; then
        printf '#define TLD_DOC_IMPLEMENTATION\n#include "tld_doc.h"\n' > "$T/doc_impl.c"
        printf '#include "tld_doc.h"\n' > "$T/doc_use.c"
        printf '#include "tld.h"\n' > "$T/doc_base.c"
        printf '#define TLD_IMPLEMENTATION\n#include "tld.h"\n' > "$T/doc_base_impl.c"
        printf '#define TLD_IMPLEMENTATION\n#define TLD_DOC_IMPLEMENTATION\n#include "tld_doc.h"\n' > "$T/doc_all_impl.c"
        if $CC -std=c11 -c -I"$SRC" "$T/doc_impl.c" -o "$T/doc_impl.o" &&
           $CC -std=c11 -c -I"$SRC" "$T/doc_base.c" -o "$T/doc_base.o"; then
                syms() { $NM "$1" | awk 'NF == 3 && $2 ~ /^[TDBSRCtdbsrc]$/ {print $2, $3}' | sed 's/ _/ /' | sort -u; }
                syms "$T/doc_base.o" > "$T/dsyms_base"
                syms "$T/doc_impl.o" | comm -23 - "$T/dsyms_base" > "$T/dsyms"
                awk '$1 ~ /^[TDBSRC]$/ && $2 !~ /^tld_doc_/ && $2 !~ /^tld__doc_/ {print "global " $2}' "$T/dsyms" > "$T/dbadsym"
                awk '$1 ~ /^[tdbsrc]$/ && $2 !~ /^tld__doc_/ && $2 !~ /[.$]/ && $2 !~ /^(l_|ltmp|L|_)/ {print "static " $2}' "$T/dsyms" >> "$T/dbadsym"
                if [ -s "$T/dbadsym" ]; then bad "tld_doc.h symbols outside tld_doc_ / tld__doc_:"; sed 's/^/        /' "$T/dbadsym"
                else echo "ok    tld_doc.h: $(grep -c " tld" "$T/dsyms") symbols: globals tld_doc_, statics tld__doc_"; fi
        else
                bad "tld_doc.h implementation does not compile"
        fi
        macros "$T/doc_base.c" > "$T/dm_base_use"
        macros "$T/doc_base_impl.c" > "$T/dm_base_impl"
        macros "$T/doc_use.c" | comm -23 - "$T/dm_base_use" > "$T/dm_use"
        macros "$T/doc_all_impl.c" | comm -23 - "$T/dm_base_impl" > "$T/dm_impl"
        for view in use impl; do
                grep -vE '^(TLD_DOC_|TLD__DOC_)' "$T/dm_$view" > "$T/dbadmac_$view"
                if [ -s "$T/dbadmac_$view" ]; then bad "tld_doc.h macros outside TLD_DOC_ in the $view view:"; sed 's/^/        /' "$T/dbadmac_$view"
                else echo "ok    tld_doc.h: $(wc -l < "$T/dm_$view" | tr -d ' ') macros in the $view view: TLD_DOC_ or TLD__DOC_"; fi
        done
        comm -23 "$T/dm_impl" "$T/dm_use" | grep -E '^TLD__DOC_' | grep -vE '^TLD__DOC_IMPLEMENTED$' > "$T/dleak"
        if [ -s "$T/dleak" ]; then bad "tld_doc.h implementation-only macros not #undef'd:"; sed 's/^/        /' "$T/dleak"
        else echo "ok    tld_doc.h: implementation-only macros are #undef'd"; fi
        if [ -f "$SRC/docs/tld_doc.md" ]; then
                sed -n 's/^| \[`\([^`]*\)`\]([^)]*) | \([a-z]*\) |.*/\1 \2/p' "$SRC/docs/tld_doc.md" > "$T/dindex"
                awk '($2 == "macro" || $2 == "constant") && $1 !~ /^TLD_DOC_[A-Z0-9_]+$/ {print}
                     ($2 == "type" || $2 == "function" || $2 == "variable") && $1 !~ /^tld_doc_[a-z0-9_]+$/ {print}' "$T/dindex" > "$T/dbadidx"
                if [ ! -s "$T/dindex" ]; then bad "no index entries in docs/tld_doc.md"
                elif [ -s "$T/dbadidx" ]; then bad "tld_doc.h documented names outside the rules:"; sed 's/^/        /' "$T/dbadidx"
                else echo "ok    tld_doc.h: $(wc -l < "$T/dindex" | tr -d ' ') documented names: tld_doc_ / TLD_DOC_"; fi
        fi
fi

# a program reusing common names
if $CC -std=c11 -Wall -Wextra -Werror -I"$SRC" "$SRC/tests/names/clash.c" -o "$T/clash" -lm -lpthread >"$T/clash.err" 2>&1 && "$T/clash"; then
        echo "ok    a program may define next, search, Parser, NODE_ROOT, AT, EXTERN, ..."
else
        bad "tests/names/clash.c:"; head -20 "$T/clash.err"
fi
exit $fail
