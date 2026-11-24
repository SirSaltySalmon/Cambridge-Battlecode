"""Share of solo-job turns spent on a group another champion also holds as its solo job that round.
usage: python tools/hc/dups.py DIR [mapsub]"""
import sys,re,glob,os,zipfile,collections
d=sys.argv[1]; sub=sys.argv[2] if len(sys.argv)>2 else ''
pat=re.compile(r'^r(\d+) c(\d+) \((\d+),(\d+)\)(?: IN-CHASE-RING)? (job|crew)\.([\w.\-]+): s(\d+)',re.M)
tot=dup=0; kinds=collections.Counter(); per=[]
for z in sorted(glob.glob(os.path.join(d,'*.repl26a'))):
    if sub not in z: continue
    s=zipfile.ZipFile(z).read('team-a.stdout').replace(b'\0',b'').decode('utf-8','replace')
    by=collections.defaultdict(list)
    for m in pat.finditer(s):
        if m.group(5)=='job': by[int(m.group(1))].append((m.group(7),m.group(6),m.group(2)))
    t=du=0
    for r,l in by.items():
        c=collections.Counter(x[0] for x in l)
        for slot,br,ch in l:
            t+=1
            if c[slot]>1: du+=1; kinds[br.split('.')[0]]+=1
    tot+=t; dup+=du; per.append((du/max(1,t),os.path.basename(z)[:-8],t))
print('solo-job turns %d, on a group another solo also holds: %d (%.1f%%)'%(tot,dup,100*dup/max(1,tot)), kinds.most_common(6))
per.sort(reverse=True); print(' '.join('%s %.0f%%'%(n,100*p) for p,n,t in per[:14] if t>50))
