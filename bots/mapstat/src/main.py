import cambc
from cambc import game_map as gm

done = False


def stats():
    # Consistent gem sources only: wolves 4, elks 15, gem clusters 7 (no golems, no monoliths).
    per = {"wolf": (4, 40), "elk": (15, 60), "gem_cluster": (7, 50)}
    count = {}
    gems = rate = 0.0
    for g in gm.camp_groups():
        for c in g.camps():
            k = str(c.kind()).split(".")[-1].lower()
            count[k] = count.get(k, 0) + 1
            if k in per:
                gems += per[k][0]
                rate += per[k][0] / (per[k][1] + 5)
    print("STAT size=%d gems=%d rate=%.2f %s" % (cambc.draft.team_size(), gems, rate,
          " ".join("%s=%d" % kv for kv in sorted(count.items()))), flush=True)


def main():
    global done
    while True:
        try:
            if cambc.is_draft():
                if not done:
                    done = True
                    stats()
                ch = cambc.draft.current_draft(cambc.Team.ALLY)
                cambc.draft.pick([p for p in range(11) if p not in ch][0])
        except Exception as e:
            print("err", e, flush=True)
        cambc.end_turn()


if __name__ == "__main__":
    main()
