import glob,re,collections,sys,os
LINE=re.compile(r"^\s*r(\d+) c(\d+) \((\d+),(\d+)\) (\S+): (.*?) \| (.*)$")
norm=lambda w: re.sub(r'\(\d+,\d+\)','(#)',re.sub(r's\d+','s#',re.sub(r'\b\d+\b','#',w)))[:110]
rs=collections.Counter(); ex=collections.defaultdict(list)
for f in glob.glob(os.path.join(sys.argv[1],sys.argv[2])):
    seq=collections.defaultdict(list)
    for line in open(f,encoding='utf-8',errors='replace'):
        m=LINE.match(line.replace(chr(0),' '))
        if m: seq[int(m.group(2))].append((int(m.group(1)),(int(m.group(3)),int(m.group(4))),m.group(5),m.group(6),m.group(7)))
    for c,s in seq.items():
        for i in range(len(s)-3):
            r0,p0,b0,_,a0=s[i]; r1,p1,b1,w1,a1=s[i+1]; r2,p2,b2,w2,a2=s[i+2]; r3,p3,_,_,_=s[i+3]
            if not (r1==r0+1 and r2==r1+1 and r3==r2+1): continue
            mv=lambda a:('move' in a or 'dash' in a or 'hop' in a)
            if p1!=p2 and p3==p1 and mv(a1) and mv(a2) and 'attack' not in a1+a2 and 'cast' not in a1+a2:
                k=(b1+': '+norm(w1), b2+': '+norm(w2)); rs[k]+=1
                if len(ex[k])<3: ex[k].append((os.path.basename(f),c,r1))
for k,n in rs.most_common(int(sys.argv[3]) if len(sys.argv)>3 else 25):
    print(n); print('   ',k[0]); print('   ',k[1]); print('    eg',ex[k])
