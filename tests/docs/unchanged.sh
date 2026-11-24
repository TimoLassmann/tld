#!/bin/sh
# tests/docs/unchanged.sh TLDDOC SRC_DIR WORK_DIR
# A docs run that changes nothing writes nothing, and one that changes a file replaces it
# whole. tlddoc reads ROOT/tld.h, ROOT/tld_ai.h and ROOT/tld_doc.h and writes
# ROOT/docs/tld.md, ROOT/docs/tld_ai.md, ROOT/docs/tld_doc.md and the corpus block in
# ROOT/tld_doc.h. This test copies the three headers to WORK_DIR/root and runs tlddoc
# there only (never on SRC_DIR):
#   1. a second run reports each of the four outputs as unchanged and touches no file or
#      directory (cmp against a copy, and find -newer a marker file);
#   2. after a doc comment in the copy's tld.h and one in its tld_doc.h are edited, the
#      run rewrites tld.md, tld_doc.md and tld_doc.h (keeping its mode, set to 0640 for
#      the test), the edits show in them, --check passes and no temporary file is left;
#   3. when the manuals' directory cannot be written (skipped for root, who can write
#      anything), the run fails with an ERROR line, leaves the manual as it was and
#      leaves no temporary file.
set -u
TLDDOC=$1
SRC=$2
W=$3
R=$W/root
OUTS="tld_doc.h docs/tld.md docs/tld_ai.md docs/tld_doc.md"
fail=0
bad() { echo "FAIL  $*"; fail=$((fail + 1)); }

# The files under the copy, which must be the three headers and the three manuals only.
files() { (cd "$R" && find . -type f | LC_ALL=C sort | tr '\n' ' '); }
EXPECT="./docs/tld.md ./docs/tld_ai.md ./docs/tld_doc.md ./tld.h ./tld_ai.h ./tld_doc.h "

rm -rf "$W"
mkdir -p "$R" "$W/snap/docs" || exit 1
for f in tld.h tld_ai.h tld_doc.h; do
        cp "$SRC/$f" "$R/$f" || exit 1
done

"$TLDDOC" "$R" >"$W/run1.out" 2>&1 || { bad "first run failed"; cat "$W/run1.out"; exit 1; }
"$TLDDOC" --check "$R" >"$W/check1.out" 2>&1 || { bad "--check after the first run"; cat "$W/check1.out"; }

# 1. A second run changes nothing.
for f in $OUTS; do
        cp "$R/$f" "$W/snap/$f" || exit 1
done
touch "$W/marker"
sleep 1         # a file system with one-second timestamps must see a rewrite as newer
"$TLDDOC" "$R" >"$W/run2.out" 2>&1 || { bad "second run failed"; cat "$W/run2.out"; }
for f in $OUTS; do
        grep -q "^tlddoc: unchanged $R/$f (" "$W/run2.out" || bad "second run: no 'unchanged' line for $f"
        cmp -s "$R/$f" "$W/snap/$f" || bad "second run: $f changed"
done
grep "wrote" "$W/run2.out" >/dev/null && bad "second run wrote: $(grep wrote "$W/run2.out")"
newer=$(find "$R" -newer "$W/marker")
[ -z "$newer" ] || bad "second run touched: $newer"
[ "$(files)" = "$EXPECT" ] || bad "second run: files are $(files)"

# 2. An edited doc comment rewrites the outputs that hold it, and nothing else.
n=$(grep -c 'TLD_STR(b): the contents of' "$R/tld.h")
[ "$n" = 1 ] || bad "the doc comment of TLD_STR to edit occurs $n times in tld.h (update this test)"
n=$(grep -c 'Free an answer from tld_doc_ask; NULL is ignored\.' "$R/tld_doc.h")
[ "$n" = 1 ] || bad "the doc comment of tld_doc_answer_free to edit occurs $n times in tld_doc.h"
sed 's/TLD_STR(b): the contents of/TLD_STR(b): the edited contents of/' "$R/tld.h" >"$W/tld.h.new" &&
        cat "$W/tld.h.new" >"$R/tld.h" || exit 1
sed 's/Free an answer from tld_doc_ask; NULL is ignored\./Free an edited answer from tld_doc_ask; NULL is ignored./' \
        "$R/tld_doc.h" >"$W/tld_doc.h.new" && cat "$W/tld_doc.h.new" >"$R/tld_doc.h" || exit 1
chmod 640 "$R/tld_doc.h" || exit 1
"$TLDDOC" "$R" >"$W/run3.out" 2>&1 || { bad "run after the edit failed"; cat "$W/run3.out"; }
for f in tld_doc.h docs/tld.md docs/tld_doc.md; do
        grep -q "^tlddoc: wrote $R/$f (" "$W/run3.out" || bad "run after the edit: no 'wrote' line for $f"
done
grep -q "^tlddoc: unchanged $R/docs/tld_ai.md (" "$W/run3.out" || bad "run after the edit: tld_ai.md not unchanged"
cmp -s "$R/docs/tld_ai.md" "$W/snap/docs/tld_ai.md" || bad "run after the edit: tld_ai.md changed"
grep -q 'TLD_STR(b): the edited contents of' "$R/docs/tld.md" || bad "the edit is not in docs/tld.md"
n=$(grep -c 'TLD_STR(b): the edited contents of' "$R/tld_doc.h")
[ "$n" = 1 ] || bad "tld_doc.h holds the edit of tld.h $n times, not once (in the record of TLD_STR)"
grep -q 'Free an edited answer from tld_doc_ask' "$R/docs/tld_doc.md" || bad "the edit is not in docs/tld_doc.md"
mode=$(ls -l "$R/tld_doc.h" | cut -c1-10)
[ "$mode" = "-rw-r-----" ] || bad "tld_doc.h has mode $mode after the rewrite, not -rw-r-----"
[ "$(files)" = "$EXPECT" ] || bad "run after the edit left: $(files)"
"$TLDDOC" --check "$R" >"$W/check3.out" 2>&1 || { bad "--check after the edit"; cat "$W/check3.out"; }

# 3. A manual that cannot be replaced stays as it was.
if [ "$(id -u)" != 0 ]; then
        cp "$R/docs/tld.md" "$W/snap/docs/tld.md" || exit 1
        sed 's/TLD_STR(b): the edited contents of/TLD_STR(b): the contents of/' "$R/tld.h" >"$W/tld.h.new" &&
                cat "$W/tld.h.new" >"$R/tld.h" || exit 1
        chmod 555 "$R/docs" || exit 1
        if "$TLDDOC" "$R" >"$W/run4.out" 2>&1; then
                bad "run into a read-only docs directory succeeded"
        fi
        chmod 755 "$R/docs"
        grep -q "ERROR" "$W/run4.out" || bad "run into a read-only docs directory: no ERROR line"
        cmp -s "$R/docs/tld.md" "$W/snap/docs/tld.md" || bad "read-only docs directory: tld.md changed"
        [ "$(files)" = "$EXPECT" ] || bad "read-only docs directory: files are $(files)"
fi

[ "$fail" = 0 ] || { echo "docs-unchanged: $fail checks failed (outputs in $W)"; exit 1; }
echo "docs-unchanged: a second run leaves all four outputs unchanged; an edit rewrites them whole"
