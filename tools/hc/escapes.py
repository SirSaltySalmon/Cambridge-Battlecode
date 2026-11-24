"""How low-health champions get away, from decoded platform replays. A case starts when a champion falls under LOW of
max health with an opposing champion within 6 tiles and lives 15 more rounds. Per case, over the next 10 rounds: the
gap to the nearest opposing champion each round, health taken after, and whether the hunters closed or fell back.
usage: python tools/hc/escapes.py DIR [win|loss|all] [them|us] [LOW=0.4] [-v]"""
import collections, json, os, sys
a = [x for x in sys.argv[1:] if x != '-v']; verbose = '-v' in sys.argv
d = a[0]; which = a[1] if len(a) > 1 else 'all'; who = a[2] if len(a) > 2 else 'them'; LOW = float(a[3]) if len(a) > 3 else 0.4
meta = json.load(open(d + '/meta.json'))
N = ['Ryth', 'Vesper', 'Vend', 'Cassi', 'Issac', 'Stell', 'Prism', 'Marc', 'Willow', 'Ren', 'Varg', 'Tavi', 'HoR']
RANGED = {1, 3, 4, 5, 7, 10}
def cheb(a, b, c, e): return max(abs(a - c), abs(b - e))
cls = collections.Counter(); n = 0; games = 0; ex = []
hunters_kind = collections.Counter()
for nm, m in sorted(meta.items()):
    if which == 'win' and not m['win'] or which == 'loss' and m['win']: continue
    f = os.path.join(d, nm + '.jsonl')
    if not os.path.exists(f): continue
    rows = [json.loads(l) for l in open(f) if l.strip()]; rows = [r for r in rows if r['ch']]
    if not rows: continue
    games += 1; mine = 0 if m['side'] == 'a' else 1
    spawn = {c[0]: (c[3], c[4]) for c in rows[0]['ch']}
    armed = {}
    for i, r in enumerate(rows):
        live = [c for c in r['ch'] if c[7]]
        for c in live:
            if (c[1] == mine) != (who == 'us'): continue
            low = c[5] < LOW * c[6]
            foes = [e for e in live if e[1] != c[1] and cheb(c[3], c[4], e[3], e[4]) <= 6]
            if not low: armed[c[0]] = True
            if not (low and foes and armed.get(c[0], True)): continue
            armed[c[0]] = False
            fut = rows[i + 1:i + 16]
            if any((not x[7]) for r2 in fut for x in r2['ch'] if x[0] == c[0]): continue
            n += 1
            gaps = []; hp = [c[5]]; home = []; hunters_hp = []
            for r2 in fut[:10]:
                me = [x for x in r2['ch'] if x[0] == c[0]][0]
                fo = [e for e in r2['ch'] if e[7] and e[1] != c[1]]
                gaps.append(min([cheb(me[3], me[4], e[3], e[4]) for e in fo] or [99]))
                hp.append(me[5]); home.append(cheb(me[3], me[4], *spawn[c[0]]))
            g0 = min(cheb(c[3], c[4], e[3], e[4]) for e in foes)
            taken = sum(max(0, hp[k] - hp[k + 1]) for k in range(len(hp) - 1))
            healed = any(hp[k + 1] > hp[k] + 50 for k in range(len(hp) - 1))
            nh = len(foes); nearest = min(foes, key=lambda e: cheb(c[3], c[4], e[3], e[4]))
            hunter_low = sum(1 for e in foes if e[5] < 0.4 * e[6])
            allm = sum(1 for x in live if x[1] == c[1] and x[0] != c[0] and cheb(c[3], c[4], x[3], x[4]) <= 6)
            # classes
            if home and min(home) <= 2 and len(gaps) >= 3: k = 'reached its spawn within 10 rounds'
            elif len(gaps) >= 3 and gaps[2] >= g0 + 3: k = 'gap opened 3+ tiles in 3 rounds (dash, or hunters turned away)'
            elif len(gaps) >= 6 and max(gaps[:6]) <= 4: k = 'hunters stayed within 4 for 6 rounds, no kill'
            elif len(gaps) >= 6 and gaps[5] > gaps[0]: k = 'gap grew slowly (out-walked)'
            else: k = 'other'
            cls[k] += 1
            cls[('hunters', min(nh, 3), 'mates', min(allm, 2))] += 0
            hunters_kind[('ranged' if nearest[2] in RANGED else 'melee') + ' nearest hunter'] += 1
            hunters_kind['all hunters low themselves'] += hunter_low == nh
            hunters_kind['victim took more damage after'] += taken > 0
            hunters_kind['victim had mates within 6'] += allm > 0
            if verbose: ex.append('%s r%d %s hp %d/%d g0 %d hunters %d (%s) mates %d gaps %s taken %d %s' % (nm[:28], r['r'], N[c[2]], c[5], c[6], g0, nh, ','.join(N[e[2]] + ('%d%%' % (100 * e[5] / e[6])) for e in foes), allm, gaps, taken, k[:20]))
print('%d games (%s): %d low %s champions lived (%.1f a game)' % (games, which, n, 'enemy' if who == 'them' else 'of our', n / max(1, games)))
for k, v in cls.most_common():
    if isinstance(k, str): print('  %3d (%.0f%%) %s' % (v, 100 * v / max(1, n), k))
for k, v in hunters_kind.most_common(): print('     %3d (%.0f%%) %s' % (v, 100 * v / max(1, n), k))
for e in ex[:60]: print('   ', e)
