"""Team pace from a decoded replay (tools/replay/replay.mjs output). usage: python tools/replay/pace.py X.jsonl [maxround]
Per team: share of champion-rounds dead / at spawn / engaged on a camp / fighting / moving / still; camp damage per
100 rounds; how many camp groups are being hit at once; gold and max health per champion over time."""
import sys,json,collections
N=['Ryth','Vesper','Vend','Cassi','Issac','Stell','Prism','Marc','Willow','Ren','Varg','Tavi','HoR']
rows=[json.loads(l) for l in open(sys.argv[1]) if l.strip()]
rows=[r for r in rows if r['ch']]
maxr=int(sys.argv[2]) if len(sys.argv)>2 else 10**9
spawn={c[0]:(c[3],c[4]) for c in rows[0]['ch']}
team={c[0]:c[1] for c in rows[0]['ch']}; typ={c[0]:c[2] for c in rows[0]['ch']}
cat=[collections.Counter(),collections.Counter()]
dmg=[collections.Counter(),collections.Counter()]
split=[collections.Counter(),collections.Counter()]
snap={}
prev=None
def cheb(a,b,c,d): return max(abs(a-c),abs(b-d))
for r in rows:
    if r['r']>maxr: break
    if prev:
        pc={(c[1],c[2]):c[3] for c in prev['camps']}
        hit=[(c[1],c[2],pc[(c[1],c[2])]-c[3]) for c in r['camps'] if (c[1],c[2]) in pc and pc[(c[1],c[2])]>c[3]]
        # camps that died this round
        now={(c[1],c[2]) for c in r['camps']}
        hit+=[(k[0],k[1],v) for k,v in pc.items() if k not in now and v>0]
        pp={c[0]:c for c in prev['ch']}
        hurt={c[0] for c in r['ch'] if c[0] in pp and c[5]<pp[c[0]][5]}
        clusters=[set(),set()]
        for c in r['ch']:
            i,t=c[0],c[1]
            if not c[7]: cat[t]['dead']+=1; continue
            x,y=c[3],c[4]
            near=[h for h in hit if cheb(x,y,h[0],h[1])<=4]
            enemy=[e for e in r['ch'] if e[1]!=t and e[7] and cheb(x,y,e[3],e[4])<=4]
            if near:
                cat[t]['camp']+=1
                for h in near: clusters[t].add((h[0]//6,h[1]//6))
            elif enemy and (i in hurt or any(e[0] in hurt for e in enemy)): cat[t]['fight']+=1
            elif cheb(x,y,*spawn[i])<=2: cat[t]['spawn']+=1
            elif i in pp and (pp[i][3],pp[i][4])!=(x,y): cat[t]['move']+=1
            else: cat[t]['still']+=1
        for h in hit:
            # credit to the team with the nearest living champion
            best=min((cheb(c[3],c[4],h[0],h[1]),c[1]) for c in r['ch'] if c[7])
            if best[0]<=5: dmg[best[1]][r['r']//100]+=h[2]
        for t in (0,1): split[t][len(clusters[t])]+=1
    if r['r'] in (100,200,300,400,600): snap[r['r']]=r['ch']
    prev=r
F=['camp','move','still','spawn','fight','dead']
for t in (0,1):
    n=sum(cat[t].values()) or 1
    print('team',t,' '.join(N[typ[i]] for i in sorted(team) if team[i]==t))
    print('   turns: '+'  '.join('%s %.1f%%'%(f,100*cat[t][f]/n) for f in F))
    print('   camp dmg per 100r: '+' '.join('%d'%dmg[t][k] for k in range(0,max(dmg[t] or [0])+1)))
    s=sum(split[t].values()) or 1
    print('   camp sites hit at once: '+' '.join('%d:%.0f%%'%(k,100*v/s) for k,v in sorted(split[t].items())))
    for rr in sorted(snap):
        print('   r%d '%rr+' | '.join('%s g%d hp%d gem%d'%(N[c[2]][:4],c[8],c[6],c[9]) for c in snap[rr] if c[1]==t))
