"""Summarise PROF lines: mean fuel per turn per function (inclusive), calls per turn, fuel per call, and the
same over heavy turns (total >= HEAVY). usage: python profsum.py STDOUT... [--heavy 70000] [--top 45]"""
import collections
import re
import sys

args = [a for a in sys.argv[1:] if a not in ("--self", "--exh")]
heavy = 70000
top = 45
if "--heavy" in args:
    i = args.index("--heavy"); heavy = int(args[i + 1]); del args[i:i + 2]
if "--top" in args:
    i = args.index("--top"); top = int(args[i + 1]); del args[i:i + 2]
tot = collections.Counter(); own = collections.Counter(); hown = collections.Counter(); calls = collections.Counter(); htot = collections.Counter(); hcalls = collections.Counter()
turns = hturns = exhausted = 0
fuel_sum = 0
for path in args:
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.replace(chr(0), "").strip()
        if not line.startswith("PROF"):
            continue
        if "EXHAUSTED" in line:
            exhausted += 1
            continue
        if line.startswith("PROFX"):
            exhausted += 1
            if "--exh" not in sys.argv:
                continue
            line = "PROF" + line[5:]
        elif "--exh" in sys.argv:
            continue
        m = re.match(r"PROF r\d+ c\d+ total (\d+)(.*)", line)
        t = int(m.group(1))
        if t > 100000 or (t == 100000 and "--exh" not in sys.argv):  # first turn
            continue
        turns += 1
        fuel_sum += t
        isheavy = t >= heavy
        hturns += isheavy
        for name, f, o, c in re.findall(r" ([\w.]+)=(\d+)/(\d+)/(\d+)", m.group(2)):
            tot[name] += int(f); own[name] += int(o); calls[name] += int(c)
            if isheavy:
                htot[name] += int(f); hown[name] += int(o); hcalls[name] += int(c)
print("turns %d, mean %.0f fuel, heavy (>=%d) %d, exhausted %d" % (turns, fuel_sum / turns, heavy, hturns, exhausted))
key = own if "--self" in sys.argv else tot
print("%-26s %9s %9s %9s %9s | %9s %9s %9s" % ("function", "incl/turn", "self/turn", "calls/t", "self/call", "heavy inc", "heavy self", "heavy c/t"))
for name, f in key.most_common(top):
    print("%-26s %9.0f %9.0f %9.2f %9.0f | %9.0f %9.0f %9.2f" % (name, tot[name] / turns, own[name] / turns, calls[name] / turns,
          own[name] / max(1, calls[name]), htot[name] / max(1, hturns), hown[name] / max(1, hturns), hcalls[name] / max(1, hturns)))
