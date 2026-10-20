"""Local benchmark: every map from both sides, then a summary.

usage:
  python tools/bench.py run BOT OPP [--unlimited] [--release] [--maps a,b] [--jobs 11] [--out DIR]
  python tools/bench.py summary BOT DIR [DIR...]

`run` plays BOT vs OPP on all 17 maps (or --maps) with BOT as team A (<map>-a) and as team B (<map>-b), writing
replays and statistics JSON to DIR (default replays/bench/BOT-vs-OPP-<normal|unlimited>), then prints the summary.
--unlimited sets 100M fuel and a 100M log limit (decisions without fuel noise). --release builds like the platform:
debug_block is skipped.
`summary` reads every <map>-<a|b>.json (+ .repl26a) in the dirs: record, gems, kills, deaths, captures, and fuel per
turn of BOT's team (mean, p99, turns at or over 100k).
"""
import collections
import glob
import json
import os
import re
import struct
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAPS = ["ambush", "collision", "contest", "corners", "eight", "elkring", "expanse", "fragment", "interlock", "lanes",
        "monolith", "packs", "pilgrimage", "rows", "rushbait-v2", "stampede", "swirl"]


def run(argv):
    bot, opp = argv[0], argv[1]
    unlimited = "--unlimited" in argv
    release = "--release" in argv
    maps = MAPS
    jobs = "11"
    out = None
    for i, a in enumerate(argv):
        if a == "--maps":
            maps = argv[i + 1].split(",")
        if a == "--jobs":
            jobs = argv[i + 1]
        if a == "--out":
            out = argv[i + 1]
    if out is None:
        out = os.path.join(ROOT, "replays", "bench", "%s-vs-%s-%s%s" % (bot, opp, "unlimited" if unlimited else "normal",
                                                                         "-release" if release else ""))
    os.makedirs(out, exist_ok=True)
    # run-batch refuses to overwrite a replay (or a killed run's .error.log), so an old run's files would be
    # summarised again or block the new games: the folder is emptied.
    for old in glob.glob(os.path.join(out, "*")):
        if os.path.isfile(old):
            os.remove(old)
    out = os.path.abspath(out)
    rows = []
    for m in maps:
        mp = os.path.join(ROOT, "maps", m + ".map26a")
        for side, (a, b) in (("a", (bot, opp)), ("b", (opp, bot))):
            stem = os.path.join(out, "%s-%s" % (m, side))
            rows.append('"%s" "%s" "%s" "%s.repl26a" --statistics "%s.json" --json' % (
                os.path.join(ROOT, "bots", a), os.path.join(ROOT, "bots", b), mp, stem, stem))
    manifest = os.path.join(out, "manifest.txt")
    with open(manifest, "w") as f:
        f.write("\n".join(rows) + "\n")
    cmd = ["cambc", "run-batch", manifest, "--jobs", jobs]
    if unlimited:
        cmd += ["--fuel-quantum", "100000000", "--log-limit", "100000000"]
    if release:
        cmd += ["--release"]
    subprocess.run(cmd, cwd=ROOT, stdout=subprocess.DEVNULL)
    summary([bot, out])


def fuel_values(z, side, picks, champions):
    # 13-byte header, 8 budget values, one value per draft pick, then one per champion turn. The first game turn of
    # each champion has the 2x budget, so it is skipped.
    raw = z.read("team-%s.fuel" % side)
    vals = [v[0] for v in struct.iter_unpack("<Q", raw[13:13 + (len(raw) - 13) // 8 * 8])]
    return vals[8 + picks + champions:]


def summary(argv):
    bot = argv[0]
    files = sorted(f for d in argv[1:] for f in glob.glob(os.path.join(d, "*.json")) if re.search(r"-[ab]\.json$", f))
    tot = collections.Counter()
    fuel = []
    lines = []
    for f in files:
        try:
            d = json.load(open(f))
        except Exception:
            continue
        side = "A" if f.endswith("-a.json") else "B"
        us = "team" + side
        them = "teamB" if us == "teamA" else "teamA"
        gems = {t["team"]: int(t["submittedGems"]) for t in d["teams"]}
        ours = [c for c in d["champions"] if c["team"] == us]
        kills = sum(int(c["metrics"]["kills"]) for c in ours)
        deaths = sum(int(c["metrics"]["deaths"]) for c in ours)
        names = {c["name"] for c in ours}
        picks = len(ours) - (1 if "Cassi" in names and "Issac" in names else 0)
        win = d["match"]["outcome"] == us
        rounds = d["match"]["roundsCompleted"]
        repl = f[:-5] + ".repl26a"
        over = 0
        n = 0
        if os.path.exists(repl):
            try:
                z = zipfile.ZipFile(repl)
                v = fuel_values(z, side.lower(), picks, len(ours))
                fuel += v
                over = sum(1 for x in v if x >= 100000)
                n = len(v)
            except Exception:
                pass
        name = os.path.basename(f)[:-5]
        lines.append("%-16s %s %5d %5d %5d %4d %5d %6d/%d" % (name, "W" if win else "L", rounds, gems[us], gems[them],
                                                             kills, deaths, over, n))
        tot.update(games=1, wins=win, gems=gems[us], opp=gems[them], kills=kills, deaths=deaths, over=over)
    print("%-16s R round  gems   opp kill death  over100k/turns" % "game")
    print("\n".join(lines))
    g = max(1, tot["games"])
    fuel.sort()
    mean = sum(fuel) / max(1, len(fuel))
    p99 = fuel[int(len(fuel) * 0.99)] if fuel else 0
    print("\n%s: %d-%d, gems %d vs %d (%+.1f%%), kills %d, deaths %d, fuel mean %.1fk p99 %.1fk, turns >=100k %d (%.2f%%)"
          % (bot, tot["wins"], tot["games"] - tot["wins"], tot["gems"], tot["opp"],
             100.0 * (tot["gems"] - tot["opp"]) / max(1, tot["opp"]), tot["kills"], tot["deaths"], mean / 1000,
             p99 / 1000, tot["over"], 100.0 * tot["over"] / max(1, len(fuel))))


if __name__ == "__main__":
    {"run": run, "summary": summary}[sys.argv[1]](sys.argv[2:])
