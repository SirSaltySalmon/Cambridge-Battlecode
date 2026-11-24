"""Share of living turns per branch (first two name parts), whole dir, team A. usage: python tools/hc/stems.py DIR [maxround] [mapsub]"""
import sys,re,glob,os,zipfile,collections
d=sys.argv[1]; mr=int(sys.argv[2]) if len(sys.argv)>2 else 10**9; sub=sys.argv[3] if len(sys.argv)>3 else ''
pat=re.compile(r'^r(\d+) c(\d+) \((\d+),(\d+)\)(?: IN-CHASE-RING)? ([\w.\-]+)',re.M)
c=collections.Counter()
for z in glob.glob(os.path.join(d,'*.repl26a')):
    if sub not in z: continue
    s=zipfile.ZipFile(z).read('team-a.stdout').replace(b'\0',b'').decode('utf-8','replace')
    for m in pat.finditer(s):
        if int(m.group(1))<=mr: c['.'.join(m.group(5).split('.')[:2])]+=1
n=sum(c.values())
print(n,'turns'); print('  '.join('%s %.1f%%'%(k,100*v/n) for k,v in c.most_common(34)))
