"""Parse a team's decision log (team-a/b.stdout in a .repl26a) into one record per champion turn.

A turn block starts with `r<round> c<id> (x,y) ...` or `r<round> c<id> dead`; the lines after it with the same
`r<round> ` prefix belong to it: the shared decision (`lead ...` / `follow ...`), the branch (`name: why`), ACT/IDLE,
`note: ...`. Lines without the prefix (`round N: ...`) are team messages.

usage as a module: turns(text) -> list of dict(round, champ, pos, dead, shared, branch, why, acts, idle, notes)
usage as a script: python tools/logparse.py REPLAY a|b [CHAMP] [FROM-TO]  prints that champion's turns.
"""
import re
import sys
import zipfile

HEAD = re.compile(r"^r(\d+) c(\d+) (?:\((\d+),(\d+)\)|dead)(.*)$")
BRANCH = re.compile(r"^([a-z][\w.]*): (.*)$")
CHAMPS = {0: "Ryth", 1: "Vesper", 2: "Vendetta", 3: "Cassi", 4: "Issac", 5: "Stellar", 6: "Prism", 7: "Marcella",
          8: "Willow", 9: "Ren", 10: "Varg", 11: "Tavi"}


def read(replay, side):
    z = zipfile.ZipFile(replay)
    return z.read("team-%s.stdout" % side).decode(errors="replace").replace("\x00", "")


def turns(text):
    out = []
    cur = None
    for raw in text.splitlines():
        line = raw.strip()
        m = HEAD.match(line)
        if m:
            cur = {"round": int(m.group(1)), "champ": int(m.group(2)), "dead": m.group(3) is None,
                   "pos": None if m.group(3) is None else (int(m.group(3)), int(m.group(4))), "tags": m.group(5),
                   "shared": "", "branch": "", "why": "", "acts": "", "idle": "", "notes": [], "extra": []}
            out.append(cur)
            continue
        if cur is None:
            continue
        prefix = "r%d " % cur["round"]
        if not line.startswith(prefix):
            continue
        body = line[len(prefix):]
        if body.startswith(("lead ", "follow ")):
            cur["shared"] = body
        elif body.startswith("ACT "):
            cur["acts"] = body[4:]
        elif body.startswith("IDLE "):
            cur["idle"] = body[5:]
        elif body.startswith("note: "):
            cur["notes"].append(body[6:])
        elif not cur["branch"] and BRANCH.match(body):
            b = BRANCH.match(body)
            cur["branch"], cur["why"] = b.group(1), b.group(2)
        else:
            cur["extra"].append(body)
    return out


if __name__ == "__main__":
    ts = turns(read(sys.argv[1], sys.argv[2]))
    champ = int(sys.argv[3]) if len(sys.argv) > 3 else None
    lo, hi = (int(x) for x in sys.argv[4].split("-")) if len(sys.argv) > 4 else (0, 10 ** 9)
    for t in ts:
        if (champ is None or t["champ"] == champ) and lo <= t["round"] <= hi:
            print("r%d c%d %s | %s | %s: %s | %s %s | %s" % (t["round"], t["champ"], t["pos"], t["shared"][:90], t["branch"],
                                                           t["why"][:120], t["acts"], t["idle"], "; ".join(t["notes"])[:200]))
