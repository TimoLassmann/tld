#!/bin/sh
# tests/misuse/check.sh CC SRC_DIR: the compiler rejects the mistakes the library is
# designed to catch. err_*.c must fail to compile with no warning flags at all,
# warn_*.c must fail with -Wall -Werror, and ok_usage.c (the correct forms) must
# compile cleanly with -Wall -Wextra -Werror.
set -u
CC=$1
SRC=$2
D=$SRC/tests/misuse
fail=0
try() { $CC -std=gnu11 "$@" -fsyntax-only -I"$SRC" >/dev/null 2>&1; }
for f in "$D"/err_*.c; do
        if try "$f"; then echo "FAIL  $(basename "$f") compiled"; fail=1; else echo "ok    $(basename "$f") rejected"; fi
done
for f in "$D"/warn_*.c; do
        if try -Wall -Werror "$f"; then echo "FAIL  $(basename "$f") compiled with -Wall -Werror"; fail=1; else echo "ok    $(basename "$f") rejected"; fi
done
if try -Wall -Wextra -Werror "$D/ok_usage.c"; then echo "ok    ok_usage.c compiles cleanly"; else
        echo "FAIL  ok_usage.c does not compile cleanly:"; $CC -std=gnu11 -Wall -Wextra -Werror -fsyntax-only -I"$SRC" "$D/ok_usage.c"; fail=1; fi
exit $fail
