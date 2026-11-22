#!/bin/sh
# tests/fuzz/run.sh SECONDS BUILD_DIR: run every fuzz target for SECONDS, starting from
# the seeds in tests/fuzz/seeds/. Fails on the first crash, leak, overrun or undefined
# behaviour; the input that caused it is left in BUILD_DIR/fuzz-crash-<target>/.
set -u
T=$1
B=$2
HERE=$(cd "$(dirname "$0")" && pwd)
fail=0
for t in json template ai http; do
        corpus=$B/fuzz-corpus-$t
        crash=$B/fuzz-crash-$t
        mkdir -p "$corpus" "$crash"
        cp "$HERE/seeds/$t/"* "$corpus/"
        if "$B/tests/fuzz/fuzz-$t" "$corpus" -max_total_time="$T" -timeout=10 \
                -rss_limit_mb=2048 -close_fd_mask=3 -artifact_prefix="$crash/" \
                -print_final_stats=1 > "$B/fuzz-$t.log" 2>&1; then
                echo "ok    fuzz-$t: $(grep -m1 'number_of_executed_units' "$B/fuzz-$t.log" | sed 's/.*: //') inputs"
        else
                echo "FAIL  fuzz-$t (log: $B/fuzz-$t.log)"
                grep -E "ERROR|SUMMARY|#[0-9]+ " "$B/fuzz-$t.log" | head -20
                fail=1
        fi
done
exit $fail
