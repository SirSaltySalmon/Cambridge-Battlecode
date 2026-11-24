"""Kill conversion from decoded platform replays: of champions that fall under LOW of max health with an opposing
champion within 6 tiles, the share dead within 15 rounds, by side; and where the survivors went.
usage: python tools/hc/finish.py DIR [win|loss|all] [LOW=0.4]"""
import collections, json, os, sys
d = sys.argv[1]; which = sys.argv[2] if len(sys.argv) > 2 else 'all'; LOW = float(sys.argv[3]) if len(sys.argv) > 3 else 0.4
meta = json.load(open(d + '/meta.json'))
N = ['Ryth', 'Vesper', 'Vend', 'Cassi', 'Issac', 'Stell', 'Prism', 'Marc', 'Willow', 'Ren', 'Varg', 'Tavi', 'HoR']
def cheb(a, b, c, e): return max(abs(a - c), abs(b - e))
res = {'us': collections.Counter(), 'th': collections.Counter()}; kinds = {'us': collections.Counter(), 'th': collections.Counter()}
bod = {'us': collections.defaultdict(lambda: [0, 0]), 'th': collections.defaultdict(lambda: [0, 0])}
games = 0
for nm, m in sorted(meta.items()):
    if which == 'win' and not m['win'] or which == 'loss' and m['win']: continue
    f = os.path.join(d, nm + '.jsonl')
    if not os.path.exists(f): continue
    rows = [json.loads(l) for l in open(f) if l.strip()]; rows = [r for r in rows if r['ch']]
    if not rows: continue
    games += 1; mine = 0 if m['side'] == 'a' else 1
    armed = {}
    for i, r in enumerate(rows):
        live = [c for c in r['ch'] if c[7]]
        for c in live:
            low = c[5] < LOW * c[6]
            foes = [e for e in live if e[1] != c[1] and cheb(c[3], c[4], e[3], e[4]) <= 6]
            if not low: armed[c[0]] = True
            if low and foes and armed.get(c[0], True):
                armed[c[0]] = False
                side = 'us' if c[1] == mine else 'th'
                mates = sum(1 for a in live if a[1] == c[1] and a[0] != c[0] and cheb(c[3], c[4], a[3], a[4]) <= 6)
                dead = False
                for r2 in rows[i + 1:i + 16]:
                    c2 = [x for x in r2['ch'] if x[0] == c[0]]
                    if c2 and not c2[0][7]: dead = True; break
                res[side]['low'] += 1; res[side]['dead'] += dead
                kinds[side][N[c[2]] + ('+' if dead else '-')] += 1
                key = (min(mates + 1, 3), min(len(foes), 3))
                bod[side][key][0] += 1; bod[side][key][1] += dead
print('%d games (%s), under %.0f%% health with an enemy within 6' % (games, which, LOW * 100))
for side in ('us', 'th'):
    v = res[side]
    print('  %s: %d times (%.1f a game), died within 15 rounds %d (%.0f%%)' % ('ours  ' if side == 'us' else 'theirs', v['low'], v['low'] / max(1, games), v['dead'], 100 * v['dead'] / max(1, v['low'])))
    print('     by kind (+ died, - lived):', ' '.join('%s %d' % kv for kv in sorted(kinds[side].items())))
    print('     victim side bodies x hunters (died/total):', '  '.join('%dv%d %d/%d' % (k[0], k[1], x[1], x[0]) for k, x in sorted(bod[side].items())))
