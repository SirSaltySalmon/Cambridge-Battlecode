"""SPSA tuning of a bot's evo_constants (see README.md).

Each iteration k: every parameter moves c_k deltas up or down at random (theta+) and the mirror way (theta-). Both play
the current center ("last best", an evo copy) and the base bot on a random share of the maps, both sides, in one
batch. f = weighted score (wins + draws/2) vs center and vs base; the center moves a_k * (f+ - f-) / (realised gap) per
parameter, clipped. Every few iterations the center plays the base on all maps (checkpoint): the progress line.
All state lives in evo_runs/<run>/: config.json, state.json, journal.jsonl, log.txt, games/<batch>/results.jsonl.
"""
import collections
import concurrent.futures
import glob
import hashlib
import json
import math
import os
import random
import re
import shutil
import subprocess
import threading
import time

import evo_params

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
RUNS = os.path.join(ROOT, "evo_runs")
DEFAULTS = {
    "base": "bots/v149",        # bot folder (repo-relative) with an evo_constants file; also the fixed opponent
    "w_best": 2.0,              # weight of the score vs the center (last best)
    "w_base": 1.0,              # weight of the score vs the base bot
    "jobs": 16,                 # cambc run-batch --jobs
    "map_fraction": 0.5,        # share of maps each iteration plays (both sides); checkpoints play all
    "checkpoint_every": 5,      # iterations between checkpoints (center vs base, all maps)
    "checkpoint_seeds": 2,      # checkpoint copies: seed 0 plain, seed s burns 300*s fuel at turn end
    "max_iterations": 0,        # 0 = until paused
    "lr": 25.0,                 # a_0: a 10-point f+/f- gap moves a parameter ~0.3 deltas at the start
    "perturb": 1.0,             # c_0 in deltas (decays as k^-0.101, never under 0.5)
    "max_step": 0.3,            # most a parameter moves per iteration, in deltas
    "rng_seed": 1,
}
RESUME_KEYS = ("jobs", "max_iterations", "checkpoint_every", "checkpoint_seeds")
A_STAB, ALPHA, GAMMA = 10, 0.602, 0.101
SLOW_MAPS = ("boss-battle",)    # seat B runs out of fuel there with any bot: left out of fuel totals


class Stopped(Exception):
    pass


def rel(path):
    return os.path.relpath(path, ROOT).replace("\\", "/")


def score(recs):
    w = sum(r["res"] == "W" for r in recs)
    l = sum(r["res"] == "L" for r in recs)
    d = sum(r["res"] == "D" for r in recs)
    n = w + l + d
    s = (w + 0.5 * d) / n if n else 0.5
    return {"w": w, "l": l, "d": d, "n": n, "score": s, "ci": 1.96 * math.sqrt(max(s * (1 - s), 1e-9) / n) if n else 0}


def fuel(recs):
    rs = [r for r in recs if not any(m in r["map"] for m in SLOW_MAPS)]
    n = max(1, len(rs))
    return {"us": sum(r["fuel_us"] for r in rs) / n, "them": sum(r["fuel_them"] for r in rs) / n}


def inspect(base):
    """Parameters of a bot folder, or EvoError."""
    path = evo_params.find(os.path.join(ROOT, base))
    params, fixed = evo_params.parse(path)
    return path, params, fixed


