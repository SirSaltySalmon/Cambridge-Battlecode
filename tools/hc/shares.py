"""Turn shares (camp / fight / move / still / spawn / dead) for us and them over decoded test replays.
usage: python tools/hc/shares.py DIR [r1=300]"""
import sys,json,os,collections,glob
d=sys.argv[1]; R1=int(sys.argv[2]) if len(sys.argv)>2 else 300; R0=int(sys.argv[3]) if len(sys.argv)>3 else 0
meta=json.load(open(d+'/meta.json'))
def cheb(a,b,c,e): return max(abs(a-c),abs(b-e))
tot={'us':collections.Counter(),'th':collections.Counter()}; dm={'us':0,'th':0}; g=0
for nm,m in meta.items():
    f=os.path.join(d,nm+'.jsonl')
    if not os.path.exists(f): continue
    rows=[json.loads(l) for l in open(f) if l.strip()]; rows=[r for r in rows if r['ch']]
    if not rows: continue
    g+=1; mine=0 if m['side']=='a' else 1
    spawn={c[0]:(c[3],c[4]) for c in rows[0]['ch']}; prev=None
    for r in rows:
        if r['r']>R1: break
        if prev and r['r']>R0:
            pc={(c[1],c[2]):c[3] for c in prev['camps']}; now={(c[1],c[2]):c[3] for c in r['camps']}
            hits=[(k[0],k[1],v-now.get(k,0)) for k,v in pc.items() if now.get(k,0)<v]
            pp={c[0]:c for c in prev['ch']}; hurt={c[0] for c in r['ch'] if c[0] in pp and c[5]<pp[c[0]][5]}
            for c in r['ch']:
                tag='us' if c[1]==mine else 'th'; t=tot[tag]
                if not c[7]: t['dead']+=1; continue
                x,y=c[3],c[4]
                if any(cheb(x,y,h[0],h[1])<=4 for h in hits): t['camp']+=1
                elif any(e[1]!=c[1] and e[7] and cheb(x,y,e[3],e[4])<=4 and (c[0] in hurt or e[0] in hurt) for e in r['ch']): t['fight']+=1
                elif cheb(x,y,*spawn[c[0]])<=2: t['spawn']+=1
                elif c[0] in pp and (pp[c[0]][3],pp[c[0]][4])!=(x,y): t['move']+=1
                else: t['still']+=1
            for x,y,dd in hits:
                best=min(((cheb(c[3],c[4],x,y),c[1]) for c in r['ch'] if c[7]),default=(99,0))
                if best[0]<=5: dm['us' if best[1]==mine else 'th']+=dd
        prev=r
print(g,'games, rounds %d-%d'%(R0+1,R1))
for tag in ('us','th'):
    n=sum(tot[tag].values())
    print('  %s: '%tag+'  '.join('%s %.1f%%'%(k,100*tot[tag][k]/n) for k in ('camp','fight','move','still','spawn','dead'))+'   champion-turns/game %.0f  camp dmg/game %.0f  dmg per camp turn %.0f'%(n/g,dm[tag]/g,dm[tag]/max(1,tot[tag]['camp'])))
