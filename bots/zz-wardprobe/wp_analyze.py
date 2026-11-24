import zipfile, glob, collections, sys
d = sys.argv[1]
place = collections.Counter(); gain = collections.defaultdict(list); rev = collections.Counter()
hits = collections.Counter(); hit_src = collections.Counter()
ghost_eps = 0; ghost_eps_blocked = 0; ghost_eps_blocked_never = 0; block_src = collections.Counter(); block_age = []
hidden_blocked = 0; hidden_src = collections.Counter(); stat = collections.Counter()
ref_total = 0; ref_tiles = collections.Counter(); per_game_kind = collections.Counter()
for f in glob.glob(d + "/*.repl26a"):
    side = "team-a" if f.endswith("-a.repl26a") else "team-b"
    t = zipfile.ZipFile(f).read(side + ".stdout").decode(errors="replace")
    last = {}; laststat = {}; ep = {}  # ep[c] = [start, end, blocked, placed]
    def close(c):
        global ghost_eps, ghost_eps_blocked, ghost_eps_blocked_never
        e = ep.pop(c, None)
        if not e: return
        ghost_eps += 1
        if e[2]: ghost_eps_blocked += 1
        if e[2] and not e[3]: ghost_eps_blocked_never += 1
    for line in t.splitlines():
        if "WP r" not in line: continue
        p = line.split("WP ", 1)[1].split(); r = int(p[0][1:]); c = p[1]; what = p[2]
        if what == "stat": laststat[c] = dict(kv.split("=") for kv in p[3:]); continue
        if c in ep and r - ep[c][1] > 1: close(c)
        if what == "place":
            kind = p[3]; kv = dict(x.split("=") for x in p[4:])
            if kv.get("ok", "1") == "0": continue
            place[kind] += 1; rev[kind] += int(kv.get("rev", 0)) > 0
            if "gain" in kv: gain[kind].append(int(kv["gain"]))
            if kind == "ghost":
                if c not in ep: ep[c] = [r, r, False, True]
                ep[c][1] = r; ep[c][3] = True
            last[c] = (kind, r)
        elif what == "scout":
            ref_total += 1; ref_tiles[(f, c, p[4], p[5])] += 1
        elif what == "blocked" and p[3] == "ghost":
            if c not in ep:
                ep[c] = [r, r, True, False]; src = last.get(c, ("none", -99)); block_src[src[0]] += 1; block_age.append(r - src[1])
            ep[c][1] = r; ep[c][2] = True
        elif what == "blocked":
            hidden_blocked += 1; hidden_src[last.get(c, ("none", 0))[0]] += 1
        elif what == "hit":
            only = p[4].endswith("=1"); hits[(p[3], only)] += 1
            if only: hit_src[last.get(c, ("none", 0))[0]] += 1
    for c in list(ep): close(c)
    for c, s in laststat.items():
        for k, v in s.items(): stat[k] += int(v)
print("placements", dict(place))
print("mean gain", {k: round(sum(v)/len(v), 1) for k, v in gain.items()}, "zero-gain share", {k: round(sum(1 for x in v if x == 0)/len(v), 2) for k, v in gain.items()})
print("reveal-at-once", {k: "%d (%.1f%%)" % (rev[k], 100*rev[k]/place[k]) for k in place})
print("turns", dict(stat), "ready %.2f" % (stat["ready"]/stat["turns"]))
tot = sum(hits.values()); wo = sum(v for (k, o), v in hits.items() if o)
print("champion hits", tot, "needing ward", wo, "%.1f%%" % (100*wo/tot), dict(hits))
print("ward-needed hits by owner's last placement", dict(hit_src), {k: round(hit_src[k]/place[k], 3) for k in place})
print("ghost episodes", ghost_eps, "blocked at start", ghost_eps_blocked, "blocked and never warded", ghost_eps_blocked_never)
print("charge spent by", dict(block_src)); block_age.sort()
if block_age: print("rounds since that placement: median", block_age[len(block_age)//2], "p25", block_age[len(block_age)//4], "p75", block_age[3*len(block_age)//4])
print("hidden-attacker blocked turns", hidden_blocked, dict(hidden_src))
reps = sorted(ref_tiles.values(), reverse=True)
print("scout refusals", ref_total, "distinct (game,champ,tile)", len(ref_tiles), "top repeats", reps[:8], "share from tiles refused 5+ times %.2f" % (sum(v for v in reps if v >= 5)/max(1, ref_total)))
