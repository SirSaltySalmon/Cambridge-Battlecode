"""v214-ward-mask step 1: classify scout ward refusals and throws.

  python -I bots/v214-ward-mask/mask_analyze.py DIR [DIR ...]

Reads the team under test (team A in -a replays, team B in -b replays) and the probe lines
"scout refused X Y k=v ..." and "throw scout X Y k=v ... why=...".
"""
import collections
import glob
import sys
import zipfile

ref = collections.Counter()
thr = collections.Counter()
why = collections.Counter()
rep = collections.Counter()
games = 0
for d in sys.argv[1:]:
    for f in glob.glob(d + "/*.repl26a"):
        side = "team-a" if f.endswith("-a.repl26a") else "team-b"
        try:
            t = zipfile.ZipFile(f).read(side + ".stdout").decode(errors="replace")
        except Exception:
            continue
        games += 1
        for line in t.splitlines():
            if "WP r" not in line:
                continue
            p = line.split("WP ", 1)[1].split()
            c = p[1]
            if p[2] == "scout" and p[3] == "refused":
                kv = dict(x.split("=", 1) for x in p[6:] if "=" in x)
                rep[(f, c, p[4], p[5])] += 1
                if kv.get("award") == "1":
                    k = "ally ward" + (" (+ally champion)" if kv.get("ally") == "1" else "")
                elif kv.get("enemy") == "1":
                    k = "enemy champion"
                elif kv.get("camp") == "1" or kv.get("spawn") == "1":
                    k = "camp / camp spawn"
                elif kv.get("ally") == "1":
                    k = "ally champion"
                elif kv.get("eward") == "1":
                    k = "enemy ward"
                else:
                    k = "unknown"
                ref[k] += 1
                ref["bush pick" if kv.get("hid") == "1" else "catch-up pick"] += 1
                if kv.get("spawn") == "1":
                    ref["on camp spawn tile"] += 1
                if kv.get("eward") == "1":
                    ref["enemy ward on tile (any)"] += 1
            elif p[2] == "throw":
                rest = line.split("why=", 1)
                kv = dict(x.split("=", 1) for x in rest[0].split("WP ", 1)[1].split()[6:] if "=" in x)
                w = rest[1].strip() if len(rest) > 1 else ""
                why[w[:60]] += 1
                for k in ("spawn", "enemy", "camp", "ally", "self", "bush", "hid", "award", "eward"):
                    if kv.get(k) == "1":
                        thr[k] += 1
                thr["total"] += 1
total = sum(v for k, v in ref.items() if k in ("ally ward", "ally ward (+ally champion)", "enemy champion",
                                                  "camp / camp spawn", "ally champion", "enemy ward", "unknown"))
print("games", games, "refusals", total, "per game %.1f" % (total / max(1, games)))
for k, v in ref.most_common():
    print("  %-28s %6d  %5.1f%%" % (k, v, 100 * v / max(1, total)))
reps = sorted(rep.values(), reverse=True)
print("distinct (game,champ,tile)", len(rep), "top", reps[:8],
      "share from tiles refused 5+ times %.2f" % (sum(v for v in reps if v >= 5) / max(1, total)))
print("throws", thr["total"], "per game %.2f" % (thr["total"] / max(1, games)), dict(thr))
for k, v in why.most_common(8):
    print("  %6d %s" % (v, k))
