import json,sys
for f in sys.argv[1:]:
    j=json.load(open(f))
    print(f, j['match']['outcome'], [(t['team'],t['submittedGems']) for t in j['teams']])
    for e in j['events']:
        if e['kind']=='death':
            for x in e['entries']:
                if x.get('context')=='camp':
                    o=e['owner']; t=x['target']; print('   r%d %s %s died (%d,%d) to %s'%(x['timestamp']['round'],o.get('team'),o.get('name'),x['x'],x['y'],t.get('name',t)))
