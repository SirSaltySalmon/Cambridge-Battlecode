"""Deaths with healthy allies standing by, from decoded platform replays. For each death to champions (an opposing
champion within 6 tiles in the 3 rounds before): over the 6 rounds before it, the victim's side bodies within 5 tiles
(in the fight) and those 6-14 tiles away, alive, over 50% health, that neither dealt nor took champion damage (standing
by), against the killers' bodies within 6. By side.
usage: python tools/hc/idle_allies.py DIR [win|loss|all] [R1=2000]"""
import collections, json, os, sys
d = sys.argv[1]; which = sys.argv[2] if len(sys.argv) > 2 else 'all'; R1 = int(sys.argv[3]) if len(sys.argv) > 3 else 2000
meta = json.load(open(d + '/meta.json'))
def cheb(a, b, c, e): return max(abs(a - c), abs(b - e))
res = {'us': collections.Counter(), 'th': collections.Counter()}; tab = {'us': collections.Counter(), 'th': collections.Counter()}; games = 0
for nm, m in sorted(meta.items()):
    if which == 'win' and not m['win'] or which == 'loss' and m['win']: continue
    f = os.path.join(d, nm + '.jsonl')
    if not os.path.exists(f): continue
    rows = [json.loads(l) for l in open(f) if l.strip()]; rows = [r for r in rows if r['ch']]
    if not rows: continue
    games += 1; mine = 0 if m['side'] == 'a' else 1
    for i in range(7, len(rows)):
        r = rows[i]; p = rows[i - 1]
        if r['r'] > R1: break
        pa = {c[0]: c for c in p['ch']}
        for c in r['ch']:
            if c[7] or not pa[c[0]][7]: continue
            v = pa[c[0]]; side = 'us' if c[1] == mine else 'th'
            foes = [e for e in p['ch'] if e[7] and e[1] != c[1] and cheb(e[3], e[4], v[3], v[4]) <= 6]
            if not foes: res[side]['camp or unseen'] += 1; continue
            b = rows[i - 4]
            vb = [x for x in b['ch'] if x[0] == c[0]][0]
            if not vb[7]: vb = v; b = p
            fo = sum(1 for e in b['ch'] if e[7] and e[1] != c[1] and cheb(e[3], e[4], vb[3], vb[4]) <= 6)
            inn = 1 + sum(1 for x in b['ch'] if x[7] and x[1] == c[1] and x[0] != c[0] and cheb(x[3], x[4], vb[3], vb[4]) <= 5)
            by = 0
            for x in b['ch']:
                if not x[7] or x[1] != c[1] or x[0] == c[0]: continue
                g = cheb(x[3], x[4], vb[3], vb[4])
                if g <= 5 or g > 14 or x[5] < 0.5 * x[6]: continue
                # stood by: no enemy within 4 of it during the 6 rounds before the death
                busy = False
                for rr in rows[i - 6:i]:
                    xx = [y for y in rr['ch'] if y[0] == x[0]][0]
                    if any(e[7] and e[1] != c[1] and cheb(e[3], e[4], xx[3], xx[4]) <= 4 for e in rr['ch']): busy = True
                if not busy: by += 1
            res[side]['deaths to champions'] += 1
            res[side]['outnumbered locally'] += fo > inn
            res[side]['with 1+ healthy ally standing by 6-14 tiles off'] += by > 0
            res[side]['outnumbered locally, and standers-by would have evened it'] += fo > inn and inn + by >= fo
            res[side]['standers-by (sum)'] += by
            tab[side][(min(inn, 4), min(fo, 4))] += 1
print('%d games (%s), to round %d' % (games, which, R1))
for side in ('us', 'th'):
    v = res[side]; n = max(1, v['deaths to champions'])
    print(' %s: %d deaths to champions (%.1f a game), %d to camps/unseen' % ('ours  ' if side == 'us' else 'theirs', v['deaths to champions'], v['deaths to champions'] / max(1, games), v['camp or unseen']))
    for k in ('outnumbered locally', 'with 1+ healthy ally standing by 6-14 tiles off', 'outnumbered locally, and standers-by would have evened it'):
        print('     %4d (%2.0f%%) %s' % (v[k], 100 * v[k] / n, k))
    print('     victim side in the fight (rows 1..4) x killers within 6 (cols 1..4): ' + ' | '.join(' '.join('%3d' % tab[side][(a, b)] for b in range(1, 5)) for a in range(1, 5)))
