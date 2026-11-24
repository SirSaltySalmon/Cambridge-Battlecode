"""Table of every cell of a platform test: result, rounds, gems banked at r300/r600/end, collected, kills/deaths, gold.
usage: python tools/hc/plattab.py NAME [NAME2 ...]   (downloads missing summaries)"""
import json,sys,subprocess,os,time,collections
def at(runs,r):
    t=0;v=0.0
    for x in runs:
        if t>r: break
        v=float(x['value']); t+=x['duration']
    return v
os.makedirs('results/plat/sum',exist_ok=True)
for name in sys.argv[1:]:
    cells=json.load(open('results/plat/%s-cells.json'%name)); rows=[]
    for k,v in sorted(cells.items()):
        o='results/plat/sum/%s.json'%v['id']
        for i in range(10):
            if os.path.exists(o): break
            subprocess.run(['cambc','matches','summary',v['id'],'--json','-o',o],capture_output=True); time.sleep(0.6+i)
        if not os.path.exists(o): continue
        d=json.load(open(o,encoding='utf-8')); us='teamA' if k.endswith('|a') else 'teamB'; th='teamB' if us=='teamA' else 'teamA'
        tl={t['owner']:t['runs'] for t in d['teamTimelines'] if t['metric']=='submittedGems'}
        def agg(T,key): return sum(float(c['metrics'][key]) for c in d['champions'] if c['team']==T)
        rows.append(dict(k=k,win=d['match']['outcome']==us,R=d['match']['roundsCompleted'],
            g3=(at(tl[us],300),at(tl[th],300)),g6=(at(tl[us],600),at(tl[th],600)),ge=(at(tl[us],9999),at(tl[th],9999)),
            col=(agg(us,'collectedGems'),agg(th,'collectedGems')),kd=(agg(us,'kills'),agg(us,'deaths')),gold=(agg(us,'earnedGold'),agg(th,'earnedGold')),
            md=(agg(us,'monsterDamageDone'),agg(th,'monsterDamageDone'))))
    json.dump(rows,open('results/plat/%s-table.json'%name,'w'))
    print(name,len(rows),'cells')
    def mean(l): return sum(l)/max(1,len(l))
    for w in (True,False):
        rs=[r for r in rows if r['win']==w]
        print('  %s n=%d rounds %.0f | banked r300 %.0f/%.0f r600 %.0f/%.0f end %.0f/%.0f | collected %.0f/%.0f | K/D %.1f/%.1f | gold %.0f/%.0f | camp dmg %.0f/%.0f'%('wins  ' if w else 'losses',len(rs),mean([r['R'] for r in rs]),
            mean([r['g3'][0] for r in rs]),mean([r['g3'][1] for r in rs]),mean([r['g6'][0] for r in rs]),mean([r['g6'][1] for r in rs]),mean([r['ge'][0] for r in rs]),mean([r['ge'][1] for r in rs]),
            mean([r['col'][0] for r in rs]),mean([r['col'][1] for r in rs]),mean([r['kd'][0] for r in rs]),mean([r['kd'][1] for r in rs]),mean([r['gold'][0] for r in rs]),mean([r['gold'][1] for r in rs]),mean([r['md'][0] for r in rs]),mean([r['md'][1] for r in rs])))
