"""MAGE_FINISH_SHARE audit: farming pace and the risk to the casters, per bot.

usage: python analyze_finish.py NEW OLD DIR [DIR...]  (NEW is team A in <map>-a and team B in <map>-b)

Farming: camp groups cleared, gems collected from camps, camp damage done (team and casters), mage turns on the cheap
finish branch (".finish": basic attacks inside the camp's reach). Risk: caster camp damage taken, deaths to camps
(death context "camp"), caster recalls and dodges for health.
"""
import collections
import glob
import json
import os
import re
import sys
import zipfile

CASTERS = ("Cassi", "Stellar", "Willow")
MAGE_IDS = {"c3": "Cassi", "c5": "Stellar", "c8": "Willow"}  # champ_id order: cassi 3, stellar 5, willow 8
TURN = re.compile(r"^r\d+ (c\d+) (?:dead|\(\d+,\d+\))(?: [A-Z-]+)* ([a-z][\w.]*): ")


def main():
    new, old = sys.argv[1], sys.argv[2]
    files = sorted(f for d in sys.argv[3:] for f in glob.glob(os.path.join(d, "*.json")))
    t = collections.defaultdict(collections.Counter)
    per_map = collections.defaultdict(dict)
    for f in files:
        d = json.load(open(f))
        game = os.path.basename(f)[:-5]
        new_side = "a" if game.endswith("-a") else "b"
        bot_of = {"team" + s.upper(): (new if s == new_side else old) for s in "ab"}
        names = {r["id"]: r for r in d["roster"]["champions"]}
        gems = {x["team"]: int(x["submittedGems"]) for x in d["teams"]}
        for team, bot in bot_of.items():
            t[bot].update(games=1, wins=d["match"]["outcome"] == team, gems=gems[team])
            per_map[game][bot] = gems[team]
        for c in d["champions"]:
            r = names[c["id"]]
            bot, m = bot_of[r["team"]], c["metrics"]
            t[bot].update(camp_done=float(m["monsterDamageDone"]), camp_taken=float(m["monsterDamageTaken"]))
            if r["name"] in CASTERS:
                t[bot].update(c_done=float(m["monsterDamageDone"]), c_taken=float(m["monsterDamageTaken"]),
                              c_deaths=int(m["deaths"]))
        for e in d["events"]:
            team = e["owner"].get("team")
            if team not in bot_of:
                continue
            bot = bot_of[team]
            name = e["owner"].get("name")
            n = len(e["entries"])
            if e["kind"] == "campClear":
                t[bot].update(clears=n)
            elif e["kind"] == "gemCollection":
                t[bot].update(camp_gems=sum(int(x["value"]) for x in e["entries"] if x.get("context") == "campGroup"))
            elif e["kind"] == "death":
                to_camp = sum(1 for x in e["entries"] if x.get("context") == "camp")
                t[bot].update(camp_deaths=to_camp)
                if name in CASTERS:
                    t[bot].update(c_camp_deaths=to_camp)
        # Mage branch counts from the logs (v90-log format).
        repl = f[:-5] + ".repl26a"
        if os.path.exists(repl):
            z = zipfile.ZipFile(repl)
            for side in "ab":
                bot = bot_of["team" + side.upper()]
                for raw in z.read("team-%s.stdout" % side).decode(errors="replace").splitlines():
                    mt = TURN.match(raw.lstrip("\x00 "))
                    if not mt or mt.group(1) not in MAGE_IDS:
                        continue
                    branch = mt.group(2)
                    if MAGE_IDS[mt.group(1)] in ("Cassi", "Stellar"):
                        if ".camp" in branch:
                            t[bot].update(mage_camp_turns=1)
                        if branch.endswith(".finish"):
                            t[bot].update(finish_turns=1)
                        if branch.endswith((".standoff", ".cooldown", ".leave", ".closing")):
                            t[bot].update(standoff_turns=1)
                    if branch in ("hurt.recall", "hurt.dodge", "band.recall"):
                        t[bot].update(c_hurt=1)
    for bot in (new, old):
        x = t[bot]
        g = max(1, x["games"])
        print("%s: %d-%d, gems %d" % (bot, x["wins"], x["games"] - x["wins"], x["gems"]))
        print("  farming: %d camp groups cleared (%.1f/game), %d gems from camps, camp damage done %.0f/game (casters %.0f)"
              % (x["clears"], x["clears"] / g, x["camp_gems"], x["camp_done"] / g, x["c_done"] / g))
        print("  mage camp turns %d: cheap finish %d (%.1f%%), standoff %d (%.1f%%)" % (
            x["mage_camp_turns"], x["finish_turns"], 100.0 * x["finish_turns"] / max(1, x["mage_camp_turns"]),
            x["standoff_turns"], 100.0 * x["standoff_turns"] / max(1, x["mage_camp_turns"])))
        print("  risk: caster camp damage taken %.0f/game (team %.0f), deaths to camps %d (casters %d), caster deaths %d,"
              " caster hurt recalls/dodges %d" % (x["c_taken"] / g, x["camp_taken"] / g, x["camp_deaths"],
                                                 x["c_camp_deaths"], x["c_deaths"], x["c_hurt"]))
    diffs = [(v[new] - v[old], k) for k, v in per_map.items() if new in v and old in v]
    print("\ngames where %s banked more / less / same: %d / %d / %d" % (
        new, sum(1 for dlt, _ in diffs if dlt > 0), sum(1 for dlt, _ in diffs if dlt < 0), sum(1 for dlt, _ in diffs if dlt == 0)))


if __name__ == "__main__":
    main()