class Run:
    def __init__(self, name):
        if not re.fullmatch(r"[A-Za-z0-9_.-]{1,24}", name or ""):
            raise evo_params.EvoError("run name: 1-24 of letters, digits, _ . - (no spaces)")
        self.name = name
        self.dir = os.path.join(RUNS, name)
        self.lock = threading.Lock()

    # ---- files ----
    def path(self, *p):
        return os.path.join(self.dir, *p)

    def exists(self):
        return os.path.isfile(self.path("config.json"))

    def load(self):
        self.config = json.load(open(self.path("config.json")))
        self.state = json.load(open(self.path("state.json")))
        self.params = self.config["params"]
        self.evo_rel = self.config["evo_file"]

    def save_state(self):
        tmp = self.path("state.json.tmp")
        json.dump(self.state, open(tmp, "w"), indent=1)
        os.replace(tmp, self.path("state.json"))

    def journal(self):
        p = self.path("journal.jsonl")
        if not os.path.isfile(p):
            return []
        return [json.loads(x) for x in open(p) if x.strip()]

    def append(self, rec):
        with open(self.path("journal.jsonl"), "a") as f:
            f.write(json.dumps(rec) + "\n")

    def create(self, config):
        if self.exists():
            raise evo_params.EvoError("run %s exists: resume it instead" % self.name)
        cfg = dict(DEFAULTS)
        cfg.update({k: v for k, v in config.items() if k in DEFAULTS and v not in (None, "")})
        for k in DEFAULTS:
            if isinstance(DEFAULTS[k], (int, float)) and not isinstance(DEFAULTS[k], bool):
                cfg[k] = type(DEFAULTS[k])(cfg[k])
        base = rel(os.path.join(ROOT, cfg["base"]))
        if " " in base:
            raise evo_params.EvoError("bot path has a space (cambc manifests split on spaces): %s" % base)
        evo_path, params, fixed = inspect(base)
        cfg["base"] = base
        cfg["evo_file"] = os.path.relpath(evo_path, os.path.join(ROOT, base)).replace("\\", "/")
        cfg["params"] = [{k: p[k] for k in ("name", "type", "int", "base", "delta", "min", "max")} for p in params]
        cfg["fixed"] = fixed
        cfg["maps"] = sorted(rel(m) for m in glob.glob(os.path.join(ROOT, "maps", "*.map26a")))
        if not cfg["maps"]:
            raise evo_params.EvoError("no maps/*.map26a")
        cfg["created"] = time.strftime("%Y-%m-%d %H:%M")
        os.makedirs(self.path("b"), exist_ok=True)
        os.makedirs(self.path("games"), exist_ok=True)
        json.dump(cfg, open(self.path("config.json"), "w"), indent=1)
        # The base file's text is kept so exports render from what the run tuned, even if the base bot changes.
        shutil.copyfile(evo_path, self.path("base_evo_constants.h"))
        self.state = {"k": 0, "center_u": [0.0] * len(params), "plan": None, "checkpoint_due": False, "map_rounds": {}}
        self.save_state()
        self.load()

    # ---- parameter space (u: deltas from the base value) ----
    def values(self, u):
        out = {}
        for p, x in zip(self.params, u):
            v = min(p["max"], max(p["min"], p["base"] + x * p["delta"]))
            if p["int"]:
                v = min(p["max"], max(p["min"], math.floor(v + 0.5)))
            out[p["name"]] = int(v) if p["int"] else round(v, 6)
        return out

    def u_of(self, values):
        return [(values[p["name"]] - p["base"]) / p["delta"] for p in self.params]

    def u_bounds(self, p):
        return (p["min"] - p["base"]) / p["delta"], (p["max"] - p["base"]) / p["delta"]

    def base_values(self):
        return self.values([0.0] * len(self.params))

    def render(self, values):
        params, _ = evo_params.parse(self.path("base_evo_constants.h"))
        return evo_params.render(self.path("base_evo_constants.h"), params, values)


