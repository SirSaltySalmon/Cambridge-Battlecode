"""Recall outcomes for our side of each game: started, completed, cancelled by us, broken (damage or stun).

usage: python analyze_recalls.py DIR [DIR...]   (games named <map>-a / <map>-b; our side is that letter)
Needs logs with the champion id on each turn line (v81+) and a big --log-limit.
"""
import collections
import glob
import os
import re
import sys
import zipfile

TURN = re.compile(r"^r(\d+) c(\d+) (dead|\(\d+,\d+\).*)$")


def turns(text):
    # Yields (round, champion, where, lines of the turn).
    current = None
    for raw in text.splitlines():
        line = raw.lstrip("\x00 ")
        m = TURN.match(line)
        if m:
            if current:
                yield current
            current = [int(m.group(1)), int(m.group(2)), m.group(3), []]
        elif current:
            current[3].append(line)
    if current:
        yield current


def recalling(lines):
    for line in lines:
        if re.match(r"r\d+ (channel\.recall|hurt\.recall|shop\.recall)", line):
            return True
    return False


def main():
    for dir_ in sys.argv[1:]:
        tally = collections.Counter()
        for path in sorted(glob.glob(os.path.join(dir_, "*.repl26a"))):
            side = "a" if path.endswith("-a.repl26a") else "b"
            text = zipfile.ZipFile(path).read("team-%s.stdout" % side).decode(errors="replace")
            last = {}
            for rnd, champ, where, lines in turns(text):
                prev = last.get(champ)
                if prev and prev[0] == rnd - 1 and prev[1]:
                    aborted = any("aborts recall" in l for l in lines)
                    if where == "dead":
                        tally["died"] += 1
                    elif "on-ally-spawn" in where or any(re.match(r"r\d+ (spawn|win)\.", l) for l in lines):
                        # Arrival is at turn start; the turn line shows where we ended, maybe already off spawn.
                        tally["completed"] += 1
                    elif recalling(lines) and not any(re.match(r"r\d+ (hurt|shop)\.recall", l) for l in lines):
                        pass  # still channelling
                    elif aborted:
                        tally["cancelled"] += 1
                    else:
                        tally["broken"] += 1
                started = any(re.match(r"r\d+ (hurt|shop)\.recall", l) for l in lines)
                if started:
                    tally["started"] += 1
                if any("within its reach" in l or "caster c" in l for l in lines):
                    tally["held_for_caster"] += 1
                last[champ] = (rnd, recalling(lines) and where != "dead")
        print("%-24s started %4d completed %4d broken %4d cancelled %4d died %3d | turns held for an enemy reach %d"
              % (dir_, tally["started"], tally["completed"], tally["broken"], tally["cancelled"], tally["died"],
                 tally["held_for_caster"]))


if __name__ == "__main__":
    main()
