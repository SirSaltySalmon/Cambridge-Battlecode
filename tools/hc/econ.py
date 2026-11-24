"""Per-champion economy over time from a match summary. usage: python tools/hc/econ.py SUMMARY.json TEAM(teamA|teamB)
Earned gold, camp damage done and taken at rounds 100..700, then the purchase log (round, champion, item x count)."""
import sys,json,collections
d=json.load(open(sys.argv[1],encoding='utf-8')); T=sys.argv[2]
RS=[100,200,300,400,500,600,700]
def at(runs,r):
    t=0;v=0.0
    for x in runs:
        if t>r: break
        v=float(x['value']); t+=x['duration']
    return v
names={c['id']:c['name'][:7] for c in d['champions'] if c['team']==T}
for met in ('earnedGold','monsterDamageDone','monsterDamageTaken','collectedGems','championDamageDone'):
    print(met)
    tot=[0]*len(RS)
    for t in d['championTimelines']:
        if t['metric']==met and t['owner']['id'] in names:
            v=[at(t['runs'],R) for R in RS]; tot=[a+b for a,b in zip(tot,v)]
            print('  %-8s'%names[t['owner']['id']]+' '.join('%7.0f'%x for x in v))
    print('  %-8s'%'TEAM'+' '.join('%7.0f'%x for x in tot))
buys=[]
for e in d['events']:
    if e['kind']=='buyItem' and e['owner'].get('id') in names:
        for x in e['entries']: buys.append((x['timestamp']['round'],names[e['owner']['id']],x['target']['name'].replace(' Shard of ',':'),x['value']))
buys.sort()
line=''
for r,n,i,v in buys:
    if r<=450: print('  r%d %s %s x%s'%(r,n,i,v))
