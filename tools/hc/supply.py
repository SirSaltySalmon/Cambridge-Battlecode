"""Camp supply use from decoded replays: how long camps on a team's own half stand alive (unfarmed).
usage: python tools/hc/supply.py DIR TEAM(0|1|meta) [r0=60] [r1=600]
TEAM meta: DIR/meta.json maps name -> {side: a|b}. Prints per camp kind: count, share of rounds alive, clears per camp."""
import sys,os,glob,json,collections
d=sys.argv[1]; tm=sys.argv[2]; r0=int(sys.argv[3]) if len(sys.argv)>3 else 60; r1=int(sys.argv[4]) if len(sys.argv)>4 else 600
meta=json.load(open(os.path.join(d,'meta.json'))) if tm=='meta' else None
def kind(h): return {1350:'wolf',2400:'elk',5000:'golem',20000:'mono',120:'cluster'}.get(h,'?')
tot=collections.defaultdict(lambda:[0,0,0,0])  # camps, alive rounds, rounds, clears
permap={}
for f in glob.glob(os.path.join(d,'*.jsonl')):
    name=os.path.basename(f)[:-6]
    team=({'a':0,'b':1}[meta[name]['side']] if meta else int(tm)) if (not meta or name in meta) else None
    if team is None: continue
    rows=[json.loads(l) for l in open(f) if l.strip()]
    rows=[r for r in rows if r['ch']]
    if not rows: continue
    sp=[[],[]]
    for c in rows[0]['ch']: sp[c[1]].append((c[3],c[4]))
    cen=[(sum(x for x,_ in s)/len(s),sum(y for _,y in s)/len(s)) for s in sp]
    hp0={(c[1],c[2]):c[3] for c in rows[0]['camps']}
    def mine(p):
        da=(p[0]-cen[team][0])**2+(p[1]-cen[team][1])**2; db=(p[0]-cen[1-team][0])**2+(p[1]-cen[1-team][1])**2
        return da<db*0.8
    own=[p for p in hp0 if mine(p)]
    alive=collections.Counter(); clears=collections.Counter(); prev=set(hp0); n=0
    for r in rows:
        if r['r']<r0 or r['r']>r1: prev={(c[1],c[2]) for c in r['camps']}; continue
        now={(c[1],c[2]) for c in r['camps']}; n+=1
        for p in own:
            if p in now: alive[p]+=1
            elif p in prev: clears[p]+=1
        prev=now
    if not n: continue
    pm=[0,0]
    for p in own:
        k=kind(hp0[p]); t=tot[k]; t[0]+=1; t[1]+=alive[p]; t[2]+=n; t[3]+=clears[p]
        pm[0]+=alive[p]; pm[1]+=n
    permap[name]=pm[0]/max(1,pm[1])
print('kind      camps  alive%%  clears/camp   (rounds %d-%d, own half)'%(r0,r1))
for k,t in sorted(tot.items()): print('%-8s %6d  %5.1f  %6.2f'%(k,t[0],100*t[1]/max(1,t[2]),t[3]/max(1,t[0])))
if '--maps' in sys.argv:
    for k,v in sorted(permap.items(),key=lambda kv:-kv[1]): print('  %-22s %.0f%%'%(k,100*v))
