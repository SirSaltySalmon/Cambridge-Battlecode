"""Text view of a decoded replay frame. usage: python tools/replay/view.py MAP.map26a X.jsonl ROUND [x0 y0 x1 y1]
Tiles: # wall, o hole, " bush, A altar. Camps by max-ish health: w wolf, e elk, G golem, M monolith, * cluster.
Champions: team 0 upper-case initial, team 1 lower-case digit-free initial in brackets list below the grid."""
import sys,json,subprocess,os
mp,jl,rnd=sys.argv[1],sys.argv[2],int(sys.argv[3])
here=os.path.dirname(os.path.abspath(__file__))
out=subprocess.run(['node',os.path.join(here,'map.mjs'),mp],capture_output=True,text=True).stdout.split('\n')
grid=[list(l[4:]) for l in out[2:] if l.strip()]
H=len(grid); W=len(grid[0])
first=None; fr=None
for l in open(jl):
    r=json.loads(l)
    if not r['ch']: continue
    if first is None: first=r
    if r['r']>=rnd: fr=r; break
hp0={(c[1],c[2]):c[3] for c in first['camps']}
def sym(h): return 'w' if h==1350 else 'e' if h==2400 else 'G' if h==5000 else 'M' if h==20000 else '*'
for (x,y),h in hp0.items(): grid[y][x]=sym(h).lower() if False else ','   # dead camp tile
for c in fr['camps']: grid[c[2]][c[1]]=sym(hp0.get((c[1],c[2]),c[3]))
N=['Ryth','Vesper','Vend','Cassi','Issac','Stell','Prism','Marc','Willow','Ren','Varg','Tavi','HoR']
L='RVDCISPMWNGTH'
leg=[]
for c in fr['ch']:
    if c[7] and c[3]>=0:
        ch=L[c[2]] if c[1]==0 else L[c[2]].lower()
        grid[c[4]][c[3]]=ch
    leg.append('%s%s(%d,%d) %d/%d g%d gem%d%s'%('A:' if c[1]==0 else 'b:',N[c[2]],c[3],c[4],c[5],c[6],c[8],c[9],'' if c[7] else ' DEAD'))
x0,y0,x1,y1=(list(map(int,sys.argv[4:8])) if len(sys.argv)>=8 else [0,0,W-1,H-1])
print('round',fr['r'])
print('    '+''.join(str(i%10) for i in range(x0,x1+1)))
for y in range(y0,y1+1): print('%3d '%y+''.join(grid[y][x0:x1+1]))
print('\n'.join(leg))
