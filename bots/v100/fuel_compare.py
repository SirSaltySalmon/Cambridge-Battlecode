"""Fuel per champion turn from replays alone (no stats JSON), grouped by a tag in the file name.

usage: python fuel_compare.py DIR SIDE TAG [TAG...]
Each *.repl26a whose name contains -TAG. (e.g. corners-new.repl26a) counts for that tag; SIDE (a or b) is the team
read. A turn of exactly 100000 ran out of fuel (see analyze_errors.py for the .fuel format). Entries over 100000 are the
draft and first turns (bigger budgets) and are left out.
"""
import glob
import os
import struct
import sys
import zipfile

LIMIT = 100000


def turn_fuel(raw):
    values = [struct.unpack_from("<Q", raw, 13 + 8 * i)[0] for i in range((len(raw) - 13) // 8)]
    rest = values[8:]
    start = rest.index(2 * LIMIT) if 2 * LIMIT in rest else 0
    while start < len(rest) and rest[start] == 2 * LIMIT:
        start += 1
    return rest[start:]


def main():
    d, side, tags = sys.argv[1], sys.argv[2], sys.argv[3:]
    for tag in tags:
        used, per_map = [], []
        for f in sorted(glob.glob(os.path.join(d, "*-%s.repl26a" % tag))):
            u = [x for x in turn_fuel(zipfile.ZipFile(f).read("team-%s.fuel" % side)) if x <= LIMIT]
            used.extend(u)
            per_map.append("%s %.0f/%d" % (os.path.basename(f).split("-%s" % tag)[0], sum(u) / max(1, len(u)),
                                           sum(1 for x in u if x == LIMIT)))
        used.sort()
        n = max(1, len(used))
        print("%-8s turns %6d mean %6.0f median %6d p95 %6d p99 %6d >=90k %4d out-of-fuel %4d" % (
            tag, len(used), sum(used) / n, used[n // 2], used[int(.95 * n)], used[int(.99 * n)],
            sum(1 for x in used if x >= 90000), sum(1 for x in used if x == LIMIT)))
        print("         " + ", ".join(per_map))


if __name__ == "__main__":
    main()
