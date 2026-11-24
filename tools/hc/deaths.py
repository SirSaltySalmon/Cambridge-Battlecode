"""Deaths and kills per champion kind for the bot under test in an h2h dir, split by cluster maps / others.
usage: python tools/hc/deaths.py replays/hc/h2h-BOT-OLD"""
import sys,glob,json,os,collections
CL={'00galaxy','00gem-fields','00quadrants','00vault','04atoll','04gem-caverns','04gem-rush','04tidepools','adgato-gemcombat1','adgato-gemcombat2','adgato-gemcombat3','adgato-gemcombat4','adgato-gemcombat5'}
st={True:collections.defaultdict(lambda:[0,0,0,0,0]),False:collections.defaultdict(lambda:[0,0,0,0,0])}
rec={True:[0,0],False:[0,0]}; gems={True:[0,0],False:[0,0]}
for f in glob.glob(sys.argv[1]+'/*.json'):
    j=json.load(open(f,encoding='utf-8')); mp=os.path.basename(f)[:-7]; cl=mp in CL
    us='teamA' if f.endswith('-a.json') else 'teamB'
    rec[cl][0 if j['match']['outcome']==us else 1]+=1
    for t in j['teams']: gems[cl][0 if t['team']==us else 1]+=int(t['submittedGems'])
    for c in j['champions']:
        m=c['metrics']; k=('US ' if c['team']==us else 'th ')+c['name'][:8]
        s=st[cl][k]; s[0]+=1; s[1]+=int(m['deaths']); s[2]+=int(m['kills']); s[3]+=int(m['collectedGems']); s[4]+=int(m['earnedGold'])
for cl in (True,False):
    print('cluster maps' if cl else 'other maps', 'record %d-%d'%tuple(rec[cl]), 'gems %d vs %d'%tuple(gems[cl]))
    for k,s in sorted(st[cl].items()): print('   %-12s games %3d  deaths/g %.2f  kills/g %.2f  collected/g %4.0f  gold/g %5.0f'%(k,s[0],s[1]/s[0],s[2]/s[0],s[3]/s[0],s[4]/s[0]))
