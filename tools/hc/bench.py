"""Hill-climb bench helper (run from the repo root).
  python tools/hc/bench.py solo BOT [jobs]            BOT vs bots/idle on every map, debug logs -> replays/hc/solo-BOT/
  python tools/hc/bench.py h2h BOT OLD [jobs] [dbg]   BOT vs OLD on every map, both sides, release -> replays/hc/h2h-BOT-OLD/
  python tools/hc/bench.py solosum BOT [BASE]         rounds to finish per map (vs BASE if given), totals
  python tools/hc/bench.py h2hsum BOT OLD             record, gems, kills/deaths, out-of-fuel, per-map losses
"""
import glob,json,os,subprocess,sys
def maps(): return sorted(glob.glob('maps/*.map26a'))
def run(rows,name,jobs,release):
    path='hc_%s.txt'%name
    open(path,'w').write('\n'.join(rows)+'\n')
    cmd=['cambc','run-batch',path,'--jobs',str(jobs),'--json','--log-limit','40000000' if not release else '0']
    if release: cmd.append('--release')
    r=subprocess.run(cmd,capture_output=True,text=True)
    # run-batch resolves manifest rows relative to the manifest, so it is written in the repo root for the run and
    # then kept under results/hc/manifests (CLAUDE.md: no bench output in the root).
    os.makedirs('results/hc/manifests',exist_ok=True)
    os.replace(path,'results/hc/manifests/'+path)
    print((r.stdout+r.stderr).strip().split('\n')[-1])
def clean(d):
    os.makedirs(d,exist_ok=True)
    for f in glob.glob(d+'/*'): os.remove(f)
def solo(bot,jobs=16):
    d='replays/hc/solo-%s'%bot; clean(d)
    rows=['bots/%s bots/idle %s %s/%s.repl26a --statistics %s/%s.json --json'%(bot,m.replace(chr(92),'/'),d,os.path.basename(m)[:-7],d,os.path.basename(m)[:-7]) for m in maps()]
    run(rows,'solo-'+bot,jobs,False)
def h2h(bot,old,jobs=16,dbg=False):
    d='replays/hc/h2h-%s-%s'%(bot,old); clean(d); rows=[]
    for m in maps():
        n=os.path.basename(m)[:-7]; m=m.replace(chr(92),'/')
        rows.append('bots/%s bots/%s %s %s/%s-a.repl26a --statistics %s/%s-a.json --json'%(bot,old,m,d,n,d,n))
        rows.append('bots/%s bots/%s %s %s/%s-b.repl26a --statistics %s/%s-b.json --json'%(old,bot,m,d,n,d,n))
    run(rows,'h2h-%s-%s'%(bot,old),jobs,not dbg)
def at(runs,r):
    t=0;v=0
    for x in runs:
        if t>r: break
        v=int(float(x['value'])); t+=x['duration']
    return v
def soloread(bot):
    out={}
    for f in glob.glob('replays/hc/solo-%s/*.json'%bot):
        j=json.load(open(f,encoding='utf-8'))
        tl=[t['runs'] for t in j['teamTimelines'] if t['metric']=='submittedGems' and t['owner']=='teamA'][0]
        g=at(tl,99999); R=j['match']['roundsCompleted']
        dth=sum(int(c['metrics']['deaths']) for c in j['champions'] if c['team']=='teamA')
        fx=sum(len(e['entries']) for e in j['events'] if e['kind']=='fuelExhausted' and e['owner']['team']=='teamA')
        out[os.path.basename(f)[:-5]]=dict(R=R if g>=1000 else 2000,g=g,g3=at(tl,300),g6=at(tl,600),d=dth,fx=fx)
    return out
def solosum(bot,base=None):
    a=soloread(bot); b=soloread(base) if base else None
    ks=sorted(a)
    def tot(x): return 'maps %d  mean rounds %.0f  unfinished %d  g300 %.0f  g600 %.0f  deaths %d  out-of-fuel %d'%(len(x),sum(v['R'] for v in x.values())/len(x),sum(v['g']<1000 for v in x.values()),sum(v['g3'] for v in x.values())/len(x),sum(v['g6'] for v in x.values())/len(x),sum(v['d'] for v in x.values()),sum(v['fx'] for v in x.values()))
    if b:
        ks=[k for k in ks if k in b]
        diffs=sorted((a[k]['R']-b[k]['R'],k) for k in ks)
        print('faster on %d, slower on %d, same %d'%(sum(d<0 for d,_ in diffs),sum(d>0 for d,_ in diffs),sum(d==0 for d,_ in diffs)))
        print('most faster:',' '.join('%s %+d'%(k,d) for d,k in diffs[:12]))
        print('most slower:',' '.join('%s %+d'%(k,d) for d,k in diffs[-12:][::-1]))
        print(base,tot({k:b[k] for k in ks}))
    print(bot,tot({k:a[k] for k in ks}))
def h2hsum(bot,old):
    d='replays/hc/h2h-%s-%s'%(bot,old)
    t=dict(w=0,l=0,us=0,th=0,k=0,dd=0,fx=0,ofx=0,cd=0,ocd=0,n=0); per={}
    for f in glob.glob(d+'/*.json'):
        j=json.load(open(f,encoding='utf-8'))
        us='teamA' if f.endswith('-a.json') else 'teamB'
        gems={x['team']:int(x['submittedGems']) for x in j['teams']}; th=[k for k in gems if k!=us][0]
        win=j['match']['outcome']==us
        t['n']+=1; t['w' if win else 'l']+=1; t['us']+=gems[us]; t['th']+=gems[th]
        mp=os.path.basename(f)[:-7]; per.setdefault(mp,[0,0])[0 if win else 1]+=1
        for e in j['events']:
            mine=e['owner']['team']==us; n=len(e['entries'])
            if e['kind']=='death':
                t['dd' if mine else 'k']+=n
                c=sum(1 for x in e['entries'] if x.get('context')=='camp'); t['cd' if mine else 'ocd']+=c
            elif e['kind']=='fuelExhausted': t['fx' if mine else 'ofx']+=n
    print('%s vs %s: %d-%d (%.1f%%)  gems %d vs %d (%+.1f%%)  K/D %d/%d  camp deaths %d vs %d  out-of-fuel %d vs %d'%(bot,old,t['w'],t['l'],100*t['w']/max(1,t['n']),t['us'],t['th'],100*(t['us']-t['th'])/max(1,t['th']),t['k'],t['dd'],t['cd'],t['ocd'],t['fx'],t['ofx']))
    print('0-2 maps:',' '.join(k for k,v in sorted(per.items()) if v[0]==0))
    print('2-0 maps:',' '.join(k for k,v in sorted(per.items()) if v[1]==0))
if __name__=='__main__':
    a=sys.argv[1:]
    if a[0]=='solo': solo(a[1],int(a[2]) if len(a)>2 else 16)
    elif a[0]=='h2h': h2h(a[1],a[2],int(a[3]) if len(a)>3 else 16,len(a)>4)
    elif a[0]=='solosum': solosum(a[1],a[2] if len(a)>2 else None)
    elif a[0]=='h2hsum': h2hsum(a[1],a[2])
