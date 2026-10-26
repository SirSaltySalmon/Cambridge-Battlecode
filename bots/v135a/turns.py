# turns.py LOG lo hi [champs]  -> each champion's turn lines + sub lines in [lo,hi]
import re, sys
log, lo, hi = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
cs = set(sys.argv[4].split(',')) if len(sys.argv) > 4 else None
head = re.compile(r'^\s*r(\d+) (c\d+) ')
keep = False
for line in open(log, encoding='utf-8', errors='replace'):
    line = line.replace(chr(0), ' ')
    m = head.match(line)
    if m:
        r = int(m.group(1))
        keep = lo <= r <= hi and (cs is None or m.group(2) in cs)
        if keep: print(line.strip())
    elif keep and line.startswith('  ') and 'setup:' not in line:
        print('      ' + line.strip())
