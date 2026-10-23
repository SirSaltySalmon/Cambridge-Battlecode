"""Where champion turns go, per game, from debug logs (cambc run without --release, --log-limit 40000000).

usage: python analyze_waste.py DIR [SIDE]  (every *.repl26a in DIR; SIDE a or b is the team whose log is read, default a)
Columns are shares of logged champion turns: boxed (no walking path to the goal), idle (any other no-op),
wait (farm.wait / guard.hold), walk (walking toward farm, bank or spawn), camp (hitting camps), fight, collect, bank.
"""
import collections
import glob
import os
import re
import sys
import zipfile

LINE = re.compile(r"^\s*r(\d+) c(\d+) \((\d+),(\d+)\) ([\w.]+): (.*)$")


def bucket(branch, text):
    acts = text.rsplit("| ", 1)[-1] if "| " in text else ""
    if "boxed in" in acts:
        return "boxed"
    if acts.startswith("idle") and not branch.startswith(("collect", "channel", "spawn.heal", "win.tribute")):
        return "idle"
    if branch.startswith(("farm.wait", "guard.hold", "farm.camp.hold")):
        return "wait"
    if branch.startswith(("farm.camp", "guard.camp", "retreat.camp", "front.retreat.camp", "mono")):
        return "camp"
    if branch.startswith(("fight", "ping", "chase")):
        return "fight"
    if branch.startswith("collect"):
        return "collect"
    if "bank" in branch or "tribute" in branch or branch.startswith(("channel.tribute", "escort")):
        return "bank"
    if branch.endswith(".walk") or branch.startswith(("spawn.leave", "farm.walk")):
        return "walk"
    return "other"


COLS = ("boxed", "idle", "wait", "walk", "camp", "fight", "collect", "bank", "other")


def main():
    side = sys.argv[2] if len(sys.argv) > 2 else "a"
    total = collections.Counter()
    print("%-26s %6s " % ("game", "turns") + " ".join("%7s" % c for c in COLS))
    for f in sorted(glob.glob(os.path.join(sys.argv[1], "*.repl26a"))):
        text = zipfile.ZipFile(f).read("team-%s.stdout" % side).decode(errors="replace").replace("\x00", "")
        c = collections.Counter()
        for raw in text.splitlines():
            m = LINE.match(raw)
            if m:
                c[bucket(m.group(5), m.group(6))] += 1
        n = max(1, sum(c.values()))
        total.update(c)
        print("%-26s %6d " % (os.path.basename(f)[:-8], n) + " ".join("%6.1f%%" % (100.0 * c[k] / n) for k in COLS))
    n = max(1, sum(total.values()))
    print("%-26s %6d " % ("ALL", n) + " ".join("%6.1f%%" % (100.0 * total[k] / n) for k in COLS))


if __name__ == "__main__":
    main()
