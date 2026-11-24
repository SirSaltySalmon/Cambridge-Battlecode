"""Print decision-log lines from a replay bundle. usage: python tools/hc/logcat.py FILE.repl26a TEAM(a|b) R0 R1 [champ-id ...] [--notes]"""
import sys,re,zipfile
z,t,r0,r1=sys.argv[1],sys.argv[2],int(sys.argv[3]),int(sys.argv[4])
rest=[a for a in sys.argv[5:] if not a.startswith('--')]; notes='--notes' in sys.argv
s=zipfile.ZipFile(z).read('team-%s.stdout'%t).replace(b'\0',b'').decode('utf-8','replace')
keep=False
for l in s.split('\n'):
    m=re.match(r'r(\d+) c(\d+) ',l)
    if m: keep = r0<=int(m.group(1))<=r1 and (not rest or m.group(2) in rest)
    elif not l.startswith('  '): keep=False
    if keep and (m or notes): print(l[:400])
