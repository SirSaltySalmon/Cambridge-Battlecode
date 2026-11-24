"""Branch tally of one replay bundle. usage: python tools/hc/tally.py FILE.repl26a [team=a] [maxround]"""
import sys,zipfile,re,collections
t=sys.argv[2] if len(sys.argv)>2 else 'a'; mr=int(sys.argv[3]) if len(sys.argv)>3 else 10**9
s=zipfile.ZipFile(sys.argv[1]).read('team-%s.stdout'%t).replace(b'\0',b'').decode('utf-8','replace')
pat=re.compile(r'^r(\d+) c(\d+) \((\d+),(\d+)\)(?: IN-CHASE-RING)? ([\w.\-]+): ([^|\n]*)',re.M)
c=collections.Counter(); w=collections.Counter()
for m in pat.finditer(s):
    if int(m.group(1))>mr: continue
    c[(m.group(2),m.group(5))]+=1; w[(m.group(5),re.sub(r'\d+','N',m.group(6))[:120])]+=1
for ch in sorted(set(k[0] for k in c)): print(ch,[(k[1],v) for k,v in c.most_common() if k[0]==ch][:10])
for k,v in w.most_common(24): print(v,k)
n=collections.Counter(re.sub(r'\d+','N',m.group(1)) for m in re.finditer(r'- (spread: [^\n]*)',s))
print(n.most_common(8))
