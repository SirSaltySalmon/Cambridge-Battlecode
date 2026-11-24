"""python flips.py DIR [MAP...]: contract transitions per game (ours), flip reasons, and gaps between flips."""
import collections, glob, os, re, sys, zipfile
LINE = re.compile(r'^\s*r(\d+) c(\d+) \((\d+),(\d+)\) (\S+): (.*?)(?: \| (.*))?$')
d = sys.argv[1]
maps = sys.argv[2:]
tr = collections.Counter(); gaps = collections.Counter(); games = 0
why = collections.Counter()
for repl in sorted(glob.glob(os.path.join(d, '*.repl26a'))):
    base = os.path.basename(repl)[:-8]
    if maps and base not in maps: continue
    side = 'a' if repl.endswith('-a.repl26a') else 'b'
    text = zipfile.ZipFile(repl).read(f'team-{side}.stdout').decode('utf-8', 'replace').replace(chr(0), ' ')
    games += 1
    rnd = 0; last = None
    for line in text.splitlines():
        m = LINE.match(line)
        if m: rnd = int(m.group(1)); continue
        s = line.strip()
        mm = re.match(r'- contract: (\w+) -> (\w+)', s)
        if mm:
            a, b = mm.group(1), mm.group(2)
            tr[f'{a}->{b}'] += 1
            if {a, b} == {'engage', 'disengage'}:
                if last is not None: gaps[min(rnd - last, 10)] += 1
                last = rnd
            if 'out-emblemed' in s: why['outclass'] += 1
            if 'chase dropped' in s: why['chase'] += 1
            if 'veto' in s: why['veto'] += 1
            if 'held' in s: why['held'] += 1
print(d, games, 'games')
for k, v in tr.most_common(): print(f'  {k}: {v} ({v / max(games, 1):.1f}/game)')
print('  e<->d flips per game:', sum(v for k, v in tr.items() if k in ('engage->disengage', 'disengage->engage')) / max(games, 1))
print('  reasons', dict(why))
print('  rounds between e<->d flips', sorted(gaps.items()))
