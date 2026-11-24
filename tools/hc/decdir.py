"""Decode every replay bundle in DIR to OUT/<name>.jsonl (parallel). usage: python tools/hc/decdir.py DIR OUT [raw]
raw: DIR holds bare .repl26a engine replays (ladder downloads) instead of local bundles."""
import sys,os,glob,zipfile,subprocess,tempfile
from concurrent.futures import ThreadPoolExecutor
d,o=sys.argv[1],sys.argv[2]; raw=len(sys.argv)>3
os.makedirs(o,exist_ok=True)
here=os.path.dirname(os.path.abspath(__file__))
def one(z):
    name=os.path.basename(z)[:-8]; out=os.path.join(o,name+'.jsonl')
    if os.path.exists(out) and os.path.getsize(out)>0: return
    if raw: src=z
    else:
        t=tempfile.mkdtemp(); zipfile.ZipFile(z).extract('replay.repl26a',t); src=os.path.join(t,'replay.repl26a')
    with open(out,'w') as f: subprocess.run(['node',os.path.join(here,'..','replay','replay.mjs'),src],stdout=f)
with ThreadPoolExecutor(12) as ex: list(ex.map(one,glob.glob(os.path.join(d,'*.repl26a'))))
