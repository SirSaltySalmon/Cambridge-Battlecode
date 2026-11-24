"""Contact episodes and turn shares from decoded platform replays (tools/hc/platreps.py output), us against them.

usage: python tools/hc/contacts.py DIR [win|loss|all] [R1=2000] [R0=0]

Turn classes per living champion and round (first that fits):
  camp     a camp within 4 tiles lost health this round
  fight    an enemy champion within 4 tiles and one of the two lost health
  contact  an enemy champion within 8 tiles (in sight), nobody hurt: standing off, chasing or backing away
  spawn    within 2 tiles of its spawn tile
  move / still
A contact episode is a run of rounds in which some pair of opposing champions is within 6 tiles (gaps of up to 5
rounds join). Per episode: rounds, bodies each side (peak within 8 of the pair), kills each way, health lost each way.
"""
import collections
import json
import os
import sys

d = sys.argv[1]
which = sys.argv[2] if len(sys.argv) > 2 else 'all'
R1 = int(sys.argv[3]) if len(sys.argv) > 3 else 2000
R0 = int(sys.argv[4]) if len(sys.argv) > 4 else 0
meta = json.load(open(d + '/meta.json'))
N = ['Ryth', 'Vesper', 'Vend', 'Cassi', 'Issac', 'Stell', 'Prism', 'Marc', 'Willow', 'Ren', 'Varg', 'Tavi', 'HoR']


def cheb(a, b, c, e):
    return max(abs(a - c), abs(b - e))


tot = {'us': collections.Counter(), 'th': collections.Counter()}
games = 0
eps = []
for nm, m in sorted(meta.items()):
    if which == 'win' and not m['win'] or which == 'loss' and m['win']:
        continue
    f = os.path.join(d, nm + '.jsonl')
    if not os.path.exists(f):
        continue
    rows = [json.loads(l) for l in open(f) if l.strip()]
    rows = [r for r in rows if r['ch']]
    if not rows:
        continue
    games += 1
    mine = 0 if m['side'] == 'a' else 1
    spawn = {c[0]: (c[3], c[4]) for c in rows[0]['ch']}
    prev = None
    cur = None
    for r in rows:
        if r['r'] > R1:
            break
        if prev and r['r'] > R0:
            pc = {(c[1], c[2]): c[3] for c in prev['camps']}
            now = {(c[1], c[2]): c[3] for c in r['camps']}
            hits = [k for k, v in pc.items() if now.get(k, 0) < v]
            pp = {c[0]: c for c in prev['ch']}
            hurt = {c[0] for c in r['ch'] if c[0] in pp and c[5] < pp[c[0]][5]}
            died = [c for c in r['ch'] if c[0] in pp and pp[c[0]][7] and not c[7]]
            live = [c for c in r['ch'] if c[7]]
            for c in r['ch']:
                t = tot['us' if c[1] == mine else 'th']
                if not c[7]:
                    t['dead'] += 1
                    continue
                x, y = c[3], c[4]
                foes = [e for e in live if e[1] != c[1]]
                if any(cheb(x, y, h[0], h[1]) <= 4 for h in hits):
                    t['camp'] += 1
                elif any(cheb(x, y, e[3], e[4]) <= 4 and (c[0] in hurt or e[0] in hurt) for e in foes):
                    t['fight'] += 1
                elif any(cheb(x, y, e[3], e[4]) <= 8 for e in foes):
                    t['contact'] += 1
                elif cheb(x, y, *spawn[c[0]]) <= 2:
                    t['spawn'] += 1
                elif c[0] in pp and (pp[c[0]][3], pp[c[0]][4]) != (x, y):
                    t['move'] += 1
                else:
                    t['still'] += 1
            # contact episodes
            ours = [c for c in live if c[1] == mine]
            theirs = [c for c in live if c[1] != mine]
            close = [(a, b) for a in ours for b in theirs if cheb(a[3], a[4], b[3], b[4]) <= 6]
            if close or (died and cur and r['r'] - cur['last'] <= 5):
                if not cur or r['r'] - cur['last'] > 5:
                    if cur:
                        eps.append(cur)
                    cur = dict(game=nm, win=m['win'], start=r['r'], last=r['r'], us=0, th=0, k=0, dth=0, lost=0.0, dealt=0.0,
                               first=None, vict=[])
                cur['last'] = r['r']
                if close:
                    ina = {a[0] for a, b in close}
                    inb = {b[0] for a, b in close}
                    na = sum(1 for c in ours if c[0] in ina or any(cheb(c[3], c[4], b[3], b[4]) <= 8 for _, b in close))
                    nb = sum(1 for c in theirs if c[0] in inb or any(cheb(c[3], c[4], a[3], a[4]) <= 8 for a, _ in close))
                    if cur['first'] is None:
                        cur['first'] = (na, nb)
                    cur['us'] = max(cur['us'], na)
                    cur['th'] = max(cur['th'], nb)
                    for c in live:
                        if c[0] in hurt and (c[0] in ina or c[0] in inb):
                            loss = pp[c[0]][5] - c[5]
                            if c[1] == mine:
                                cur['lost'] += loss
                            else:
                                cur['dealt'] += loss
                for c in died:
                    if c[1] == mine:
                        cur['dth'] += 1
                        cur['vict'].append(N[c[2]])
                    else:
                        cur['k'] += 1
        prev = r
    if cur:
        eps.append(cur)

