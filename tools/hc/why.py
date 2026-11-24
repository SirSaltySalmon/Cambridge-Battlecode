"""Count decision reasons (digits masked) for branches matching a prefix. usage: python tools/hc/why.py DIR PREFIX [mapsub] [team=a]"""
import sys,re,glob,os,zipfile,collections
d,pre=sys.argv[1],sys.argv[2]; sub=sys.argv[3] if len(sys.argv)>3 else ''; team=sys.argv[4] if len(sys.argv)>4 else 'a'
pat=re.compile(r'^r(\d+) c(\d+) \((\d+),(\d+)\)(?: IN-CHASE-RING)? ([\w.\-]+): ([^|\n]*)',re.M)
c=collections.Counter()
for z in glob.glob(os.path.join(d,'*.repl26a')):
    if sub not in z: continue
    s=zipfile.ZipFile(z).read('team-%s.stdout'%team).replace(b'\0',b'').decode('utf-8','replace')
    for m in pat.finditer(s):
        if m.group(5).startswith(pre): c[(m.group(5),re.sub(r'\d+','N',m.group(6)).strip()[:150])]+=1
for k,v in c.most_common(40): print(v,k[0],'|',k[1])
