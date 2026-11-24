"""Download and print the summaries of test-set cells. usage: python tools/hc/platsum.py NAME 'substring of opponent|map|side' ..."""
import json,sys,subprocess,os,time
name=sys.argv[1]; cells=json.load(open('results/plat/%s-cells.json'%name)); os.makedirs('results/plat/sum',exist_ok=True)
def at(runs,r):
    t=0;v=0.0
    for x in runs:
        if t>r: break
        v=float(x['value']); t+=x['duration']
    return v
for pat in sys.argv[2:]:
    for k,v in sorted(cells.items()):
        if pat not in k: continue
        o='results/plat/sum/%s.json'%v['id']
        for i in range(8):
            if os.path.exists(o): break
            subprocess.run(['cambc','matches','summary',v['id'],'--json','-o',o],capture_output=True); time.sleep(1+i)
        d=json.load(open(o,encoding='utf-8')); us='teamA' if k.endswith('|a') else 'teamB'
        tl={t['owner']:t['runs'] for t in d['teamTimelines'] if t['metric']=='submittedGems'}
        print('%s %s: %s in %d rounds; gems us/them at r200 %d/%d r400 %d/%d r600 %d/%d end %d/%d'%(name,k,'WIN' if d['match']['outcome']==us else 'LOSS',d['match']['roundsCompleted'],
            *[x for R in (200,400,600,9999) for x in (at(tl[us],R),at(tl[[t for t in tl if t!=us][0]],R))]))
        for c in d['champions']:
            m=c['metrics']
            print('   %s %-9s K%s D%s col %4s sub %4s gold %5s campdmg %6d taken %5d champdmg %5d | %s'%('US' if c['team']==us else 'th',c['name'][:9],m['kills'],m['deaths'],m['collectedGems'],m['submittedGems'],m['earnedGold'],int(m['monsterDamageDone']),int(m['monsterDamageTaken']),int(m['championDamageDone']),
                ', '.join('%sx%s'%(i['item'].replace(' Emblem of ','E:').replace(' Shard of ','S:')[:12],i['quantity']) for i in c['items'])[:110]))
