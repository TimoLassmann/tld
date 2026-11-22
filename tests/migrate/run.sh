#!/bin/sh
# tests/migrate/run.sh PYTHON SRC_DIR WORK_DIR: tools/migrate.py turns old.c into
# expected.c, a second run changes nothing, and the report flags what needs a person.
set -eu
PY=$1
SRC=$2
W=$3
mkdir -p "$W"
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
echo "migrate: ok"
