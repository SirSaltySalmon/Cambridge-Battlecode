"""Context of our deaths in decoded test replays: allies and enemies within 6 tiles two rounds before, the victim's
gems, and what the enemy team looks like (how many enemies were within 12 of each other). usage: python tools/hc/deathctx.py DIR"""
import sys,json,os,collections
d=sys.argv[1]; meta=json.load(open(d+'/meta.json'))
N=['Ryth','Vesper','Vend','Cassi','Issac','Stell','Prism','Marc','Willow','Ren','Varg','Tavi','HoR']
def cheb(a,b,c,e): return max(abs(a-c),abs(b-e))
ctx=collections.Counter(); kinds=collections.Counter(); gems=0; n=0; games=0; grp=collections.Counter(); egrp=collections.Counter(); samples=0
for nm,m in meta.items():
    f=os.path.join(d,nm+'.jsonl')
    if not os.path.exists(f): continue
    rows=[json.loads(l) for l in open(f) if l.strip()]; rows=[r for r in rows if r['ch']]
    if not rows: continue
    games+=1; mine=0 if m['side']=='a' else 1
    for i in range(3,len(rows)):
        r=rows[i]; p=rows[i-1]; b=rows[i-3]
        if r['r']%10==0:
            for team,cnt in ((mine,grp),(1-mine,egrp)):
                al=[c for c in r['ch'] if c[1]==team and c[7]]
                for c in al: cnt[min(4,sum(1 for a in al if a[0]!=c[0] and cheb(a[3],a[4],c[3],c[4])<=6))]+=1
        pa={c[0]:c for c in p['ch']}
        for c in r['ch']:
            if c[1]==mine and not c[7] and pa.get(c[0]) and pa[c[0]][7]:
                v=[x for x in b['ch'] if x[0]==c[0]][0]
                if not v[7]: v=pa[c[0]]
                al=sum(1 for x in b['ch'] if x[1]==mine and x[7] and x[0]!=c[0] and cheb(x[3],x[4],v[3],v[4])<=6)
                en=sum(1 for x in b['ch'] if x[1]!=mine and x[7] and cheb(x[3],x[4],v[3],v[4])<=6)
                ctx[(min(al,3),min(en,4))]+=1; kinds[N[c[2]]]+=1; gems+=v[9]; n+=1
print(games,'games,',n,'deaths (%.1f a game), gems lost with them %.0f a game'%(n/max(1,games),gems/max(1,games)))
print('victims:',kinds.most_common())
print('allies near (rows) x enemies near (cols 0..4+), two rounds before:')
for a in range(4): print('   allies %d%s: '%(a,'+' if a==3 else ' ')+' '.join('%3d'%ctx[(a,e)] for e in range(5)))
for tag,cnt in (('our',grp),('their',egrp)):
    t=sum(cnt.values()); print('  %s champions with k allies within 6 (every 10th round): '%tag+' '.join('%d:%.0f%%'%(k,100*cnt[k]/t) for k in range(5)))
