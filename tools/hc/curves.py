"""Team curves from match summaries: cumulative camp damage, earned gold, collected and tributed gems at fixed rounds.
usage: python tools/hc/curves.py top SCRATCH   |   python tools/hc/curves.py solo BOT [mapfilter-file]"""
import sys,json,glob,os,collections
RS=[100,200,300,400,500,600]
def at(runs,r):
    t=0;v=0.0
    for x in runs:
        if t>r: break
        v=float(x['value']); t+=x['duration']
    return v
def curves(d,team):
    out={}
    ids={c['id'] for c in d['champions'] if c['team']==team}
    for met in ('monsterDamageDone','earnedGold','collectedGems','monsterDamageTaken','deaths'):
        tls=[t['runs'] for t in d['championTimelines'] if t['metric']==met and t['owner']['id'] in ids]
        out[met]=[sum(at(r,R) for r in tls) for R in RS]
    tl=[t['runs'] for t in d['teamTimelines'] if t['metric']=='submittedGems' and t['owner']==team][0]
    out['submitted']=[at(tl,R) for R in RS]
    return out
res={}
if sys.argv[1]=='top':
    S=sys.argv[2]
    idx=json.load(open(S+'/top_idx.json'))
    tid={1:'547efb5b-57e5-4355-8501-3b331f506d62',3:'ee83f5e5-bcad-4923-86b4-b79ea6083d2e',2:'1890540d-1ffc-4847-adbf-d0d08772f268'}
    lists={rk:{m['matchId']:m for m in json.load(open(S+'/t_%d.json'%rk,encoding='utf-8'))} for rk in tid}
    for rk,mid,mp in idx:
        f=S+'/top/%s.json'%mid
        if not os.path.exists(f): continue
        d=json.load(open(f,encoding='utf-8')); m=lists[rk][mid]
        side=[t['side'] for t in m['teams'] if t['teamId']==tid[rk]][0]; us='teamA' if side=='a' else 'teamB'
        if d['match']['outcome']!=us: continue
        res.setdefault(mp.replace('_','-'),[]).append(curves(d,us))
else:
    for f in glob.glob('replays/hc/solo-%s/*.json'%sys.argv[2]):
        res.setdefault(os.path.basename(f)[:-5],[]).append(curves(json.load(open(f,encoding='utf-8')),'teamA'))
json.dump(res,open('hc_curves_%s.json'%(sys.argv[1] if sys.argv[1]=='top' else sys.argv[2]),'w'))
keys=sorted(res)
if len(sys.argv)>3 and sys.argv[1]!='top':
    keep=set(json.load(open(sys.argv[3]))); keys=[k for k in keys if k in keep]
print(len(keys),'maps')
for met in ('monsterDamageDone','earnedGold','collectedGems','submitted','monsterDamageTaken','deaths'):
    print('%-20s'%met+' '.join('r%d %7.0f'%(R,sum(sum(c[met][i] for c in res[k])/len(res[k]) for k in keys)/len(keys)) for i,R in enumerate(RS)))
