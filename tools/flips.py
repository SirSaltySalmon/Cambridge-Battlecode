"""Count shared-mode flips in a team's log: python tools/flips.py DIR [a|b|both]

Reads the leader's `round N: <mode>, route slot S` lines. A flip is a mode change; a bounce is a change that is undone
within BOUNCE rounds (farm -> retreat -> farm). Prints per-game counts and the pair totals.
"""
import collections
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import logparse

LINE = re.compile(r"^round (\d+): (farm|fight|retreat|tribute), route slot (\d+)")
BOUNCE = 3


def flips(text):
    seq = []
    for raw in text.splitlines():
        m = LINE.match(raw.strip("\x00 \r"))
        if m:
            r, mode = int(m.group(1)), m.group(2)
            if not seq or seq[-1][1] != mode:
                seq.append((r, mode))
    pairs = collections.Counter()
    bounces = collections.Counter()
    for i in range(1, len(seq)):
        pairs[(seq[i - 1][1], seq[i][1])] += 1
        if i + 1 < len(seq) and seq[i + 1][1] == seq[i - 1][1] and seq[i + 1][0] - seq[i][0] <= BOUNCE:
            bounces[(seq[i - 1][1], seq[i][1])] += 1
    return len(seq) - 1, pairs, bounces


if __name__ == "__main__":
    d = sys.argv[1]
    which = sys.argv[2] if len(sys.argv) > 2 else "us"
    tp, tb = collections.Counter(), collections.Counter()
    total = 0
    for f in sorted(glob.glob(os.path.join(d, "*.repl26a"))):
        side = "a" if f.endswith("-a.repl26a") else "b"
        n, p, b = flips(logparse.read(f, side))
        total += n
        tp.update(p)
        tb.update(b)
        print("%-20s flips %4d bounces %4d" % (os.path.basename(f), n, sum(b.values())))
    print("total flips %d, bounces %d" % (total, sum(tb.values())))
    for k, v in tp.most_common():
        print("  %s -> %s: %d (bounced back within %d rounds: %d)" % (k[0], k[1], v, BOUNCE, tb[k]))


def slot_bounces(text):
    """Target changes, and target changes undone within BOUNCE rounds (A -> B -> A)."""
    seq = []
    for raw in text.splitlines():
        m = LINE.match(raw.strip("\x00 \r"))
        if m:
            r, s = int(m.group(1)), int(m.group(3))
            if not seq or seq[-1][1] != s:
                seq.append((r, s))
    back = sum(1 for i in range(1, len(seq) - 1) if seq[i + 1][1] == seq[i - 1][1] and seq[i + 1][0] - seq[i][0] <= BOUNCE)
    return len(seq) - 1, back
