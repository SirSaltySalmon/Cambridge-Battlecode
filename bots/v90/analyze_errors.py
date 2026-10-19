"""Bench audit: bot errors (by the branch logged just before them) and fuel use, per bot.

usage: python analyze_errors.py NEW OLD DIR [DIR...]
Every <map>-a.json / <map>-b.json with a matching .repl26a in the dirs; NEW is team A in <map>-a and team B in <map>-b.

Fuel comes from the replay's team-<a|b>.fuel: a 13-byte header, eight u64 limits, the draft turns, the first play turn
of each champion (200k), then one u64 per champion turn: fuel used, 100000 when the turn ran out. Out-of-fuel turns
are also counted from the stats JSON's fuelExhausted events.
"""
import collections
import glob
import json
import os
import re
import struct
import sys
import zipfile

BRANCH = re.compile(r"^r\d+ ([a-z][\w.]*): ")
LIMIT = 100000


def turn_fuel(raw):
    values = [struct.unpack_from("<Q", raw, 13 + 8 * i)[0] for i in range((len(raw) - 13) // 8)]
    rest = values[8:]
    start = rest.index(2 * LIMIT) if 2 * LIMIT in rest else 0
    while start < len(rest) and rest[start] == 2 * LIMIT:
        start += 1
    return rest[start:]


def main():
    new, old = sys.argv[1], sys.argv[2]
    files = sorted(f for d in sys.argv[3:] for f in glob.glob(os.path.join(d, "*.json")))
    vision = collections.Counter()
    other = collections.Counter()
    by_branch = collections.defaultdict(collections.Counter)
    exhausted = collections.Counter()
    fuel = collections.defaultdict(list)
    worst = collections.defaultdict(collections.Counter)  # bot -> map -> out-of-fuel turns
    record = collections.Counter()
    gems = collections.Counter()
    games = 0
    for f in files:
        repl = f[:-5] + ".repl26a"
        if not os.path.exists(repl):
            continue
        games += 1
        game = os.path.basename(f)[:-5]
        d = json.load(open(f))
        z = zipfile.ZipFile(repl)
        new_side = "a" if game.endswith("-a") else "b"
        bot_of = {new_side: new, ("b" if new_side == "a" else "a"): old}
        for side in "ab":
            bot, team = bot_of[side], "team" + side.upper()
            last = "?"
            for raw in z.read("team-%s.stdout" % side).decode(errors="replace").splitlines():
                line = raw.lstrip("\x00 ")
                m = BRANCH.match(line)
                if m and m.group(1) != "note":
                    last = m.group(1)
                elif line.startswith("bot error"):
                    if "not in vision" in line:
                        vision[bot] += 1
                        by_branch[bot][last] += 1
                    else:
                        other[bot] += 1
            used = turn_fuel(z.read("team-%s.fuel" % side))
            fuel[bot].extend(used)
            worst[bot][game] += sum(1 for x in used if x >= LIMIT)
            gems[bot] += int(next(t["submittedGems"] for t in d["teams"] if t["team"] == team))
            record[bot] += d["match"]["outcome"] == team
            for e in d["events"]:
                if e["kind"] == "fuelExhausted" and e["owner"]["team"] == team:
                    exhausted[bot] += len(e["entries"])
    print("%d games" % games)
    for bot in (new, old):
        used = sorted(fuel[bot])
        n = max(1, len(used))
        out = sum(1 for x in used if x >= LIMIT)
        print("\n%s: %d wins, gems %d" % (bot, record[bot], gems[bot]))
        print("  'not in vision' errors %d (%.2f/game), other bot errors %d" % (vision[bot], vision[bot] / max(1, games), other[bot]))
        for name, k in by_branch[bot].most_common(10):
            print("    %-30s %d" % (name, k))
        print("  fuel per turn over %d champion turns: mean %.0f, median %d, p95 %d, p99 %d"
              % (len(used), sum(used) / n, used[n // 2], used[int(n * 0.95)], used[int(n * 0.99)]))
        print("  turns >= 90k: %d (%.2f%%); out of fuel: %d (%.3f%%), fuelExhausted events %d, in %d games"
              % (sum(1 for x in used if x >= 90000), 100.0 * sum(1 for x in used if x >= 90000) / n, out, 100.0 * out / n,
                 exhausted[bot], sum(1 for k in worst[bot].values() if k)))
        print("  most out-of-fuel turns: " + ", ".join("%s %d" % kv for kv in worst[bot].most_common(5) if kv[1]))


if __name__ == "__main__":
    main()
