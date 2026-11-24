"""Two-tile shuffles in decision logs: runs of 6+ turns in which a champion's tile equals its tile two turns earlier
and differs from the last one. Per branch pair: turns, and how many had an enemy champion within 12 tiles.
usage: python tools/hc/shuffle.py DIR [bot|old|both] [-v]"""
import collections, json, os, re, sys, zipfile, glob
a = [x for x in sys.argv[1:] if x != '-v']; verbose = '-v' in sys.argv
d = a[0]; rule = a[1] if len(a) > 1 else 'bot'
pat = re.compile(r'^r(\d+) c(\d+) \((\d+),(\d+)\)(?: IN-CHASE-RING)? ([\w.\-]+)(?:: ([^|\n]*))?', re.M)
def cheb(a, b, c, e): return max(abs(a - c), abs(b - e))
tot = collections.Counter(); near = collections.Counter(); why = collections.defaultdict(collections.Counter); turns = 0; ex = []
for z in sorted(glob.glob(os.path.join(d, '*.repl26a'))):
    side_a = z.endswith('-a.repl26a')
    teams = [0 if side_a else 1] if rule == 'bot' else [1 if side_a else 0] if rule == 'old' else [0, 1]
    rows = {}
    o = z[:-8] + '.jsonl'
    if os.path.exists(o):
        for l in open(o):
            r = json.loads(l)
            if r['ch']: rows[r['r']] = r
    zf = zipfile.ZipFile(z)
    for mine in teams:
        s = zf.read('team-%s.stdout' % 'ab'[mine]).replace(b'\0', b'').decode('utf-8', 'replace')
        per = collections.defaultdict(list)
        for m in pat.finditer(s):
            per[int(m.group(2))].append((int(m.group(1)), (int(m.group(3)), int(m.group(4))), m.group(5), (m.group(6) or '').strip()))
        for c, seq in per.items():
            turns += len(seq); i = 2
            while i < len(seq):
                j = i
                while j < len(seq) and seq[j][1] == seq[j - 2][1] and seq[j][1] != seq[j - 1][1] and seq[j][0] - seq[j - 1][0] == 1: j += 1
                if j - i >= 6:
                    for k in range(i, j):
                        stem = '.'.join(seq[k][2].split('.')[:2]); tot[stem] += 1
                        why[stem][re.sub(r'\d+', '#', seq[k][3])[:100]] += 1
                        r = rows.get(seq[k][0])
                        if r and any(e[7] and e[1] != mine and cheb(e[3], e[4], *seq[k][1]) <= 12 for e in r['ch']): near[stem] += 1
                    if verbose and len(ex) < 25: ex.append('%s r%d-%d c%d %s <-> %s  %s / %s' % (os.path.basename(z)[:-8], seq[i][0], seq[j - 1][0], c, seq[i][1], seq[i - 1][1], seq[i][2], seq[i + 1][2]))
                i = max(j, i + 1)
n = sum(tot.values())
print('%d turns, %d in two-tile shuffles of 6+ (%.1f%%), %d with an enemy within 12' % (turns, n, 100 * n / max(1, turns), sum(near.values())))
for k, v in tot.most_common(16):
    print('  %5d (enemy near %4d)  %-20s %s' % (v, near[k], k, ' || '.join('%s (%d)' % kv for kv in why[k].most_common(2))))
for e in ex: print('   ', e)
