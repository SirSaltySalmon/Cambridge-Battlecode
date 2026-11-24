"""Curves over time (us vs them) for a platform test, other-than-cluster maps by default. usage: python tools/hc/platcurve.py NAME [all|cluster|other]"""
import json,sys,collections
CL={'04gem_rush','04atoll','04gem_caverns','00gem_fields','04tidepools','adgato_gemcombat4','adgato_gemcombat5','00galaxy','00quadrants','00vault','adgato_gemcombat1','adgato_gemcombat2','adgato_gemcombat3'}
def at(runs,r):
    t=0;v=0.0
    for x in runs:
        if t>r: break
        v=float(x['value']); t+=x['duration']
    return v
name=sys.argv[1]; which=sys.argv[2] if len(sys.argv)>2 else 'other'
cells=json.load(open('results/plat/%s-cells.json'%name))
acc=collections.defaultdict(lambda:[0.0]*6); n=0; RS=[100,200,300,400,500,600]; per=collections.defaultdict(lambda:[0,0.0,0.0,0.0])
for k,v in cells.items():
    mp=k.split('|')[1]
    if which=='other' and mp in CL: continue
    if which=='cluster' and mp not in CL: continue
    d=json.load(open('results/plat/sum/%s.json'%v['id'],encoding='utf-8')); us='teamA' if k.endswith('|a') else 'teamB'; n+=1
    for T,tag in ((us,'us'),('teamB' if us=='teamA' else 'teamA','th')):
        ids={c['id']:c['name'][:7] for c in d['champions'] if c['team']==T}
        for met in ('collectedGems','earnedGold','monsterDamageDone','deaths','kills','monsterDamageTaken'):
            for t in d['championTimelines']:
                if t['metric']==met and t['owner']['id'] in ids:
                    for i,R in enumerate(RS): acc[(tag,met)][i]+=at(t['runs'],R)
                    if tag=='us' and met in('monsterDamageDone','deaths','earnedGold'):
                        p=per[ids[t['owner']['id']]]
                        if met=='monsterDamageDone': p[0]+=1; p[1]+=at(t['runs'],100)
                        elif met=='deaths': p[2]+=at(t['runs'],600)
                        else: p[3]+=at(t['runs'],600)
        tl=[t['runs'] for t in d['teamTimelines'] if t['metric']=='submittedGems' and t['owner']==T][0]
        for i,R in enumerate(RS): acc[(tag,'banked')][i]+=at(tl,R)
print(name,which,n,'games')
for met in ('monsterDamageDone','collectedGems','banked','earnedGold','kills','deaths','monsterDamageTaken'):
    for tag in ('us','th'): print('  %-20s %s '%(met,tag)+' '.join('%7.1f'%(x/n) for x in acc[(tag,met)]))
print('  per champion (ours): games, camp damage by r100, deaths by r600, gold by r600')
for k,p in sorted(per.items()): print('    %-8s %3d  %6.0f  %4.2f  %6.0f'%(k,p[0],p[1]/p[0],p[2]/p[0],p[3]/p[0]))
