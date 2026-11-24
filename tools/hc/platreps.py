"""Download and decode the replays of the test cells whose key contains any given substring.
usage: python tools/hc/platreps.py NAME OUTDIR sub1 [sub2 ...]   -> OUTDIR/<opponent>_<map>_<side>.jsonl + meta.json"""
import json,sys,subprocess,os,zipfile,tempfile,time
name,out=sys.argv[1],sys.argv[2]; os.makedirs(out,exist_ok=True)
cells=json.load(open('results/plat/%s-cells.json'%name))
meta=json.load(open(out+'/meta.json')) if os.path.exists(out+'/meta.json') else {}
for k,v in sorted(cells.items()):
    if not any(s in k for s in sys.argv[3:]): continue
    nm=k.replace('|','_').replace(' ','').replace('=','').replace('^','').replace('+','')
    o=os.path.join(out,nm+'.jsonl'); meta[nm]=dict(side=k[-1],win=v['win'],key=k)
    if os.path.exists(o) and os.path.getsize(o)>1000: continue
    t=tempfile.mkdtemp(); z=os.path.join(t,'r.zip')
    for i in range(8):
        subprocess.run(['cambc','matches','download',v['id'],'-o',z],capture_output=True)
        if os.path.exists(z): break
        time.sleep(1+i)
    zipfile.ZipFile(z).extract('replay.repl26a',t)
    with open(o,'w') as f: subprocess.run(['node','tools/replay/replay.mjs',os.path.join(t,'replay.repl26a')],stdout=f)
json.dump(meta,open(out+'/meta.json','w'))
print(len(meta),'replays')
