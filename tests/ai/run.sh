#!/bin/sh
# tests/ai/run.sh GROUP BIN_DIR
# tld_ai.h tests against tests/ai/mock_ollama.py, which returns canned answers in the
# documented Ollama shapes. They check requests, caching, replay, review, threads,
# options, the HTTP client and ai_defaults; they say nothing about how real models
# answer. Run from the repository root (ctest does). Never contacts a real
# Ollama: every program gets TLD_AI_HOST pointing at a mock on a random free port, or at a
# closed port.
#
# GROUP: plumbing noserver http json cache defaults sanitize tsan all
set -u
GROUP=${1:-all}
B=${2:-build/tests}
T=$(mktemp -d "${TMPDIR:-/tmp}/ai-test.XXXXXX")
FAIL=0
PIDS=""

cleanup() {
    for p in $PIDS; do kill "$p" 2>/dev/null; done
    wait 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

# The mock's interpreter: uv's python when uv is installed, else python3. Resolved to
# a path once, so the mock runs as a direct child that kill reaches.
if command -v uv >/dev/null 2>&1; then
    PY=$(uv run --no-project python -c 'import sys; print(sys.executable)' 2>/dev/null)
fi
[ -n "${PY:-}" ] || PY=$(command -v python3)
[ -n "$PY" ] || { echo "FAIL  no python for the mock server"; exit 1; }

# Safety net: nothing below may reach a real server by accident.
CLOSED=$("$PY" -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1])')
export TLD_AI_HOST=http://127.0.0.1:$CLOSED TLD_AI_DB=$T/default.cache TLD_AI_REVIEWS=$T/reviews
unset TLD_AI_LLM TLD_AI_DECIDER TLD_AI_EMBEDDER TLD_AI_NUM_CTX TLD_AI_TIMEOUT TLD_AI_FRESH

free_port() { "$PY" -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1])'; }

# start_mock NAME [mock args...] -> sets PORT and HOST; the log is $T/NAME.log
start_mock() {
    name=$1
    shift
    PORT=$(free_port)
    HOST=http://127.0.0.1:$PORT
    "$PY" tests/ai/mock_ollama.py "$PORT" "$@" 2>"$T/$name.log" &
    PIDS="$PIDS $!"
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
}
reqs() { grep -c '^mock: ' "$T/$1.log"; }
count() { grep -c "$1" "$T/$2.log"; }
check() { # check NAME RESULT(0 = pass) DETAIL
    if [ "$2" = 0 ]; then echo "PASS  $1"; else echo "FAIL  $1: $3"; FAIL=1; fi
}
# limit SECONDS cmd... : kill the command if it runs longer (exit 142)
limit() { s=$1; shift; perl -e 'alarm shift; exec @ARGV or exit 127' "$s" "$@"; }