print('%d games (%s), rounds %d-%d' % (games, which, R0 + 1, R1))
for tag in ('us', 'th'):
    n = sum(tot[tag].values())
    print('  %s: ' % tag + '  '.join('%s %.1f%%' % (k, 100 * tot[tag][k] / max(1, n)) for k in
                                     ('camp', 'fight', 'contact', 'move', 'still', 'spawn', 'dead')) + '   turns/game %.0f' % (n / max(1, games)))
eps = [e for e in eps if R0 < e['start'] <= R1]
print('contact episodes: %.1f a game, mean %.1f rounds' % (len(eps) / max(1, games), sum(e['last'] - e['start'] + 1 for e in eps) / max(1, len(eps))))
by = collections.defaultdict(lambda: [0, 0, 0, 0, 0.0, 0.0])
for e in eps:
    a, b = e['first'] or (e['us'], e['th'])
    key = 'more' if a > b else 'equal' if a == b else 'fewer'
    v = by[key]
    v[0] += 1
    v[1] += e['k']
    v[2] += e['dth']
    v[3] += e['last'] - e['start'] + 1
    v[4] += e['dealt']
    v[5] += e['lost']
for key in ('more', 'equal', 'fewer'):
    v = by[key]
    if v[0]:
        print('  first contact with %-5s bodies: %4d episodes (%.1f a game), kills %d deaths %d, %.1f rounds, health dealt %.0f taken %.0f per episode' % (
            key, v[0], v[0] / games, v[1], v[2], v[3] / v[0], v[4] / v[0], v[5] / v[0]))
sz = collections.Counter()
for e in eps:
    a, b = e['first'] or (e['us'], e['th'])
    sz[(min(a, 5), min(b, 5))] += 1
print('  first-contact bodies ours x theirs (rows ours 1..5, cols theirs 1..5):')
for a in range(1, 6):
    print('     %d: ' % a + ' '.join('%4d' % sz[(a, b)] for b in range(1, 6)))
nok = [e for e in eps if e['k'] == 0 and e['dth'] == 0]
print('  episodes with no kill either way: %d (%.0f%%), %.1f rounds each, %.1f bodies of ours' % (
    len(nok), 100 * len(nok) / max(1, len(eps)), sum(e['last'] - e['start'] + 1 for e in nok) / max(1, len(nok)),
    sum(e['us'] for e in nok) / max(1, len(nok))))
vict = collections.Counter(v for e in eps for v in e['vict'])
print('  our deaths in episodes by champion:', vict.most_common())
