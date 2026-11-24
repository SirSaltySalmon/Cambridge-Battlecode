"""v214-ward-hold: hold-specific ward numbers from probe lines (run with python -I, from anywhere).

  python -I bots/v214-ward-hold/wp_hold.py DIR [DIR ...]

Reads the team under test (team A in -a replays, team B in -b). Per DIR group (all dirs pooled):
- games, ghost placements per game, ghost episodes and the share blocked at start, hidden-attacker blocked turns;
- held turns (charge ready, hold kept it) by reason;
- scout placements split by what a hold would have stopped: FIGHT mode, an enemy within WARD_HOLD_RANGE now, or one
  within 1-4 / 5-9 rounds ago (needs the near/fight/ago fields this build adds), with their reveals and the
  ward-needed hits credited to them (owner's latest placement, as wp_analyze.py does).
"""
import collections
import glob
import sys
import zipfile


def cls(kv):
    if "fight" not in kv:
        return "unknown"
    if kv["fight"] == "1":
        return "fight"
    if kv["near"] == "1":
        return "near"
    ago = int(kv["ago"])
    if ago < 5:
        return "ago1-4"
    if ago < 10:
        return "ago5-9"
    return "clear"


def main(dirs):
    games = 0
    place = collections.Counter()
    rev = collections.Counter()
    hits = collections.Counter()
    held = collections.Counter()
    ghost = 0
    eps = eps_blocked = eps_never = 0
    hidden_blocked = 0
    hidden_place = 0
    hits_total = hits_only = 0
    for d in dirs:
        for f in glob.glob(d + "/*.repl26a"):
            side = "team-a" if f.endswith("-a.repl26a") else "team-b"
            try:
                t = zipfile.ZipFile(f).read(side + ".stdout").decode(errors="replace")
            except Exception:
                continue
            games += 1
            last = {}
            ep = {}

            def close(c):
                nonlocal eps, eps_blocked, eps_never
                e = ep.pop(c, None)
                if not e:
                    return
                eps += 1
                eps_blocked += e[1]
                eps_never += e[1] and not e[2]

            for line in t.splitlines():
                if "WP r" not in line:
                    continue
                p = line.split("WP ", 1)[1].split()
                r = int(p[0][1:])
                c = p[1]
                what = p[2]
                if what == "stat":
                    continue
                if c in ep and r - ep[c][0] > 1:
                    close(c)
                if what == "held":
                    kv = dict(x.split("=") for x in p[3:])
                    held[cls(kv)] += 1
                elif what == "place":
                    kind = p[3]
                    kv = dict(x.split("=") for x in p[4:])
                    if kv.get("ok", "1") == "0":
                        continue
                    if kind == "ghost":
                        ghost += 1
                        if c not in ep:
                            ep[c] = [r, False, True]
                        ep[c][0] = r
                        ep[c][2] = True
                        last[c] = ("ghost", "-")
                    elif kind == "hidden":
                        hidden_place += 1
                        last[c] = ("hidden", "-")
                    else:
                        k = (kind, cls(kv))
                        place[k] += 1
                        rev[k] += int(kv.get("rev", 0)) > 0
                        last[c] = k
                elif what == "blocked" and p[3] == "ghost":
                    if c not in ep:
                        ep[c] = [r, True, False]
                    ep[c][0] = r
                    ep[c][1] = True
                elif what == "blocked":
                    hidden_blocked += 1
                elif what == "hit":
                    hits_total += 1
                    if p[4].endswith("=1"):
                        hits_only += 1
                        hits[last.get(c, ("none", "-"))] += 1
            for c in list(ep):
                close(c)
    print("games", games, "ghost placements", ghost, "per game %.2f" % (ghost / max(1, games)),
          "hidden placements", hidden_place, "hidden blocked", hidden_blocked)
    print("ghost episodes", eps, "blocked at start %d (%.0f%%)" % (eps_blocked, 100 * eps_blocked / max(1, eps)),
          "blocked and never warded", eps_never)
    print("champion hits", hits_total, "needing ward", hits_only)
    print("held turns", dict(held), "total", sum(held.values()))
    print("%-24s %8s %8s %8s" % ("scout placement", "count", "reveals", "wo-hits"))
    for k in sorted(place):
        print("%-24s %8d %8d %8d" % ("%s/%s" % k, place[k], rev[k], hits[k]))
    print("ward-needed hits by other last placement", {"%s/%s" % k: v for k, v in hits.items() if k not in place})


if __name__ == "__main__":
    main(sys.argv[1:])
