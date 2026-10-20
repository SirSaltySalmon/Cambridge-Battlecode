"""Queue unranked match groups: python tools/ladder_queue.py OPPONENTS_FILE GROUPS_OUT

OPPONENTS_FILE has one "team_id name" per line. Each team gets one group of five maps; the maps rotate through the
17-map pool so every map is played about equally often. GROUPS_OUT gets one "group_id name" line per group, the file
tools/ladder_fetch.py reads. Each group uses one of the team's 50 unranked groups per reset.
"""
import json
import subprocess
import sys

MAPS = ["ambush", "collision", "contest", "corners", "eight", "elkring", "expanse", "fragment", "interlock", "lanes",
        "monolith", "packs", "pilgrimage", "rows", "rushbait_v2", "stampede", "swirl"]


def group_id(data):
    for key in ("groupId", "id", "matchGroupId"):
        if isinstance(data, dict) and key in data:
            return data[key]
    if isinstance(data, dict):
        for value in data.values():
            found = group_id(value)
            if found:
                return found
    return None


def main():
    teams = [line.split(None, 1) for line in open(sys.argv[1], encoding="utf-8") if line.strip()]
    with open(sys.argv[2], "a", encoding="utf-8") as out:
        for i, (team, name) in enumerate(teams):
            maps = ",".join(MAPS[(5 * i + k) % len(MAPS)] for k in range(5))
            r = subprocess.run(["cambc", "match-groups", "create", team, "--maps", maps, "--json"],
                               capture_output=True, text=True, encoding="utf-8")
            try:
                gid = group_id(json.loads(r.stdout))
            except ValueError:
                gid = None
            if not gid:
                print("FAILED", name.strip(), r.stdout[:300], r.stderr[:300])
                continue
            out.write("%s %s\n" % (gid, name.strip()))
            out.flush()
            print(gid, name.strip(), maps)


if __name__ == "__main__":
    main()
