"""Deaths to champions in a mirror dir (both teams' logs): for one champion kind, the last five branch families before
the death and the commonest reasons.

usage: python tools/hc/champdeaths.py DIR KIND_ID   (12 Heart of Ruin, 0 Ryth, 1 Vesper, 8 Willow, 7 Marcella, 6 Prism)
"""
import collections
import glob
import json
import re
import sys
import zipfile

d, kid = sys.argv[1], sys.argv[2]
NAME = {0: 'Ryth', 1: 'Vesper Vale', 2: 'Vendetta', 3: 'Cassi', 4: 'Issac', 5: 'Stellar', 6: 'Prism', 7: 'Marcella',
        8: 'Willow', 9: 'Ren', 10: 'Varg', 11: 'Tavi', 12: 'Heart of Ruin'}[int(kid)]
pat = re.compile(r'^r(\d+) c(\d+) \((\d+),(\d+)\)(?: IN-CHASE-RING)? ([\w.\-]+): ([^\n]*)\n((?:  [^\n]*\n)*)', re.M)
seq = collections.Counter()
why = collections.Counter()
n = games = 0
hp = []
for f in sorted(glob.glob(d + '/*.json')):
    j = json.load(open(f, encoding='utf-8'))
    games += 1
    for team, side in (('teamA', 'a'), ('teamB', 'b')):
        deaths = [x['timestamp']['round'] for e in j['events']
                  if e['kind'] == 'death' and e['owner'].get('team') == team and e['owner']['name'] == NAME
                  for x in e['entries'] if x.get('context') == 'champion']
        if not deaths:
            continue
        s = zipfile.ZipFile(f[:-5] + '.repl26a').read('team-%s.stdout' % side).replace(b'\0', b'').decode('utf-8', 'replace')
        turns = [(int(m.group(1)), m.group(5), m.group(6)[:110], m.group(7)) for m in pat.finditer(s) if m.group(2) == kid]
        for r in deaths:
            t = [u for u in turns if r - 9 <= u[0] <= r]
            if len(t) < 3:
                continue
            n += 1
            seq[' > '.join('.'.join(u[1].split('.')[:2]) for u in t[-5:])] += 1
            for u in t[-7:]:
                why[(u[1], re.sub(r'\d+', 'N', u[2]).split('|')[0][:100])] += 1
            m = re.search(r'@s hp (\d+)/(\d+)', t[-min(len(t), 6)][3])
            if m:
                hp.append(int(m.group(1)) / int(m.group(2)))
hp.sort()
print('%s: %d deaths to champions in %d games (%.2f per team-game); health ~6 turns before: median %.0f%%' % (
    NAME, n, games, n / max(1, games) / 2, 100 * hp[len(hp) // 2] if hp else 0))
for k, v in seq.most_common(12):
    print('  %3d %s' % (v, k))
print()
for k, v in why.most_common(18):
    print('  %4d %s: %s' % (v, k[0], k[1]))
