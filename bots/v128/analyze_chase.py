import re, sys, zipfile, glob, os
from collections import defaultdict
LINE = re.compile(r'^\s*r(\d+) c(\d+) (?:\((\d+),(\d+)\) )?(\S+): (.*?)(?: \| (.*))?$')
def analyse(path, team):
    z = zipfile.ZipFile(path)
    text = z.read(f'team-{team}.stdout').decode('utf-8', 'replace').replace(chr(0), ' ')
    turns = defaultdict(dict)
    for l in text.splitlines():
        m = LINE.match(l)
        if m: turns[int(m.group(2))][int(m.group(1))] = (m.group(5), m.group(6))
    total = chase = 0; streaks = []
    for c, rs in turns.items():
        run = 0
        for r in sorted(rs):
            branch, why = rs[r]
            if branch == 'dead.shop': continue
            total += 1
            closing = branch.startswith('fight.hit') and 'closing' in why
            if closing: chase += 1; run += 1
            else:
                if run: streaks.append((run, branch.startswith('fight.hit') and 'in reach' in why))
                run = 0
    return total, chase, streaks
T = C = 0; S_all = []
for f in sorted(glob.glob(sys.argv[1] + '/*.repl26a')):
    t, c, s = analyse(f, sys.argv[2] if len(sys.argv) > 2 else 'a')
    T += t; C += c; S_all += s
    long_ = [x for x in s if x[0] >= 6]
    print('%-26s turns %5d closing %4d (%.1f%%) chases %3d, >=6 rounds %3d (caught %d)' % (os.path.basename(f), t, c, 100*c/t, len(s), len(long_), sum(x[1] for x in long_)))
by = defaultdict(lambda: [0, 0, 0])
for n, caught in S_all:
    k = '1-2' if n <= 2 else '3-5' if n <= 5 else '6-10' if n <= 10 else '11+'
    by[k][0] += 1; by[k][1] += caught; by[k][2] += n
print('all: closing %d of %d turns (%.1f%%)' % (C, T, 100*C/T))
for k in ['1-2', '3-5', '6-10', '11+']: print('  chases of %-5s rounds: %4d, caught %4d (%.0f%%), turns %5d' % (k, by[k][0], by[k][1], 100*by[k][1]/max(1, by[k][0]), by[k][2]))
