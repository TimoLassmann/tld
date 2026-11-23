#!/bin/sh
# tests/ai/compile.sh CC OUT_DIR - tld_ai.h compiles with -std=c11 -Wall -Wextra -pedantic
# without a single warning: two TUs (implementations + declarations only), and the
# implementation after strict-mode system headers. Then runs them against a closed
# port (no server is ever contacted).
set -u
CC=$1
OUT=$2
F="-std=c11 -Wall -Wextra -pedantic -Werror -I."
mkdir -p "$OUT"
fail=0
$CC $F tests/ai/compile_impl.c tests/ai/compile_use.c -o "$OUT/ai-compile-two" -lm -lpthread || fail=1
$CC $F tests/ai/compile_early.c tests/ai/compile_tld.c -o "$OUT/ai-compile-early" -lm -lpthread || fail=1
[ $fail = 0 ] || { echo "FAIL  tld_ai.h does not compile cleanly"; exit 1; }
# A port nothing listens on (python3, which the mock-server tests need anyway).
PORT=$(python3 -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1])')
export TLD_AI_HOST=http://127.0.0.1:$PORT TLD_AI_DB=off TLD_AI_REVIEWS=$OUT/ai-compile-reviews
"$OUT/ai-compile-two" && "$OUT/ai-compile-early" || { echo "FAIL  compiled programs"; exit 1; }
echo "PASS  tld_ai.h compiles warning-free (two TUs; after strict system headers)"
