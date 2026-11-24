"""Download + decode the replay of one test cell. usage: python tools/hc/platrep.py NAME 'opponent|map|side' OUT.jsonl"""
import json,sys,subprocess,os,zipfile,tempfile
cells=json.load(open('results/plat/%s-cells.json'%sys.argv[1])); k=[x for x in cells if sys.argv[2] in x][0]
t=tempfile.mkdtemp(); z=os.path.join(t,'r.zip')
subprocess.run(['cambc','matches','download',cells[k]['id'],'-o',z],capture_output=True)
zipfile.ZipFile(z).extract('replay.repl26a',t)
with open(sys.argv[3],'w') as f: subprocess.run(['node','tools/replay/replay.mjs',os.path.join(t,'replay.repl26a')],stdout=f)
print(k,cells[k])
