"""evotune: SPSA tuning of a cambc bot's evo_constants, with a local dashboard.

    python tools/evotune/evotune.py                         dashboard at http://127.0.0.1:8765
    python tools/evotune/evotune.py --run NAME [--base bots/v149] [--jobs 16] ...   also start (or resume) NAME
    python tools/evotune/evotune.py --inspect bots/v149     list a bot's tunable parameters and exit

Ctrl+C stops the batch; finished games are kept and the next start of the same run resumes.
"""
import argparse
import http.server
import json
import os
import sys
import threading
import urllib.parse
import webbrowser

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import engine  # noqa: E402
import evo_params  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ENGINE = engine.Engine()


def runs():
    out = []
    if os.path.isdir(engine.RUNS):
        for name in sorted(os.listdir(engine.RUNS)):
            try:
                r = engine.Run(name)
            except evo_params.EvoError:
                continue
            if r.exists():
                r.load()
                out.append({"name": name, "base": r.config["base"], "k": r.state["k"], "created": r.config["created"]})
    return out


def run_view(name):
    r = engine.Run(name)
    if not r.exists():
        raise evo_params.EvoError("no run %s" % name)
    r.load()
    cfg = {k: v for k, v in r.config.items() if k != "maps"}
    cfg["map_count"] = len(r.config["maps"])
    return {"config": cfg, "k": r.state["k"], "center": r.values(r.state["center_u"]), "journal": r.journal()}


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def reply(self, code, body, ctype="application/json"):
        data = body if isinstance(body, bytes) else json.dumps(body).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def guard(self, fn):
        try:
            self.reply(200, fn())
        except evo_params.EvoError as e:
            self.reply(400, {"error": str(e)})
        except Exception as e:  # noqa: BLE001
            self.reply(500, {"error": "%s: %s" % (type(e).__name__, e)})

    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        q = dict(urllib.parse.parse_qsl(u.query))
        if u.path == "/":
            self.reply(200, open(os.path.join(HERE, "dashboard.html"), "rb").read(), "text/html; charset=utf-8")
        elif u.path == "/api/status":
            self.guard(ENGINE.status)
        elif u.path == "/api/runs":
            self.guard(lambda: {"runs": runs(), "defaults": engine.DEFAULTS})
        elif u.path == "/api/run":
            self.guard(lambda: run_view(q.get("name", "")))
        else:
            self.reply(404, {"error": "not found"})

    def do_POST(self):
        n = int(self.headers.get("Content-Length") or 0)
        body = json.loads(self.rfile.read(n) or b"{}")
        p = self.path
        if p == "/api/inspect":
            def go():
                path, params, fixed = engine.inspect(body.get("base", ""))
                return {"file": engine.rel(path), "params": [{k: v for k, v in x.items() if k != "span"} for x in params],
                        "fixed": fixed}
            self.guard(go)
        elif p == "/api/start":
            self.guard(lambda: (ENGINE.start(body.get("name", ""), body.get("config") or {}), {"ok": True})[1])
        elif p == "/api/pause":
            self.guard(lambda: (ENGINE.request_pause(), {"ok": True})[1])
        elif p == "/api/stop":
            self.guard(lambda: (ENGINE.request_stop(), {"ok": True})[1])
        elif p == "/api/export":
            def go():
                values, text = engine.export(body.get("name", ""), body.get("which", "center"))
                return {"values": values, "text": text}
            self.guard(go)
        elif p == "/api/make":
            def go():
                path, values = engine.make_version(body.get("name", ""), body.get("which", "center"), body.get("bot", ""))
                return {"path": path, "values": values}
            self.guard(go)
        else:
            self.reply(404, {"error": "not found"})


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--no-browser", action="store_true")
    ap.add_argument("--inspect", metavar="BOT")
    ap.add_argument("--run", metavar="NAME")
    for k, v in engine.DEFAULTS.items():
        ap.add_argument("--" + k.replace("_", "-"), dest=k, type=type(v), default=None)
    args = ap.parse_args()
    if args.inspect:
        try:
            path, params, fixed = engine.inspect(args.inspect)
        except evo_params.EvoError as e:
            sys.exit("error: %s" % e)
        print(engine.rel(path))
        for x in params:
            print("  %-28s %-7s base %-8g delta %-6g range %g..%g" % (x["name"], x["type"], x["base"], x["delta"], x["min"], x["max"]))
        for x in fixed:
            print("  %-28s fixed (%s)" % (x["name"], x["why"]))
        return
    if args.run:
        cfg = {k: getattr(args, k) for k in engine.DEFAULTS if getattr(args, k) is not None}
        try:
            ENGINE.start(args.run, cfg)
        except evo_params.EvoError as e:
            sys.exit("error: %s" % e)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    url = "http://127.0.0.1:%d/" % args.port
    print("evotune dashboard: %s  (Ctrl+C stops; runs resume)" % url)
    if not args.no_browser:
        threading.Timer(0.5, lambda: webbrowser.open(url)).start()
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("stopping...")
        if ENGINE.busy():
            ENGINE.request_stop()
            ENGINE.thread.join(timeout=30)
    finally:
        srv.server_close()


if __name__ == "__main__":
    main()