class Engine:
    """One tuning run at a time, in a background thread; the GUI reads status() and the run's files."""

    def __init__(self):
        self.thread = None
        self.run = None
        self.proc = None
        self.pause = threading.Event()
        self.stop_now = threading.Event()
        self.logs = collections.deque(maxlen=300)
        self.status_ = {"state": "idle"}
        self.lock = threading.Lock()

    # ---- control ----
    def busy(self):
        return self.thread is not None and self.thread.is_alive()

    def start(self, name, config=None):
        if self.busy():
            raise evo_params.EvoError("a run is going: %s" % self.run.name)
        run = Run(name)
        if run.exists():
            run.load()
            # A resume may change how it runs, never what it optimises.
            changed = {k: type(DEFAULTS[k])(v) for k, v in (config or {}).items()
                       if k in RESUME_KEYS and v not in (None, "") and type(DEFAULTS[k])(v) != run.config[k]}
            if changed:
                run.config.update(changed)
                json.dump(run.config, open(run.path("config.json"), "w"), indent=1)
        else:
            run.create(config or {})
        self.run = run
        self.pause.clear()
        self.stop_now.clear()
        self.logs.clear()
        self.thread = threading.Thread(target=self._main, daemon=True)
        self.thread.start()

    def request_pause(self):
        self.pause.set()
        self.log("pause requested: stopping after this iteration")

    def request_stop(self):
        self.pause.set()
        self.stop_now.set()
        self._kill()
        self.log("stop requested: killing the batch (finished games are kept)")

    def _kill(self):
        p = self.proc
        if p and p.poll() is None:
            subprocess.run(["taskkill", "/F", "/T", "/PID", str(p.pid)], capture_output=True)

    def status(self):
        with self.lock:
            s = dict(self.status_)
        s["run"] = self.run.name if self.run else None
        s["busy"] = self.busy()
        s["log"] = list(self.logs)[-80:]
        return s

    def set(self, **kw):
        with self.lock:
            self.status_.update(kw)

    def log(self, msg):
        line = time.strftime("%H:%M:%S ") + msg
        self.logs.append(line)
        if self.run:
            with open(self.run.path("log.txt"), "a", encoding="utf-8") as f:
                f.write(line + "\n")

    # ---- main loop ----
    def _main(self):
        run = self.run
        try:
            self.set(state="running", phase="building the base bot")
            self.log("run %s: %d parameters, base %s" % (run.name, len(run.params), run.config["base"]))
            self._build(os.path.join(ROOT, run.config["base"]))
            maxit = run.config["max_iterations"]
            while not self.pause.is_set():
                if run.state["checkpoint_due"]:
                    self._checkpoint()
                    run.state["checkpoint_due"] = False
                    run.save_state()
                    continue
                if maxit and run.state["k"] >= maxit:
                    self.log("reached %d iterations" % maxit)
                    break
                self._iteration()
            self.set(state="paused", phase="paused at iteration %d" % run.state["k"])
            self.log("paused at iteration %d" % run.state["k"])
        except Stopped:
            self.set(state="stopped", phase="stopped in iteration %d (resume continues it)" % run.state["k"])
            self.log("stopped")
        except Exception as e:  # noqa: BLE001 - shown in the GUI, the run stays resumable
            self._kill()
            self.set(state="error", phase="error: %s" % e)
            self.log("ERROR: %s" % e)

    def _plan(self, k):
        run = self.run
        cfg = run.config
        rng = random.Random("%s:%d" % (cfg["rng_seed"], k))
        c = max(0.5, cfg["perturb"] / (k + 1) ** GAMMA)
        signs = [rng.choice((-1, 1)) for _ in run.params]
        cu = run.state["center_u"]
        plus = run.values([x + c * s for x, s in zip(cu, signs)])
        minus = run.values([x - c * s for x, s in zip(cu, signs)])
        maps = cfg["maps"]
        n = max(1, min(len(maps), int(round(cfg["map_fraction"] * len(maps)))))
        return {"k": k, "c": c, "signs": signs, "plus": plus, "minus": minus, "center": run.values(cu),
                "maps": sorted(rng.sample(maps, n))}

    def _iteration(self):
        run = self.run
        cfg = run.config
        k = run.state["k"]
        plan = run.state["plan"]
        if not plan or plan["k"] != k:
            plan = self._plan(k)
            run.state["plan"] = plan
            run.save_state()
        t0 = time.time()
        self.set(phase="iteration %d: building" % k, k=k)
        same = plan["center"] == run.base_values()
        tags = {"p": ("p%04d" % k, plan["plus"]), "m": ("m%04d" % k, plan["minus"])}
        if not same:
            tags["c"] = ("c" + self._hash(plan["center"]), plan["center"])
        bots = self._make_many(tags)
        base = cfg["base"]
        center = bots.get("c", base)
        opps = {"base": base} if same else {"best": center, "base": base}
        rows = [(cand, opp, m) for cand in ("p", "m") for opp in opps for m in plan["maps"]]
        self.set(phase="iteration %d: %d games%s" % (k, len(rows) * 2, " (center = base)" if same else ""))
        res = self._batch("it%04d" % k, [(bots[c], opps[o], m, "%s|%s" % (c, o)) for c, o, m in rows])
        sc = {}
        for cand in ("p", "m"):
            for opp in opps:
                sc[cand + "_" + opp] = score([r for r in res if r["key"] == "%s|%s" % (cand, opp)])
            if same:
                sc[cand + "_best"] = sc[cand + "_base"]
        wb, wv = cfg["w_best"], cfg["w_base"]
        f = {c: (wb * sc[c + "_best"]["score"] + wv * sc[c + "_base"]["score"]) / (wb + wv) for c in ("p", "m")}
        # Gradient step on the realised gap (rounding and bounds can shrink it or close it).
        a = cfg["lr"] / (k + 1 + A_STAB) ** ALPHA
        up, um = run.u_of(plan["plus"]), run.u_of(plan["minus"])
        new_u, steps = [], []
        for p, x, xp, xm in zip(run.params, run.state["center_u"], up, um):
            gap = xp - xm
            step = 0.0 if abs(gap) < 1e-9 else max(-cfg["max_step"], min(cfg["max_step"], a * (f["p"] - f["m"]) / gap))
            lo, hi = run.u_bounds(p)
            new_u.append(max(lo, min(hi, x + step)))
            steps.append(step)
        rec = {"type": "iter", "k": k, "time": round(time.time() - t0), "c": plan["c"], "a": a, "maps": len(plan["maps"]),
               "plus": plan["plus"], "minus": plan["minus"], "center_before": plan["center"],
               "center_after": run.values(new_u), "center_u": new_u, "steps": steps, "f_plus": f["p"], "f_minus": f["m"],
               "scores": sc, "fuel_plus": fuel([r for r in res if r["key"].startswith("p|")]),
               "fuel_minus": fuel([r for r in res if r["key"].startswith("m|")])}
        run.append(rec)
        self.log("it %d: f+ %.3f f- %.3f (%d games, %ds)  moved: %s" % (
            k, f["p"], f["m"], len(res), rec["time"],
            ", ".join("%s %s->%s" % (n, plan["center"][n], rec["center_after"][n]) for n in plan["center"]
                      if plan["center"][n] != rec["center_after"][n]) or "nothing past rounding"))
        run.state.update(k=k + 1, center_u=new_u, plan=None)
        run.state["checkpoint_due"] = (k + 1) % cfg["checkpoint_every"] == 0
        run.save_state()
        for tag in ("p", "m"):
            shutil.rmtree(bots[tag], ignore_errors=True)
        self._sweep_centers(keep=self._hash(rec["center_after"]))

    def _checkpoint(self):
        run = self.run
        cfg = run.config
        k = run.state["k"]
        values = run.values(run.state["center_u"])
        if values == run.base_values():
            self.log("checkpoint %d skipped: center = base" % k)
            return
        h = self._hash(values)
        tags = {s: (("c%s" % h) if s == 0 else ("x%ss%d" % (h, s)), values) for s in range(cfg["checkpoint_seeds"])}
        self.set(phase="checkpoint %d: building" % k)
        bots = self._make_many(tags, burns={s: s for s in tags})
        rows = [(bots[s], cfg["base"], m, "s%d" % s) for s in tags for m in cfg["maps"]]
        self.set(phase="checkpoint %d: %d games vs base" % (k, len(rows) * 2))
        t0 = time.time()
        res = self._batch("ck%04d" % k, rows)
        sc = score(res)
        rec = {"type": "checkpoint", "k": k, "time": round(time.time() - t0), "values": values, "u": run.state["center_u"],
               **sc, "fuel": fuel(res),
               "per_map": [[r["map"], r["side"], r["res"], r["key"]] for r in res],
               "seeds": {"s%d" % s: score([r for r in res if r["key"] == "s%d" % s]) for s in tags}}
        run.append(rec)
        self.log("checkpoint %d: %d-%d-%d vs base, score %.3f +- %.3f, out-of-fuel/game %.1f vs %.1f" % (
            k, sc["w"], sc["l"], sc["d"], sc["score"], sc["ci"], rec["fuel"]["us"], rec["fuel"]["them"]))
        for s in tags:
            if s:
                shutil.rmtree(bots[s], ignore_errors=True)

    # ---- bots ----
    @staticmethod
    def _hash(values):
        return hashlib.sha1(json.dumps(values, sort_keys=True).encode()).hexdigest()[:6]

    def _sweep_centers(self, keep):
        for d in glob.glob(self.run.path("b", "evo-c*")):
            if not os.path.basename(d).endswith(keep):
                shutil.rmtree(d, ignore_errors=True)

    def _make_many(self, tags, burns=None):
        """{key: (tag, values)} -> {key: repo-relative bot folder}, built in parallel."""
        burns = burns or {}
        with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, len(tags))) as ex:
            futs = {key: ex.submit(self._make, tag, values, burns.get(key, 0)) for key, (tag, values) in tags.items()}
            return {key: f.result() for key, f in futs.items()}

    def _make(self, tag, values, burn):
        run = self.run
        dst = run.path("b", "evo-" + tag)
        marker = os.path.join(dst, "evo_copy.json")
        want = {"values": values, "burn": burn, "base": run.config["base"]}
        wasm = os.path.join(dst, ".cambc", "build-release", "bot-injected.wasm")
        if os.path.isfile(marker) and os.path.isfile(wasm) and json.load(open(marker)) == want:
            return rel(dst)
        if self.stop_now.is_set():
            raise Stopped()
        shutil.rmtree(dst, ignore_errors=True)
        shutil.copytree(os.path.join(ROOT, run.config["base"]), dst,
                        ignore=shutil.ignore_patterns(".cambc", "*.py", "build", "target", "evo_copy.json"))
        open(os.path.join(dst, run.evo_rel), "w", encoding="utf-8").write(run.render(values))
        readme = os.path.join(dst, "README.md")
        old = open(readme, encoding="utf-8").read() if os.path.isfile(readme) else ""
        open(readme, "w", encoding="utf-8").write(
            "# EVO COPY %s (evotune run %s, base %s) - generated, not a release candidate\n\n%s"
            % (tag, run.name, run.config["base"], old))
        if burn:
            main = os.path.join(dst, "src", "main.cpp")
            text = open(main, encoding="utf-8").read() if os.path.isfile(main) else ""
            hook = re.findall(r"^([ \t]*)cambc::end_turn\(\);", text, re.M)
            if len(hook) != 1:
                raise evo_params.EvoError("seed copies need exactly one `cambc::end_turn();` in src/main.cpp; "
                                          "set checkpoint seeds to 1")
            ind = hook[0]
            text = text.replace("%scambc::end_turn();" % ind, "%s{ volatile int sink = 0; for (int i = 0; i < %d; ++i) "
                                "sink += i; }  // evotune fuel-burn seed\n%scambc::end_turn();" % (ind, 300 * burn, ind))
            open(main, "w", encoding="utf-8").write(text)
        self._build(dst)
        json.dump(want, open(marker, "w"))
        return rel(dst)

    def _build(self, bot_dir):
        p = subprocess.run(["cambc", "build", "--release", bot_dir], cwd=ROOT, capture_output=True, text=True,
                           errors="replace")
        if p.returncode != 0:
            tail = (p.stdout + p.stderr).strip().splitlines()[-15:]
            raise evo_params.EvoError("build failed for %s:\n%s" % (rel(bot_dir), "\n".join(tail)))

    # ---- games ----
    def _batch(self, name, rows):
        """rows: (cand bot, opponent bot, map, key) -> one record per game (both sides), resumable."""
        run = self.run
        d = run.path("games", name)
        os.makedirs(d, exist_ok=True)
        done_path = os.path.join(d, "results.jsonl")
        done = {}
        if os.path.isfile(done_path):
            for x in open(done_path):
                if x.strip():
                    r = json.loads(x)
                    done[r["id"]] = r
        games = []
        for cand, opp, m, key in rows:
            stem = os.path.basename(m)[:-len(".map26a")]
            for side in ("a", "b"):
                gid = "%s-%s-%s" % (key.replace("|", "_"), stem, side)
                games.append({"id": gid, "cand": cand, "opp": opp, "map": m, "stem": stem, "side": side, "key": key})
        todo = [g for g in games if g["id"] not in done]
        rounds = run.state["map_rounds"]
        todo.sort(key=lambda g: -rounds.get(g["stem"], 1000))
        total, t0 = len(games), time.time()
        self.set(done=len(games) - len(todo), total=total, eta=None)
        if todo:
            for g in todo:  # a killed game leaves its replay, stats or .repl26a.error.log; run-batch refuses those
                for f in glob.glob(os.path.join(d, g["id"] + ".*")):
                    os.remove(f)
            lines = []
            for g in todo:
                a, b = (g["cand"], g["opp"]) if g["side"] == "a" else (g["opp"], g["cand"])
                rp = lambda x: os.path.relpath(os.path.join(ROOT, x), d).replace("\\", "/")
                lines.append("%s %s %s %s.repl26a --statistics %s.json --json" % (rp(a), rp(b), rp(g["map"]), g["id"], g["id"]))
            manifest = os.path.join(d, "manifest.txt")
            open(manifest, "w").write("\n".join(lines) + "\n")
            out = open(os.path.join(d, "cambc.log"), "w", encoding="utf-8", errors="replace")
            self.proc = subprocess.Popen(
                ["cambc", "run-batch", "manifest.txt", "--jobs", str(run.config["jobs"]), "--release", "--log-limit", "0",
                 "--json"], cwd=d, stdout=out, stderr=subprocess.STDOUT,
                creationflags=getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0))
            pending = {g["id"]: g for g in todo}
            n_start = len(games) - len(todo)
            try:
                while True:
                    alive = self.proc.poll() is None
                    self._collect(d, pending, done, done_path)
                    finished = len(games) - len(pending)
                    rate = (finished - n_start) / max(1.0, time.time() - t0)
                    self.set(done=finished, eta=round(len(pending) / rate) if rate > 0 else None)
                    if self.stop_now.is_set():
                        self._kill()
                        raise Stopped()
                    if not alive:
                        break
                    time.sleep(2)
                time.sleep(1)
                self._collect(d, pending, done, done_path)
            finally:
                out.close()
                self.proc = None
            if pending:
                self.log("%s: %d of %d games failed (see %s)" % (name, len(pending), total, rel(os.path.join(d, "cambc.log"))))
                if len(pending) > 0.1 * total:
                    raise evo_params.EvoError("%s: %d games failed - look at %s" % (name, len(pending),
                                                                                  rel(os.path.join(d, "cambc.log"))))
            for f in glob.glob(os.path.join(d, "*.repl26a")) + glob.glob(os.path.join(d, "*-[ab].json")):
                try:
                    os.remove(f)
                except OSError:
                    pass
        run.save_state()
        return [done[g["id"]] for g in games if g["id"] in done]

    def _collect(self, d, pending, done, done_path):
        new = []
        for gid, g in list(pending.items()):
            jp = os.path.join(d, gid + ".json")
            if not os.path.isfile(jp):
                continue
            try:
                j = json.load(open(jp, encoding="utf-8"))
            except (json.JSONDecodeError, OSError, UnicodeDecodeError):
                continue  # still being written
            us = "teamA" if g["side"] == "a" else "teamB"
            them = "teamB" if us == "teamA" else "teamA"
            gems = {t["team"]: int(t["submittedGems"]) for t in j["teams"]}
            cnt = collections.Counter()
            for e in j["events"]:
                if e["kind"] in ("fuelExhausted", "kill", "death"):
                    cnt[(e["kind"], e["owner"]["team"] == us)] += len(e["entries"])
            res = "D" if gems[us] == gems[them] else "W" if j["match"]["outcome"] == us else "L"
            rec = {"id": gid, "key": g["key"], "map": g["stem"], "side": g["side"], "res": res,
                   "gems_us": gems[us], "gems_them": gems[them], "rounds": j["match"]["roundsCompleted"],
                   "fuel_us": cnt[("fuelExhausted", True)], "fuel_them": cnt[("fuelExhausted", False)],
                   "kills": cnt[("kill", True)], "deaths": cnt[("death", True)]}
            r = self.run.state["map_rounds"]
            r[g["stem"]] = max(r.get(g["stem"], 0), rec["rounds"])
            done[gid] = rec
            new.append(rec)
            del pending[gid]
            for ext in (".json", ".repl26a"):
                try:
                    os.remove(os.path.join(d, gid + ext))
                except OSError:
                    pass  # the replay may still be open; swept with the batch folder later
        if new:
            with open(done_path, "a") as f:
                for rec in new:
                    f.write(json.dumps(rec) + "\n")


def export(run_name, which):
    """evo_constants text for the current center ("center") or a checkpoint k (int)."""
    run = Run(run_name)
    run.load()
    if which == "center":
        values = run.values(run.state["center_u"])
    else:
        recs = [r for r in run.journal() if r["type"] == "checkpoint" and r["k"] == int(which)]
        if not recs:
            raise evo_params.EvoError("no checkpoint %s" % which)
        values = recs[-1]["values"]
    return values, run.render(values)


def make_version(run_name, which, new_name):
    """Copy the base bot to bots/<new_name> with the chosen values (a real version, not an evo copy)."""
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", new_name or ""):
        raise evo_params.EvoError("bot name: letters, digits, _ . -")
    run = Run(run_name)
    run.load()
    dst = os.path.join(ROOT, "bots", new_name)
    if os.path.exists(dst):
        raise evo_params.EvoError("bots/%s exists" % new_name)
    values, text = export(run_name, which)
    shutil.copytree(os.path.join(ROOT, run.config["base"]), dst, ignore=shutil.ignore_patterns(".cambc", "evo_copy.json"))
    open(os.path.join(dst, run.evo_rel), "w", encoding="utf-8").write(text)
    return rel(dst), values
