"""Turn budget from debug logs. usage: python tools/hc/budget.py DIR [map-substring] [maxround]
Per map (both teams pooled): share of living-champion turns per branch family, plus pace from the stats JSON."""
import sys,os,re,glob,zipfile,json,collections
d=sys.argv[1]; sub=sys.argv[2] if len(sys.argv)>2 else ''; maxr=int(sys.argv[3]) if len(sys.argv)>3 else 400
pat=re.compile(r'^r(\d+) c(\d+) \((\d+),(\d+)\)(?: IN-CHASE-RING)? ([\w.\-]+)',re.M)
def fam(b):
    if b.startswith('farm.off') or b.startswith('farm.wait') or b in('farm.hold','idle'): return 'wait'
    if b.startswith('farm.camp') or b.startswith('farm.hit') or b.startswith('farm.with'): return 'camp'
    if b.startswith('farm.walk') or b.startswith('farm.moveon') or b.startswith('farm.'): return 'walk'
    if b.startswith('collect') or b.startswith('farm.capture'): return 'collect'
    if b.startswith('channel.tribute') or b.startswith('bank') or b.startswith('tribute') or b.startswith('win'): return 'bank'
    if b.startswith('channel.recall') or b.startswith('shop') or b.startswith('spawn') or 'recall' in b: return 'recall'
    if b.startswith('fight') or b.startswith('ghost') or b.startswith('poke'): return 'fight'
    if b.startswith('retreat') or b.startswith('hurt') or b.startswith('outclass'): return 'flee'
    if b.startswith('dead'): return 'dead'
    return 'other'
F=['camp','walk','wait','collect','bank','recall','fight','flee','dead','other']
tot=collections.Counter(); oth=collections.Counter()
print('%-22s R    g200 g400 gEnd  dth | '%'map'+' '.join('%5s'%f[:5] for f in F))
for z in sorted(glob.glob(os.path.join(d,'*.repl26a'))):
    name=os.path.basename(z)[:-8]
    if sub not in name: continue
    c=collections.Counter()
    try:
        zf=zipfile.ZipFile(z)
        for t in 'ab':
            s=zf.read('team-%s.stdout'%t).replace(b'\0',b'').decode('utf-8','replace')
            for m in pat.finditer(s):
                if int(m.group(1))<=maxr:
                    f=fam(m.group(5)); c[f]+=1
                    if f=='other': oth[m.group(5)]+=1
    except Exception as e: print(name,'ERR',e); continue
    j=json.load(open(z[:-8]+'.json',encoding='utf-8'))
    def at(runs,r):
        t=0;v=0
        for x in runs:
            if t>r: break
            v=int(float(x['value'])); t+=x['duration']
        return v
    tl=[t['runs'] for t in j['teamTimelines'] if t['metric']=='submittedGems']
    dth=sum(int(ch['metrics']['deaths']) for ch in j['champions'])
    n=sum(c.values()) or 1
    print('%-22s %4d %4d %4d %4d  %3d | '%(name,j['match']['roundsCompleted'],sum(at(r,200) for r in tl)//2,sum(at(r,400) for r in tl)//2,sum(at(r,9999) for r in tl)//2,dth)+' '.join('%5.1f'%(100*c[f]/n) for f in F))
    tot.update(c)
n=sum(tot.values()) or 1
print('%-22s                           | '%'ALL'+' '.join('%5.1f'%(100*tot[f]/n) for f in F))
print('other:',oth.most_common(25))