group_plumbing() {
    start_mock a 0.93
    export TLD_AI_HOST=$HOST
    TLD_AI_DB=$T/a.cache limit 60 "$B/ai-triage" tests/ai/ticket.txt >"$T/out1.txt"; rc=$?
    n1=$(reqs a)
    [ $rc = 0 ] && [ "$n1" = 5 ]; check "triage first run sends 5 requests" $? "rc=$rc requests=$n1"

    TLD_AI_DB=$T/a.cache limit 60 "$B/ai-triage" tests/ai/ticket.txt >"$T/out2.txt"
    n2=$(reqs a)
    [ "$n2" = "$n1" ] && cmp -s "$T/out1.txt" "$T/out2.txt"
    check "triage rerun replays from cache" $? "new requests=$((n2 - n1))"

    TLD_AI_DB=$T/a.cache TLD_AI_FRESH=On limit 60 "$B/ai-triage" tests/ai/ticket.txt >/dev/null
    n3=$(reqs a)
    [ "$n3" -gt "$n2" ]; check "TLD_AI_FRESH=On bypasses the cache" $? "no new requests"

    TLD_AI_DB=$T/b.cache limit 60 "$B/ai-triage" tests/ai/t2.txt >"$T/out3.txt"
    grep -q 'Mock reply' "$T/out3.txt"; check "billing branch returns text" $? "$(cat "$T/out3.txt")"

    TLD_AI_DB=$T/o.cache limit 60 "$B/ai-opts" tests/ai/img.png >"$T/opts.txt"; rc=$?
    "$PY" - "$T/o.cache" <<'EOF'
import json, sys
r = json.loads(open(sys.argv[1]).readline())["request"]
o = r.get("options", {})
ok = (r["model"] == "inkling-2-mini" and r["messages"][0] == {"role": "system", "content": "Be brief."}
      and r["messages"][1].get("images") == ["iVBORyBmYWtl"] and r["stream"] is False
      and o.get("temperature") == 0 and o.get("seed") == 7 and o.get("stop") == ["END"]
      and o.get("repeat_penalty") == 1.1 and r.get("think") == "high" and r.get("keep_alive") == "10m"
      and open(sys.argv[1]).readline().count('"seed"') == 1)
sys.exit(0 if ok else 1)
EOF
    [ $? = 0 ] && [ $rc = 0 ] && grep -q 'tokens 40/12 cache 0' "$T/opts.txt"
    check "all options reach the request body" $? "rc=$rc $(cat "$T/opts.txt")"

    TLD_AI_DB=$T/c.cache TLD_AI_NUM_CTX=16384 limit 60 "$B/ai-par" 4 >/dev/null
    "$PY" - "$T/c.cache" <<'EOF'
import json, sys
seen = {}
for line in open(sys.argv[1]):
    e = json.loads(line)
    seen.setdefault(e["endpoint"], set()).add(e["request"].get("options", {}).get("num_ctx"))
ok = seen["/api/chat"] == {16384} and seen["/api/embed"] == {16384} and seen["/v1/systemone"] == {None}
sys.exit(0 if ok else 1)
EOF
    check "TLD_AI_NUM_CTX goes to chat and embed, not decisions" $? "unexpected num_ctx values"

    start_mock p 0.93
    export TLD_AI_HOST=$HOST
    TLD_AI_DB=$T/p.cache limit 60 "$B/ai-par" 8 >"$T/par.txt"
    p1=$(reqs p)
    grep -q '16/16 ok' "$T/par.txt" && [ "$p1" = 34 ]
    check "par: 16 parallel jobs, 34 requests" $? "requests=$p1, $(head -1 "$T/par.txt")"
    TLD_AI_DB=$T/p.cache limit 60 "$B/ai-par" 8 >/dev/null
    p2=$(reqs p)
    [ "$p2" = "$p1" ]; check "par rerun on 8 threads replays from cache" $? "new requests=$((p2 - p1))"
    grep -q 'dim 8, nearest: \[Variant' "$T/par.txt"
    check "embeddings and nearest neighbours" $? "$(sed -n 2p "$T/par.txt")"
    echo "      timing: $(head -1 "$T/par.txt")"

    start_mock r 0.55
    export TLD_AI_HOST=$HOST
    TLD_AI_DB=$T/r.cache TLD_AI_REVIEWS=$T/rev limit 60 "$B/ai-triage" tests/ai/ticket.txt >/dev/null 2>"$T/rev1.txt"; rc=$?
    f=$(ls "$T"/rev/*.json 2>/dev/null | head -1)
    [ $rc = 3 ] && [ -n "$f" ] && grep -q '"status": "open"' "$f"
    check "low confidence opens a review file (exit 3)" $? "rc=$rc file=$f"
    TLD_AI_DB=$T/r.cache TLD_AI_REVIEWS=$T/rev limit 60 "$B/ai-triage" tests/ai/ticket.txt >/dev/null 2>&1; rc=$?
    [ $rc = 3 ]; check "an open review stays open on rerun" $? "rc=$rc"
    "$PY" - "$f" <<'EOF'
import json, sys
r = json.load(open(sys.argv[1]))
assert r["what"] and r["payload"] and r["answer"] is None
r["status"], r["answer"] = "done", "billing"
json.dump(r, open(sys.argv[1], "w"), indent=2)
EOF
    before=$(reqs r)
    TLD_AI_DB=$T/r.cache TLD_AI_REVIEWS=$T/rev limit 60 "$B/ai-triage" tests/ai/ticket.txt >"$T/rev.txt" 2>&1; rc=$?
    after=$(reqs r)
    [ $rc = 0 ] && grep -q 'reviewer chose billing' "$T/rev.txt" && [ "$after" = "$before" ]
    check "answered review resumes with no new requests" $? "rc=$rc new requests=$((after - before))"
}

group_noserver() {
    TLD_AI_HOST=http://127.0.0.1:$CLOSED limit 20 "$B/ai-apis" AI_ENOSERVER 3 >"$T/ns.txt"; rc=$?
    [ $rc = 0 ]; check "no server: every API fails fast with AI_ENOSERVER" $? "rc=$rc
$(cat "$T/ns.txt")"
    TLD_AI_HOST=http://127.0.0.1:$CLOSED limit 20 "$B/ai-text" hi 2>"$T/ns2.txt"; rc=$?
    [ $rc = 1 ] && grep -q 'AI_ENOSERVER: no Ollama server' "$T/ns2.txt"
    check "no server: the message names the problem" $? "$(cat "$T/ns2.txt")"
    TLD_AI_HOST=http://127.0.0.1:$CLOSED TLD_AI_TIMEOUT=5s TLD_AI_NUM_CTX=12x TLD_AI_FRESH=maybe \
        limit 20 "$B/ai-text" hi 2>"$T/env.txt"; rc=$?
    [ $rc = 1 ] && grep -q 'ignoring TLD_AI_TIMEOUT=5s' "$T/env.txt" &&
        grep -q 'ignoring TLD_AI_NUM_CTX=12x' "$T/env.txt" && grep -q 'ignoring TLD_AI_FRESH=maybe' "$T/env.txt"
    check "environment values that do not parse are ignored with a warning" $? "$(cat "$T/env.txt")"
}

group_http() {
    start_mock ch 0.93 --chunked
    TLD_AI_HOST=$HOST TLD_AI_DB=$T/ch.cache limit 60 "$B/ai-triage" tests/ai/ticket.txt >"$T/ch1.txt"; rc=$?
    [ $rc = 0 ] && [ "$(reqs ch)" = 5 ] && grep -q '^fix: mock title' "$T/ch1.txt"
    check "chunked replies: triage" $? "rc=$rc $(cat "$T/ch1.txt")"
    TLD_AI_HOST=$HOST TLD_AI_DB=$T/ch2.cache limit 60 "$B/ai-par" 8 >"$T/ch2.txt"; rc=$?
    [ $rc = 0 ] && grep -q '16/16 ok' "$T/ch2.txt" && grep -q 'dim 8, nearest: \[Variant' "$T/ch2.txt"
    check "chunked replies: par and embeddings" $? "rc=$rc $(cat "$T/ch2.txt")"

    start_mock slow 0.93 --slow 3
    t0=$(date +%s)
    TLD_AI_HOST=$HOST TLD_AI_TIMEOUT=1 limit 20 "$B/ai-text" hi 2>"$T/slow.txt"; rc=$?
    t1=$(date +%s)
    [ $rc = 1 ] && grep -q 'AI_EHTTP: .*timed out' "$T/slow.txt" && [ $((t1 - t0)) -le 2 ]
    check "TLD_AI_TIMEOUT=1: a slow reply fails with AI_EHTTP in time" $? "rc=$rc $((t1 - t0)) s $(cat "$T/slow.txt")"
    (unset TLD_AI_HOST; limit 20 "$B/ai-defaults" timeout "$HOST") >"$T/dslow.txt"; rc=$?
    [ $rc = 0 ]; check "ai_defaults timeout: a slow reply fails in time" $? "$(cat "$T/dslow.txt")"

    TLD_AI_HOST=https://127.0.0.1:$CLOSED limit 20 "$B/ai-apis" AI_EHTTP 3 >"$T/https.txt"; rc=$?
    [ $rc = 0 ] && grep -q 'https is not supported' "$T/https.txt"
    check "https host: every API fails with AI_EHTTP" $? "rc=$rc
$(cat "$T/https.txt")"

    start_mock nm 0.93
    TLD_AI_HOST=$HOST TLD_AI_LLM=not-installed limit 20 "$B/ai-text" hi 2>"$T/nm.txt"; rc=$?
    [ $rc = 1 ] && grep -q 'AI_ENOMODEL' "$T/nm.txt" && [ "$(reqs nm)" = 0 ]
    check "a model that is not installed gives AI_ENOMODEL" $? "$(cat "$T/nm.txt")"
}

group_json() {
    start_mock js 0.93
    TLD_AI_HOST=$HOST TLD_AI_DB=$T/js.cache limit 60 "$B/ai-special" "$T/sp" >"$T/sp.txt" 2>&1; rc=$?
    [ $rc = 0 ]; check "special characters round-trip through the reply" $? "$(cat "$T/sp.txt")"
    "$PY" - "$T/js.log" "$T/sp" <<'EOF'
import json, sys
prompt = open(sys.argv[2] + ".prompt", encoding="utf-8", newline="").read()
state = open(sys.argv[2] + ".state", encoding="utf-8", newline="").read()
reqs = [json.loads(l.split(" ", 2)[2]) for l in open(sys.argv[1]) if l.startswith("mock: ")]
chat = [r for r in reqs if "messages" in r]
dec = [r for r in reqs if "questions" in r]
assert len(chat) == 1 and len(dec) == 2, (len(chat), len(dec))
assert chat[0]["messages"][0]["content"] == prompt, "system prompt changed"
assert chat[0]["messages"][1]["content"] == prompt, "user prompt changed"
assert dec[0]["questions"]["q"]["instructions"] == prompt, "question changed"
assert dec[0]["state"] == {"ticket": 'réfund "now"', "n": 42, "ok": True}, dec[0]["state"]
assert dec[1]["state"] == state, "plain state changed"
EOF
    check "special characters reach the server intact" $? "logged requests differ"
}

group_cache() {
    start_mock tf 0.93 --fail-first /api/chat:1
    TLD_AI_HOST=$HOST TLD_AI_DB=$T/tf.cache limit 60 "$B/ai-triage" tests/ai/ticket.txt >"$T/tf1.txt" 2>&1; rc=$?
    n1=$(reqs tf)
    [ $rc = 0 ] && [ "$(count '^fail:' tf)" = 1 ] && [ "$n1" = 5 ]
    check "transient failure: the retry succeeds" $? "rc=$rc calls=$n1 $(cat "$T/tf1.txt")"
    TLD_AI_HOST=$HOST TLD_AI_DB=$T/tf.cache limit 60 "$B/ai-triage" tests/ai/ticket.txt >"$T/tf2.txt" 2>&1
    n2=$(reqs tf)
    [ "$n2" = "$n1" ] && cmp -s "$T/tf1.txt" "$T/tf2.txt"
    check "transient failure: rerun replays without new requests" $? "new requests=$((n2 - n1))"

    start_mock cc 0.93
    TLD_AI_HOST=$HOST TLD_AI_DB=$T/cc.cache limit 60 "$B/ai-par" 8 >"$T/cc1.txt" 2>&1 &
    a=$!
    TLD_AI_HOST=$HOST TLD_AI_DB=$T/cc.cache limit 60 "$B/ai-par" 8 >"$T/cc2.txt" 2>&1 &
    b=$!
    wait $a; ra=$?
    wait $b; rb=$?
    "$PY" - "$T/cc.cache" <<'EOF'
import json, sys
lines = open(sys.argv[1]).read().split("\n")
assert lines[-1] == "", "no final newline"
recs = [json.loads(l) for l in lines[:-1]]
assert len(recs) == 68, len(recs)
for r in recs:
    assert set(r) >= {"key", "endpoint", "model", "digest", "request", "response", "seconds", "created"}
    assert r["key"].count("#") == 1
EOF
    [ $? = 0 ] && [ $ra = 0 ] && [ $rb = 0 ]
    check "two processes append to one cache: 68 valid lines" $? "rc=$ra/$rb"
    before=$(reqs cc)
    TLD_AI_HOST=$HOST TLD_AI_DB=$T/cc.cache limit 60 "$B/ai-par" 8 >/dev/null 2>&1; rc=$?
    [ $rc = 0 ] && [ "$(reqs cc)" = "$before" ]
    check "the shared cache replays" $? "rc=$rc new requests=$(( $(reqs cc) - before ))"

    printf '{"key": "trunc' >>"$T/cc.cache"
    TLD_AI_HOST=$HOST TLD_AI_DB=$T/cc.cache limit 60 "$B/ai-par" 8 >/dev/null 2>"$T/dmg.txt"; rc=$?
    [ $rc = 0 ] && [ "$(reqs cc)" = "$before" ] && grep -q 'skipped 1 damaged line' "$T/dmg.txt"
    check "a damaged last line is skipped with a warning" $? "rc=$rc $(tail -3 "$T/dmg.txt")"
    TLD_AI_HOST=$HOST TLD_AI_DB=$T/cc.cache TLD_AI_FRESH=1 limit 60 "$B/ai-text" after >/dev/null 2>&1
    "$PY" -c "
import json,sys
ls=open(sys.argv[1]).read().split('\n')
json.loads(ls[-2])" "$T/cc.cache"
    check "an append after a damaged line starts on a new line" $? "last line not valid"
}

group_defaults() {
    start_mock d1 0.93
    (unset TLD_AI_HOST TLD_AI_DB TLD_AI_REVIEWS
     limit 60 "$B/ai-defaults" set "$HOST" "$T/d1.cache" "$T/d1rev" defaults) >"$T/d1.txt" 2>"$T/d1.err"; rc=$?
    [ $rc = 0 ]; check "ai_defaults: calls and validation" $? "rc=$rc
$(grep -v '^ok' "$T/d1.txt") $(cat "$T/d1.err")"
    "$PY" - "$T/d1.log" defaults <<'EOF'
import json, sys
reqs = [(l.split(" ", 2)[1], json.loads(l.split(" ", 2)[2])) for l in open(sys.argv[1]) if l.startswith("mock: ")]
def models(ep): return [r["model"] for p, r in reqs if p == ep]
env = sys.argv[2] == "env"
llm, dec, emb = ("inkling-2-mini:latest", "autotrust-jev:latest", "embeddinggemma-2:latest") if env \
    else ("alt-llm", "alt-decider", "alt-embedder")
assert models("/api/chat") == [llm, "other-model", llm], models("/api/chat")
assert models("/v1/systemone") == [dec, dec, "autotrust-jev"], models("/v1/systemone")
assert models("/api/embed") == [emb, "embeddinggemma-2", "embeddinggemma-2"], models("/api/embed")
for p, r in reqs:
    if p == "/api/chat":
        o = r["options"]
        assert r["messages"][0] == {"role": "system", "content": "DEFAULT-SYS"}, r
        assert o["seed"] == 9 and o["stop"] == ["DEFAULT-STOP"] and r["keep_alive"] == "5m", r
        assert o["temperature"] == (0.25 if r["model"] == llm else 0.75), r
    else:
        assert "options" not in r and "keep_alive" not in r and "system" not in r, r
    if p == "/api/embed":
        assert r["dimensions"] == 64, r
EOF
    check "ai_defaults: models per endpoint, opts only in chat, embed_dimensions in embed" $? "logged requests differ"
    [ -s "$T/d1.cache" ] && ls "$T"/d1rev/*.json >/dev/null 2>&1 && [ ! -e "$T/default.cache" ]
    check "ai_defaults: cache path and review directory" $? "files missing"

    start_mock d2 0.93
    TLD_AI_HOST=$HOST TLD_AI_LLM=inkling-2-mini:latest TLD_AI_DECIDER=autotrust-jev:latest \
        TLD_AI_EMBEDDER=embeddinggemma-2:latest TLD_AI_DB=$T/d2.cache TLD_AI_REVIEWS=$T/d2rev TLD_AI_TIMEOUT=5 \
        limit 60 "$B/ai-defaults" set "http://127.0.0.1:$CLOSED" "$T/d2.cache" "$T/d2rev" env >"$T/d2.txt" 2>"$T/d2.err"; rc=$?
    [ $rc = 0 ]; check "environment overrides ai_defaults (calls and validation)" $? "rc=$rc
$(grep -v '^ok' "$T/d2.txt") $(cat "$T/d2.err")"
    "$PY" - "$T/d2.log" env <<'EOF'
import json, sys
reqs = [(l.split(" ", 2)[1], json.loads(l.split(" ", 2)[2])) for l in open(sys.argv[1]) if l.startswith("mock: ")]
def models(ep): return [r["model"] for p, r in reqs if p == ep]
assert models("/api/chat") == ["inkling-2-mini:latest", "other-model", "inkling-2-mini:latest"], models("/api/chat")
assert models("/v1/systemone") == ["autotrust-jev:latest"] * 2 + ["autotrust-jev"], models("/v1/systemone")
assert models("/api/embed") == ["embeddinggemma-2:latest", "embeddinggemma-2", "embeddinggemma-2"]
EOF
    [ $? = 0 ] && [ -s "$T/d2.cache" ] && ls "$T"/d2rev/*.json >/dev/null 2>&1 && [ ! -e "$T/d2.cache.unused" ] && [ ! -e "$T/d2rev.unused" ]
    check "environment overrides ai_defaults (host, models, cache, reviews)" $? "logged requests or files differ"

    start_mock d4 0.93
    TLD_AI_HOST=$HOST TLD_AI_DB=$T/d4.cache limit 60 "$B/ai-defaults" reset "$HOST" >"$T/d4.txt" 2>&1; rc=$?
    [ $rc = 0 ] && "$PY" - "$T/d4.log" <<'EOF'
import json, sys
reqs = [(l.split(" ", 2)[1], json.loads(l.split(" ", 2)[2])) for l in open(sys.argv[1]) if l.startswith("mock: ")]
chat = [(r["model"], r["messages"][0]["content"] if r["messages"][0]["role"] == "system" else None)
        for p, r in reqs if p == "/api/chat"]
emb = [(r["model"], r.get("dimensions")) for p, r in reqs if p == "/api/embed"]
assert chat == [("alt-llm", "RESET-SYS"), ("inkling-2-mini:latest", None),
                ("inkling-2-mini:latest", None)], chat
assert emb == [("embeddinggemma-2:latest", 64), ("embeddinggemma-2:latest", None)], emb
EOF
    check "ai_defaults: a later call replaces the defaults, NULL resets" $? "rc=$rc $(grep -v '^ok' "$T/d4.txt")"

    start_mock d3 0.93
    (unset TLD_AI_HOST; limit 60 "$B/ai-defaults" stress "$HOST") >"$T/d3.txt" 2>&1; rc=$?
    [ $rc = 0 ]; check "defaults changing while 8 threads call" $? "rc=$rc $(cat "$T/d3.txt")"

    limit 20 "$B/ai-defaults" afterexit >"$T/d5.txt" 2>&1; rc=$?
    [ $rc = 0 ]; check "calls after the exit cleanup fail cleanly" $? "rc=$rc $(cat "$T/d5.txt")"
}

group_sanitize() { # the examples as built (an ASAN build type adds the sanitizers)
    start_mock sa 0.93
    export TLD_AI_HOST=$HOST
    for run in first replay; do
        TLD_AI_DB=$T/sa.cache limit 120 "$B/ai-triage" tests/ai/ticket.txt >/dev/null 2>"$T/as1.txt"; r1=$?
        TLD_AI_DB=$T/sp.cache limit 120 "$B/ai-par" 8 >/dev/null 2>"$T/as2.txt"; r2=$?
        TLD_AI_DB=$T/sd.cache limit 120 "$B/ai-defaults" stress "$HOST" >/dev/null 2>"$T/as3.txt"; r3=$?
        n=$(cat "$T/as1.txt" "$T/as2.txt" "$T/as3.txt" | grep -c -E 'runtime error|AddressSanitizer|LeakSanitizer')
        [ $r1 = 0 ] && [ $r2 = 0 ] && [ $r3 = 0 ] && [ "$n" = 0 ]
        check "sanitizers, $run run: triage, par, defaults stress" $? "rc=$r1/$r2/$r3 reports=$n"
        grep -h -E -A5 'SUMMARY|runtime error' "$T/as1.txt" "$T/as2.txt" "$T/as3.txt" | head -40
    done
}

group_tsan() {
    start_mock ts 0.93
    TLD_AI_HOST=$HOST TLD_AI_DB=$T/ts.cache TLD_AI_FRESH=1 limit 300 "$B/ai-par-tsan" 8 >/dev/null 2>"$T/tsan.txt"; r1=$?
    TLD_AI_HOST=$HOST TLD_AI_DB=$T/ts2.cache limit 300 "$B/ai-defaults-tsan" stress "$HOST" >/dev/null 2>"$T/tsan2.txt"; r2=$?
    n=$(cat "$T/tsan.txt" "$T/tsan2.txt" | grep -c 'WARNING: ThreadSanitizer')
    [ "$n" -eq 0 ] && [ $r1 = 0 ] && [ $r2 = 0 ]
    check "ThreadSanitizer: par on 8 threads, defaults stress" $? "rc=$r1/$r2, $n warnings"
    [ "$n" -eq 0 ] || head -80 "$T/tsan.txt" "$T/tsan2.txt"
}

case $GROUP in
all) for g in plumbing noserver http json cache defaults; do group_$g; done ;;
plumbing | noserver | http | json | cache | defaults | sanitize | tsan) group_$GROUP ;;
*) echo "unknown group $GROUP"; exit 2 ;;
esac

[ $FAIL = 0 ] && echo "all tests passed" || echo "some tests failed"
exit $FAIL
