"""Print one stretch of a decoded replay round by round: every living champion's tile, health and what it lost.
usage: python tools/hc/epview.py FILE.jsonl SIDE(a|b) R0 R1 [x y radius]   (ours upper case, theirs lower case)"""
import json, sys
f, side, r0, r1 = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
box = [int(x) for x in sys.argv[5:8]] if len(sys.argv) > 7 else None
N = ['Ryth', 'Vesp', 'Vend', 'Cass', 'Issa', 'Stel', 'Pris', 'Marc', 'Will', 'Ren', 'Varg', 'Tavi', 'HoR']
mine = 0 if side == 'a' else 1; prev = {}
for l in open(f):
    r = json.loads(l)
    if not r['ch']: continue
    if r0 - 1 <= r['r'] <= r1:
        out = []
        for c in sorted(r['ch'], key=lambda c: (c[1] != mine, c[0])):
            if box and max(abs(c[3] - box[0]), abs(c[4] - box[1])) > box[2]: continue
            nm = N[c[2]].upper() if c[1] == mine else N[c[2]].lower()
            if not c[7]: out.append('%s dead' % nm); continue
            dl = c[5] - prev.get(c[0], c[5])
            out.append('%s(%d,%d)%d%%%s%s' % (nm, c[3], c[4], 100 * c[5] / c[6], ('%+d' % dl) if dl else '', '*' if c[11] else ''))
        if r['r'] >= r0: print('r%d ' % r['r'] + ' '.join(out))
    prev = {c[0]: c[5] for c in r['ch']}
