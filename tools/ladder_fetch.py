"""Download finished matches of unranked match groups: python tools/ladder_fetch.py GROUP_IDS_FILE OUT_DIR

GROUP_IDS_FILE has one "group_id name" per line (from `cambc match-groups create ... --json`). Each finished match is
saved as OUT_DIR/<map>-<our side>-<match id 8>.repl26a and .json (summary), the layout tools/ladder.py reads. Prints how
many matches are still queued or running.
"""
import json
import os
import subprocess
import sys

OUR_TEAM = "7b219a14"


def main():
    ids, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    pending = 0
    for line in open(ids):
        parts = line.split()
        if not parts:
            continue
        r = subprocess.run(["cambc", "match-groups", "show", parts[0], "--json"], capture_output=True, text=True)
        try:
            g = json.loads(r.stdout)
        except ValueError:
            print("could not read group", parts[0], r.stdout[:200], r.stderr[:200])
            continue
        for m in g["matches"]:
            if m["status"] != "finished":
                pending += 1
                continue
            us = [t for t in m["teams"] if t["teamId"].startswith(OUR_TEAM)][0]
            stem = os.path.join(out, "%s-%s-%s" % (m["map"].replace("_", "-"), us["side"], m["matchId"][:8]))
            if not os.path.exists(stem + ".repl26a"):
                subprocess.run(["cambc", "matches", "download", m["matchId"], "-o", stem + ".repl26a"], capture_output=True)
            if not os.path.exists(stem + ".json"):
                subprocess.run(["cambc", "matches", "summary", m["matchId"], "--json", "-o", stem + ".json"],
                               capture_output=True)
    print("pending matches:", pending)


if __name__ == "__main__":
    main()
