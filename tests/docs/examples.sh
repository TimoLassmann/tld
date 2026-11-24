#!/bin/sh
# tests/docs/examples.sh TLDDOC CC SRC_DIR WORK_DIR tld|ai|doc [CFLAGS]
# Every code example in the doc comments of tld.h (tld), tld_ai.h (ai) or tld_doc.h
# (doc) compiles, links and runs without failing or crashing. tlddoc --examples writes
# each example as its own program (WORK_DIR/tld-NN.c, WORK_DIR/tld_ai-NN.c,
# WORK_DIR/tld_doc-NN.c). CFLAGS are the build's own (a sanitizer build passes its
# -fsanitize flags).
#
# The tld_ai.h examples run against tests/ai/mock_ollama.py, a MOCK server that returns
# canned answers: this checks that the examples work as C programs, not what a real
# model answers. They never contact a real Ollama. tld_doc.h needs no server.
set -u
TLDDOC=$1
CC=$2
SRC=$3
W=$4
WHICH=$5
CFLAGS=${6:-}
case $WHICH in
tld) stem=tld; header=tld.h ;;
ai) stem=tld_ai; header=tld_ai.h ;;
doc) stem=tld_doc; header=tld_doc.h ;;
*) echo "usage: examples.sh TLDDOC CC SRC_DIR WORK_DIR tld|ai|doc"; exit 2 ;;
esac
MOCK_PID=""
cleanup() { [ -n "$MOCK_PID" ] && kill "$MOCK_PID" 2>/dev/null; wait 2>/dev/null; }
trap cleanup EXIT INT TERM

rm -rf "$W"
mkdir -p "$W"
"$TLDDOC" --examples "$W" "$SRC" >/dev/null || exit 1

F="$CFLAGS -std=c11 -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-but-set-variable \
   -Wno-unused-label -Wno-unused-function -I$SRC"
if [ "$WHICH" = tld ]; then
        printf '#define TLD_IMPLEMENTATION\n#include "tld.h"\n' >"$W/impl.c"
elif [ "$WHICH" = ai ]; then
        printf '#define TLD_IMPLEMENTATION\n#define AI_IMPLEMENTATION\n#include "tld_ai.h"\n' >"$W/impl.c"
else
        printf '#define TLD_IMPLEMENTATION\n#define TLD_DOC_IMPLEMENTATION\n#include "tld_doc.h"\n' >"$W/impl.c"
fi
# shellcheck disable=SC2086
$CC $CFLAGS -std=c11 -I"$SRC" -c "$W/impl.c" -o "$W/impl.o" || exit 1

if [ "$WHICH" = ai ]; then
        # The mock's interpreter: uv's python when uv is installed, else python3 (as in
        # tests/ai/run.sh).
        PY=""
        if command -v uv >/dev/null 2>&1; then
                PY=$(uv run --no-project python -c 'import sys; print(sys.executable)' 2>/dev/null)
        fi
        [ -n "$PY" ] || PY=$(command -v python3)
        [ -n "$PY" ] || { echo "FAIL  no python for the mock server"; exit 1; }
        PORT=$("$PY" -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1])')
        "$PY" "$SRC/tests/ai/mock_ollama.py" "$PORT" 2>"$W/mock.log" &
        MOCK_PID=$!
        "$PY" - "$PORT" <<'EOF' || { echo "FAIL  mock server did not start"; exit 1; }
import sys, time, urllib.request
for _ in range(100):
    try:
        urllib.request.urlopen("http://127.0.0.1:%s/api/version" % sys.argv[1], timeout=1)
        sys.exit(0)
    except Exception:
        time.sleep(0.05)
sys.exit(1)
EOF
        export TLD_AI_HOST=http://127.0.0.1:$PORT TLD_AI_DB=off TLD_AI_REVIEWS=$W/reviews
        unset TLD_AI_LLM TLD_AI_DECIDER TLD_AI_EMBEDDER TLD_AI_NUM_CTX TLD_AI_TIMEOUT TLD_AI_FRESH
        echo "NOTE  the tld_ai.h examples run against a MOCK server (tests/ai/mock_ollama.py,"
        echo "      canned answers at $TLD_AI_HOST), not a real Ollama or model"
fi

n=0
fail=0
for f in "$W/$stem"-*.c; do
        [ -f "$f" ] || continue
        n=$((n + 1))
        what=$(head -1 "$f" | sed 's|^/\* Example from the doc comment of \(.*\) (tlddoc.*|\1|')
        # shellcheck disable=SC2086
        if ! $CC $F "$f" "$W/impl.o" -o "${f%.c}" -lm -lpthread 2>"${f%.c}.err"; then
                echo "FAIL  $what: does not compile ($f)"
                sed 's/^/      /' "${f%.c}.err" | head -20
                fail=$((fail + 1))
        elif ! "${f%.c}" >"${f%.c}.out" 2>&1; then
                echo "FAIL  $what: the program failed or crashed ($f)"
                sed 's/^/      /' "${f%.c}.out" | head -20
                fail=$((fail + 1))
        fi
done
[ "$n" -gt 0 ] || { echo "FAIL  no examples found in $header"; exit 1; }
if [ "$WHICH" = ai ]; then
        calls=$(grep -c '^mock: ' "$W/mock.log")
        suffix=" against the MOCK server ($calls model calls answered with canned replies)"
        # the examples ignore a failed call, so a mock nobody reached must fail the test
        [ "$calls" -gt 0 ] || { echo "FAIL  no example reached the mock server"; fail=$((fail + 1)); }
else
        suffix=""
fi
[ "$fail" = 0 ] || { echo "doc-examples: $fail of $n $header examples failed$suffix"; exit 1; }
echo "doc-examples: all $n $header examples compile and run$suffix"
