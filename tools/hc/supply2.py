"""Supply use by group composition (own half, team 0). usage: python tools/hc/supply2.py DECDIR [r0=60] [r1=600]
Camp kinds by engine type. Group classes: pure gem-cluster groups by size, gold-only, wolf mixes, elk mixes, golem, mono.
Prints groups, share of rounds the group stands alive, captures per group, and gems per round they give vs their ceiling."""
import sys,os,glob,json,collections
d=sys.argv[1]; r0=int(sys.argv[2]) if len(sys.argv)>2 else 60; r1=int(sys.argv[3]) if len(sys.argv)>3 else 600
T={0:'w',1:'e',2:'c',3:'g',4:'M',5:'G'}   # set from the SDK enum below
T={0:'w',1:'e',2:'c',3:'g',4:'M',5:'G'}   # bc::camp_type order
GEM={'w':4,'e':15,'G':36,'M':200,'c':7,'g':0}; RESP={'w':40,'e':60,'G':80,'M':150,'c':50,'g':50}
def cls(k):
    s=set(k)
    if s<={'c'}: return 'gems x%d'%len(k) if len(k)<4 else 'gems x4+'
    if s<={'g'}: return 'gold only'
    if s<={'c','g'}: return 'gems+gold'
    if 'M' in s: return 'monolith'
    if 'G' in s: return 'golem..'
    if 'e' in s: return 'elk x%d..'%k.count('e') if k.count('e')<3 else 'elk x3+..'
    return 'wolf x%d..'%k.count('w') if k.count('w')<3 else 'wolf x3+..'
tot=collections.defaultdict(lambda:[0,0,0,0,0.0,0.0]); maps=0
for f in glob.glob(os.path.join(d,'*.jsonl')):
    rows=[json.loads(l) for l in open(f) if l.strip()]
    rows=[r for r in rows if r['ch']]
    if not rows: continue
    maps+=1
    sp=[[],[]]
    for c in rows[0]['ch']: sp[c[1]].append((c[3],c[4]))
    cen=[(sum(x for x,_ in s)/len(s),sum(y for _,y in s)/len(s)) for s in sp]
    camps={(c[1],c[2]):T.get(c[0],'?') for c in rows[0]['camps']}
    seen=set(); groups=[]
    for p in camps:
        if p in seen: continue
        st=[p]; seen.add(p); g=[]
        while st:
            q=st.pop(); g.append(q)
            for dd in ((1,0),(-1,0),(0,1),(0,-1)):
                n=(q[0]+dd[0],q[1]+dd[1])
                if n in camps and n not in seen: seen.add(n); st.append(n)
        cx=sum(x for x,_ in g)/len(g); cy=sum(y for _,y in g)/len(g)
        da=(cx-cen[0][0])**2+(cy-cen[0][1])**2; db=(cx-cen[1][0])**2+(cy-cen[1][1])**2
        if da<db*0.8: groups.append(g)
    last=min(r1,rows[-1]['r']); n=0
    alive=[0]*len(groups); caps=[0]*len(groups); was=[True]*len(groups)
    for r in rows:
        if r['r']<r0 or r['r']>r1: continue
        now={(c[1],c[2]) for c in r['camps']}; n+=1
        for i,g in enumerate(groups):
            a=any(p in now for p in g)
            if a: alive[i]+=1
            if was[i] and not a: caps[i]+=1
            was[i]=a
    if not n: continue
    for i,g in enumerate(groups):
        k=''.join(sorted(camps[p] for p in g)); c=cls(k); t=tot[c]
        gems=sum(GEM[x] for x in k); resp=max(RESP[x] for x in k)
        t[0]+=1; t[1]+=alive[i]; t[2]+=n; t[3]+=caps[i]; t[4]+=gems*caps[i]/n; t[5]+=gems/(resp+10.0)
print('%d maps, rounds %d-%d, own half'%(maps,r0,r1))
print('%-12s groups/map alive%%  captures  gems/round/map  ceiling'%'class')
sg=sc=0
for k,t in sorted(tot.items(),key=lambda kv:-kv[1][5]):
    print('%-12s %6.2f    %5.1f   %5.2f     %6.3f        %6.3f'%(k,t[0]/maps,100*t[1]/max(1,t[2]),t[3]/max(1,t[0]),t[4]/maps,t[5]/maps)); sg+=t[4]; sc+=t[5]
print('%-12s                               %6.3f        %6.3f'%('ALL',sg/maps,sc/maps))
