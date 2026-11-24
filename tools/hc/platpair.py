"""Paired comparison of two platform test runs (tables from plattab.py), cell by cell, by class.
usage: python tools/hc/platpair.py NEW BASE   -> means of banked gems, collected, K/D, gold, camp damage; margin changes"""
import json, sys
sys.path.insert(0, 'tools/hc')
import plat2
new, base = sys.argv[1], sys.argv[2]
A = {r['k']: r for r in json.load(open('results/plat/%s-table.json' % new))}
B = {r['k']: r for r in json.load(open('results/plat/%s-table.json' % base))}
def mean(l): return sum(l) / max(1, len(l))
for cls in ('near', 'top'):
    ks = [k for k in A if k in B and (k.split('|')[0] in plat2.TOP_NAMES) == (cls == 'top')]
    print('%s: %d paired cells; wins %s %d, %s %d' % (cls, len(ks), new, sum(A[k]['win'] for k in ks), base, sum(B[k]['win'] for k in ks)))
    for name, f in (('banked at end, us', lambda r: r['ge'][0]), ('banked at end, them', lambda r: r['ge'][1]), ('banked r600, us', lambda r: r['g6'][0]),
                    ('banked r600, them', lambda r: r['g6'][1]), ('collected, us', lambda r: r['col'][0]), ('kills', lambda r: r['kd'][0]),
                    ('deaths', lambda r: r['kd'][1]), ('gold, us', lambda r: r['gold'][0]), ('gold, them', lambda r: r['gold'][1]),
                    ('camp damage, us', lambda r: r['md'][0]), ('camp damage, them', lambda r: r['md'][1]), ('rounds', lambda r: r['R'])):
        print('   %-22s %8.1f  vs %8.1f' % (name, mean([f(A[k]) for k in ks]), mean([f(B[k]) for k in ks])))
    marg = sorted(((A[k]['ge'][0] - A[k]['ge'][1]) - (B[k]['ge'][0] - B[k]['ge'][1]), k) for k in ks)
    same = sum(1 for d, k in marg if d == 0 and A[k]['R'] == B[k]['R'])
    print('   identical games %d; margin change: mean %+.0f, median %+.0f; better in %d, worse in %d' % (
        same, mean([d for d, _ in marg]), marg[len(marg) // 2][0], sum(d > 0 for d, _ in marg), sum(d < 0 for d, _ in marg)))
    print('   worst: ' + ', '.join('%s %+d' % (k.split('|')[1] + '@' + k.split('|')[0][:8], d) for d, k in marg[:4]))
    print('   best:  ' + ', '.join('%s %+d' % (k.split('|')[1] + '@' + k.split('|')[0][:8], d) for d, k in marg[-4:]))
