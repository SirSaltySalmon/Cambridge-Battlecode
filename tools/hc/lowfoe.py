"""What we do with a low enemy beside us, from a local debug bench (logs + decoded replays; run disagree.py first to
decode). A turn counts when an enemy champion under LOW of max health stands within GAP tiles of one of our living
champions that is over 40% health itself. Per branch: turns, and the commonest reasons.
usage: python tools/hc/lowfoe.py DIR [bot|old|both] [LOW=0.4] [GAP=3] [-v]"""
import collections, json, os, re, sys, zipfile, glob
a = [x for x in sys.argv[1:] if x != '-v']; verbose = '-v' in sys.argv
d = a[0]; rule = a[1] if len(a) > 1 else 'bot'; LOW = float(a[2]) if len(a) > 2 else 0.4; GAP = int(a[3]) if len(a) > 3 else 3
N = ['Ryth', 'Vesper', 'Vend', 'Cassi', 'Issac', 'Stell', 'Prism', 'Marc', 'Willow', 'Ren', 'Varg', 'Tavi', 'HoR']
pat = re.compile(r'^r(\d+) c(\d+) \((\d+),(\d+)\)(?: IN-CHASE-RING)? ([\w.\-]+)(?:: ([^|\n]*))?(?:\| ([^\n]*))?', re.M)
def cheb(a, b, c, e): return max(abs(a - c), abs(b - e))
tot = collections.Counter(); why = collections.defaultdict(collections.Counter); acts = collections.defaultdict(collections.Counter); n = 0; ex = []; kinds = collections.Counter()
for z in sorted(glob.glob(os.path.join(d, '*.repl26a'))):
    side_a = z.endswith('-a.repl26a')
    teams = [0 if side_a else 1] if rule == 'bot' else [1 if side_a else 0] if rule == 'old' else [0, 1]
    o = z[:-8] + '.jsonl'
    rows = [json.loads(l) for l in open(o) if l.strip()]; rows = [r for r in rows if r['ch']]
    zf = zipfile.ZipFile(z)
    for mine in teams:
        s = zf.read('team-%s.stdout' % 'ab'[mine]).replace(b'\0', b'').decode('utf-8', 'replace')
        br = {(int(m.group(1)), int(m.group(2))): (m.group(5), (m.group(6) or '').strip(), (m.group(7) or '').strip()) for m in pat.finditer(s)}
        for r in rows:
            live = [c for c in r['ch'] if c[7]]
            lows = [e for e in live if e[1] != mine and e[5] < LOW * e[6]]
            if not lows: continue
            for c in live:
                if c[1] != mine or c[5] < 0.4 * c[6]: continue
                near = [e for e in lows if cheb(c[3], c[4], e[3], e[4]) <= GAP]
                if not near: continue
                b, w, act = br.get((r['r'], c[2]), ('?', '', ''))
                stem = '.'.join(b.split('.')[:2]); n += 1; tot[stem] += 1; kinds[(N[c[2]], 'attack' in act or 'cast' in act)] += 1
                why[stem][re.sub(r'\d+', '#', w)[:110]] += 1; acts[stem][re.sub(r'\d+', '#', act)[:30]] += 1
                if verbose and not stem.startswith('fight') and len(ex) < 40: ex.append('%s r%d %s (%d,%d) foe %s %d%% at (%d,%d): %s: %s | %s' % (os.path.basename(z)[:-8], r['r'], N[c[2]], c[3], c[4], N[near[0][2]], 100 * near[0][5] / near[0][6], near[0][3], near[0][4], b, w[:120], act[:30]))
print('%d turns with an enemy under %.0f%% within %d tiles of a healthy champion of ours' % (n, LOW * 100, GAP))
for k, v in tot.most_common(18):
    print('  %5d %4.1f%%  %-18s acts: %s  ||  %s' % (v, 100 * v / n, k, ', '.join('%s %d' % kv for kv in acts[k].most_common(3)), ' || '.join('%s (%d)' % kv for kv in why[k].most_common(2))))
print('by champion (attacked or cast / total):', ' '.join('%s %d/%d' % (k, kinds[(k, True)], kinds[(k, True)] + kinds[(k, False)]) for k in N if kinds[(k, True)] + kinds[(k, False)]))
for e in ex: print('   ', e)
