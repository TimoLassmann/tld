#!/bin/sh
# tests/migrate/run.sh PYTHON SRC_DIR WORK_DIR: tools/migrate.py turns old.c into
# expected.c, a second run changes nothing, and the report flags what needs a person.
set -eu
PY=$1
SRC=$2
W=$3
# The work file sits under a directory named build*: a file named on the command line is
# never skipped, wherever it lives.
rm -rf "$W"
mkdir -p "$W/build-x"
W=$W/build-x
cp "$SRC/tests/migrate/old.c" "$W/m.c"
"$PY" "$SRC/tools/migrate.py" --write "$W/m.c" > "$W/report1.txt"
diff -u "$SRC/tests/migrate/expected.c" "$W/m.c"
"$PY" "$SRC/tools/migrate.py" --write "$W/m.c" > "$W/report2.txt"
diff -u "$SRC/tests/migrate/expected.c" "$W/m.c"
grep -q "^# 1 files read, 0 changed" "$W/report2.txt"
for why in "draws from a generator" "opposite sense of OK/FAIL" "writes a tld_strbuf" \
           "tld_global_stop_run is gone" "debug printer removed" "declared here as a variable" \
           "installs no signal handlers"; do
        grep -q "$why" "$W/report1.txt" || { echo "report lacks: $why"; exit 1; }
done
# Walking a tree skips build* directories inside it, not the ones above it.
mkdir -p "$W/tree/build" "$W/tree/src"
cp "$SRC/tests/migrate/old.c" "$W/tree/build/a.c"
cp "$SRC/tests/migrate/old.c" "$W/tree/src/b.c"
"$PY" "$SRC/tools/migrate.py" "$W/tree" > "$W/report3.txt"
grep -q "^# 1 files read, 1 would change" "$W/report3.txt" || { echo "tree walk:"; tail -1 "$W/report3.txt"; exit 1; }
echo "migrate: ok"
