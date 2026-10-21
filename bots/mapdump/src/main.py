import cambc
from cambc import game_map as gm

done = False
def sset(b):
    return {(p.x, p.y) for p in b.positions()}
def dump():
    W, H = gm.width(), gm.height()
    walls, holes, bushes, altars = sset(gm.walls()), sset(gm.holes()), sset(gm.bushes()), sset(gm.altars())
    sa = sset(gm.spawn_region(cambc.Team.ALLY)) | sset(gm.spawn_region(cambc.Team.ENEMY))
    camps = {}
    groups = gm.camp_groups()
    for gi, g in enumerate(groups):
        for t in sset(g.spawn_tiles()): camps[t] = gi
    print("MAP", W, H, flush=True)
    for y in range(H):
        row = []
        for x in range(W):
            t = (x, y)
            if t in camps: gi = camps[t]; ch = chr(ord('a') + gi) if gi < 26 else chr(ord('A') + gi - 26)
            elif t in walls: ch = '#'
            elif t in holes: ch = 'o'
            elif t in altars: ch = '@'
            elif t in sa: ch = 'S'
            elif t in bushes: ch = '"'
            else: ch = '.'
            row.append(ch)
        print("%02d %s" % (y, "".join(row)))
    for gi, g in enumerate(groups):
        print("G", gi, len(g), g.gem_value(), g.gold_value(), sorted(sset(g.spawn_tiles())))

def main():
    global done
    while True:
        try:
            if cambc.is_draft():
                ch = cambc.draft.current_draft(cambc.Team.ALLY)
                cambc.draft.pick([p for p in range(11) if p not in ch][0])
            elif not done:
                done = True
                dump()
        except Exception as e:
            print("err", e, flush=True)
        cambc.end_turn()

if __name__ == "__main__":
    main()
