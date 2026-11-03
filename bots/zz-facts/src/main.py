# Throwaway: prints one FACTS line per map (team size, altars, spawn gap, camp gems by zone), then resigns.
import json
import cambc
from cambc import game_map as gm
from cambc import DistanceField, Bitboard


def field_from(roots):
    f = DistanceField()
    seen = Bitboard()
    f.compute(gm.walls() | gm.holes() | gm.altars(), roots, seen)
    return f, seen


def nearest(f, seen, tiles):
    best = None
    for t in tiles:
        if seen.is_set(t):
            d = f.get(t)
            if best is None or d < best:
                best = d
    return 255 if best is None else best


def facts():
    ours, theirs = gm.spawn_region(cambc.Team.ALLY), gm.spawn_region(cambc.Team.ENEMY)
    fo, so = field_from(ours)
    ft, st = field_from(theirs)
    out = dict(w=gm.width(), h=gm.height(), size=cambc.draft.team_size(), altars=len(gm.altars().positions()),
               gap=nearest(fo, so, theirs.positions()), safe=0, unsafe=0, mid=0, near12=0, gems=0, monsters=0, kinds={})
    for g in gm.camp_groups():
        tiles = g.spawn_tiles().positions()
        a, b = nearest(fo, so, tiles), nearest(ft, st, tiles)
        gems = g.gem_value()
        out["gems"] += gems
        for c in g.camps():
            k = str(c.kind()).split(".")[-1]
            out["kinds"][k] = out["kinds"].get(k, 0) + 1
        zone = "safe" if a * 1.5 <= b else "unsafe" if b * 1.5 <= a else "mid"
        out[zone] += gems
        if a <= 12:
            out["near12"] += gems
    altar_steps = [nearest(fo, so, [p for p in [cambc.Vec2(x.x + dx, x.y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)]
                                    if gm.is_in_bounds(p)]) for x in gm.altars().positions()]
    out["altar_ours"] = altar_steps
    altar_theirs = [nearest(ft, st, [p for p in [cambc.Vec2(x.x + dx, x.y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)]
                                     if gm.is_in_bounds(p)]) for x in gm.altars().positions()]
    out["altar_theirs"] = altar_theirs
    return out


done = False


def main():
    global done
    while True:
        try:
            if cambc.is_draft():
                if not done:
                    done = True
                    print("FACTS " + json.dumps(facts()), flush=True)
                ch = cambc.draft.current_draft(cambc.Team.ALLY)
                cambc.draft.pick([p for p in cambc.ChampionDraft if p not in ch][0])
            else:
                cambc.resign()
        except Exception as e:
            print("err", repr(e), flush=True)
        cambc.end_turn()


if __name__ == "__main__":
    main()
