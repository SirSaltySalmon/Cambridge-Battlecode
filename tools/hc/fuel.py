"""Fuel per champion turn for team A (or -a/-b bench files) of every bundle in DIR. usage: python tools/hc/fuel.py DIR [DIR...]"""
import glob,os,sys,zipfile
sys.path.insert(0,'bots/v114')
from fuel_compare import LIMIT, turn_fuel
for d in sys.argv[1:]:
    used=[];worst=[];games=0
    for f in sorted(glob.glob(os.path.join(d,'*.repl26a'))):
        side='b' if f.endswith('-b.repl26a') else 'a'
        u=[x for x in turn_fuel(zipfile.ZipFile(f).read('team-%s.fuel'%side)) if x<=LIMIT]
        used+=u; games+=1; o=sum(1 for x in u if x==LIMIT)
        if o: worst.append((o,os.path.basename(f)[:-8]))
    used.sort(); n=max(1,len(used))
    print('%-28s games %3d turns %7d mean %6.0f median %6d p95 %6d p99 %6d out-of-fuel %5d (%.2f%%)'%(d[-28:],games,len(used),sum(used)/n,used[n//2],used[int(.95*n)],used[int(.99*n)],sum(1 for x in used if x==LIMIT),100*sum(1 for x in used if x==LIMIT)/n))
    worst.sort(reverse=True)
    print('    worst: '+', '.join('%s %d'%(m,c) for c,m in worst[:10]))
