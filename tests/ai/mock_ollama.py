"""Stand-in for Ollama, used only to test tld_ai.h's plumbing (HTTP, requests, parsing,
caching, replay, ai_defaults). It returns canned answers in the documented shapes; it
is not a model, and it never talks to a real Ollama.

    mock_ollama.py PORT [CONFIDENCE] [--chunked] [--slow S] [--fail-first PATH:N]

Every request is written to stderr, one line each:
    mock: PATH JSON        a model call (/api/chat, /v1/systemone, /api/embed)
    get: PATH              a GET (/api/version, /api/tags)
    fail: PATH             a model call answered with the injected HTTP 500
"""
import argparse, hashlib, json, math, sys, threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ap = argparse.ArgumentParser()
ap.add_argument("port", type=int)
ap.add_argument("conf", type=float, nargs="?", default=0.93)
ap.add_argument("--chunked", action="store_true", help="send bodies chunked")
ap.add_argument("--slow", type=float, default=0, help="seconds before each model reply")
ap.add_argument("--fail-first", default="", help="PATH:N, first N calls to PATH get 500")
args = ap.parse_args()

CLOUD_HOST = "https://ollama.com:443"
TAGS = [
    {"name": "inkling-2-mini:latest", "model": "inkling-2-mini:latest", "digest": "sha256:aaa",
     "size": 3300000000},
    {"name": "autotrust-jev:latest", "model": "autotrust-jev:latest", "digest": "sha256:bbb",
     "size": 900000000},
    {"name": "embeddinggemma-2:latest", "model": "embeddinggemma-2:latest",
     "digest": "sha256:ccc", "size": 620000000},
    {"name": "other-model:latest", "model": "other-model:latest", "digest": "sha256:ddd",
     "size": 5000000000},
    {"name": "alt-llm:latest", "model": "alt-llm:latest", "digest": "sha256:a11",
     "size": 1000000000},
    {"name": "alt-decider:latest", "model": "alt-decider:latest", "digest": "sha256:a22",
     "size": 1000000000},
    {"name": "alt-embedder:latest", "model": "alt-embedder:latest", "digest": "sha256:a33",
     "size": 1000000000},
    {"name": "gpt-oss:120b-cloud", "model": "gpt-oss:120b-cloud", "digest": "sha256:eee",
     "size": 384, "remote_model": "gpt-oss:120b", "remote_host": CLOUD_HOST},
]
lock = threading.Lock()
fail_path, _, fail_n = args.fail_first.partition(":")
fails = {"left": int(fail_n or 0)}
fills = {"n": 0}


def norm(m):
    return m if ":" in m.rsplit("/", 1)[-1] else m + ":latest"


def log(line):
    sys.stderr.write(line + "\n")
    sys.stderr.flush()


def fake(schema, n):
    out = {}
    for k, p in schema.get("properties", {}).items():
        out[k] = {"string": f"mock {k}", "number": 0.5 if n == 1 else 0.1,
                  "integer": 1, "boolean": True}[p["type"]]
    return out


class H(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def reply(self, obj, status=200):
        b = json.dumps(obj).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Connection", "close")
        if args.chunked:
            self.send_header("Transfer-Encoding", "chunked")
            self.end_headers()
            step = max(7, len(b) // 5)      # several chunks, odd sizes
            for i in range(0, len(b), step):
                part = b[i:i + step]
                self.wfile.write(b"%x;ext=1\r\n%s\r\n" % (len(part), part))
            self.wfile.write(b"0\r\n\r\n")
        else:
            self.send_header("Content-Length", str(len(b)))
            self.end_headers()
            self.wfile.write(b)
        self.wfile.flush()
        self.close_connection = True

    def do_GET(self):
        log(f"get: {self.path}")
        if self.path == "/api/version":
            self.reply({"version": "0.40.17-mock"})
        elif self.path == "/api/tags":
            self.reply({"models": TAGS})
        else:
            self.reply({"error": "not found"}, 404)

    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        req = json.loads(self.rfile.read(n) or b"{}")
        model = norm(req.get("model", ""))
        if model not in {t["name"] for t in TAGS}:
            return self.reply({"error": f"model '{req.get('model')}' not found"}, 404)
        with lock:
            if fail_path == self.path and fails["left"] > 0:
                fails["left"] -= 1
                log(f"fail: {self.path}")
                return self.reply({"error": "mock transient failure"}, 500)
        log(f"mock: {self.path} {json.dumps(req)}")
        if args.slow:
            time.sleep(args.slow)
        if self.path == "/api/embed":
            texts = req["input"] if isinstance(req["input"], list) else [req["input"]]

            def vec(t):  # same first word, same vector
                h = hashlib.sha256(t.lower().split()[0].encode()).digest()
                v = [b - 128 for b in h[:8]]
                n = math.sqrt(sum(x * x for x in v))
                return [x / n for x in v]
            self.reply({"model": req["model"], "embeddings": [vec(t) for t in texts],
                        "prompt_eval_count": 5 * len(texts)})
        elif self.path == "/api/chat":
            time.sleep(0.2)  # stand-in for generation time
            fmt = req.get("format")
            if isinstance(fmt, dict):
                with lock:
                    fills["n"] += 1
                    k = fills["n"]
                content = json.dumps(fake(fmt, k))
            else:
                content = "Mock reply to: " + req["messages"][-1]["content"]
            self.reply({"model": req["model"],
                        "message": {"role": "assistant", "content": content},
                        "done": True, "prompt_eval_count": 40, "eval_count": 12,
                        "total_duration": 5043500667})
        elif self.path == "/v1/systemone":
            state = json.dumps(req["state"]).lower()
            ans = {}
            for name, q in req["questions"].items():
                if q["type"] == "choice":
                    opts = list(q["criteria"])
                    pick = ("bug" if "crash" in state and "bug" in opts else
                            "billing" if "refund" in state and "billing" in opts else opts[0])
                    rest = (1 - args.conf) / (len(opts) - 1)
                    ans[name] = {"type": "choice", "choice": pick, "confidence": args.conf,
                                 "probabilities": {o: (args.conf if o == pick else rest)
                                                   for o in opts}}
                elif q["type"] == "noul":
                    ans[name] = {"type": "noul", "noul": 0.9 if "refund" in state else 0.05}
                else:
                    levels = len(q["criteria"])
                    ans[name] = {"type": "score", "score": 1.6, "confidence": 0.7,
                                 "legend": q["criteria"],
                                 "probabilities": {str(i): 1 / levels for i in range(levels)}}
            self.reply({"model": req["model"], "answers": ans,
                        "usage": {"input_tokens": 150, "output_tokens": 1}})
        else:
            self.reply({"error": "unknown endpoint"}, 404)


ThreadingHTTPServer.daemon_threads = True
ThreadingHTTPServer.request_queue_size = 128  # Go's net/http has a deep backlog too
ThreadingHTTPServer(("127.0.0.1", args.port), H).serve_forever()
