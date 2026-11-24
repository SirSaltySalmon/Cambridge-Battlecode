"""Camp deaths of team A in a solo/bench dir: per champion, the branches of its last turns before dying.
usage: python tools/hc/campdeaths.py DIR [names,comma] [show=N examples]"""
import sys,json,glob,os,re,zipfile,collections
d=sys.argv[1]; names=sys.argv[2].split(',') if len(sys.argv)>2 and sys.argv[2]!='-' else None; show=int(sys.argv[3]) if len(sys.argv)>3 else 0
KID={'Ryth':0,'Vesper Vale':1,'Vendetta':2,'Cassi':3,'Issac':4,'Stellar':5,'Prism':6,'Marcella':7,'Willow':8,'Ren':9,'Varg':10,'Tavi':11,'Heart of Ruin':12}
pat=re.compile(r'^r(\d+) c(\d+) \((\d+),(\d+)\)(?: IN-CHASE-RING)? ([\w.\-]+): ([^\n]*)',re.M)
last=collections.Counter(); kinds=collections.Counter(); ex=[]
for f in sorted(glob.glob(d+'/*.json')):
    j=json.load(open(f,encoding='utf-8'))
    deaths=[]
    for e in j['events']:
        if e['kind']=='death' and e['owner'].get('team')=='teamA':
            for x in e['entries']:
                if names and e['owner']['name'] not in names: continue
                deaths.append((x['timestamp']['round'],e['owner']['name'],x.get('context'),x['x'],x['y']))
    if not deaths: continue
    s=zipfile.ZipFile(f[:-5]+'.repl26a').read('team-a.stdout').replace(b'\0',b'').decode('utf-8','replace')
    turns=collections.defaultdict(list)
    for m in pat.finditer(s): turns[int(m.group(2))].append((int(m.group(1)),m.group(5),m.group(6)[:150]))
    for r,nm,ctx,x,y in deaths:
        kinds[(nm,ctx)]+=1
        t=[u for u in turns[KID[nm]] if r-6<=u[0]<=r]
        if t:
            last[(nm,ctx,t[-1][1])]+=1
            if len(ex)<show: ex.append((os.path.basename(f)[:-5],nm,r,ctx,t[-5:]))
print(kinds.most_common())
for k,v in last.most_common(25): print(v,k)
for mp,nm,r,ctx,t in ex:
    print('--',mp,nm,'dies r%d'%r,ctx)
    for u in t: print('     r%d %s: %s'%u)
