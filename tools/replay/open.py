"""Opening timeline of one team from a decoded replay: per 20 rounds, camp damage dealt near the team, gold and gems held
plus max health (items), deaths. usage: python tools/replay/open.py X.jsonl TEAM [rounds=300] [bin=20]"""
import sys,json
rows=[json.loads(l) for l in open(sys.argv[1]) if l.strip()]
rows=[r for r in rows if r['ch']]
T=int(sys.argv[2]); R=int(sys.argv[3]) if len(sys.argv)>3 else 300; B=int(sys.argv[4]) if len(sys.argv)>4 else 20
def cheb(a,b,c,d): return max(abs(a-c),abs(b-d))
prev=None; dmg={}; eng={}; alive={}
for r in rows:
    if r['r']>R: break
    if prev:
        pc={(c[1],c[2]):c[3] for c in prev['camps']}; now={(c[1],c[2]):c[3] for c in r['camps']}
        hits=[(k[0],k[1],v-now.get(k,0)) for k,v in pc.items() if now.get(k,0)<v]
        mine=[c for c in r['ch'] if c[1]==T and c[7]]
        b=r['r']//B
        for x,y,d in hits:
            if any(cheb(c[3],c[4],x,y)<=5 for c in mine): dmg[b]=dmg.get(b,0)+d
        for c in mine:
            alive[b]=alive.get(b,0)+1
            if any(cheb(c[3],c[4],x,y)<=4 for x,y,d in hits): eng[b]=eng.get(b,0)+1
    prev=r
print('round  campdmg  engaged%  ')
for b in sorted(alive): print('%4d   %6d   %5.1f'%(b*B,dmg.get(b,0),100*eng.get(b,0)/max(1,alive[b])))
