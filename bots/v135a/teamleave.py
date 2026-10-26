import glob,re,collections,sys,os
LINE=re.compile(r"^\s*r(\d+) c(\d+) \((\d+),(\d+)\) (\S+): (.*?) \| (.*)$")
LEAVE=('.exit','.leave','.stray','.share','hurt.dodge','.hold','.back','.out')
def leaving(b): return any(b.endswith(x) or b.startswith(x) for x in LEAVE) or 'leave' in b
cnt=collections.Counter(); eg=[]; reason=collections.Counter()
for f in glob.glob(os.path.join(sys.argv[1],sys.argv[2])):
    byr=collections.defaultdict(dict)
    for line in open(f,encoding='utf-8',errors='replace'):
        m=LINE.match(line.replace(chr(0),' '))
        if m: byr[int(m.group(1))][int(m.group(2))]=(m.group(5),m.group(6))
    last=-99
    for r in sorted(byr):
        if r-last<10: continue
        team=byr[r]; prev=byr.get(r-1,{})
        if any(b.startswith(('fight','retreat')) for b,_ in team.values()): continue
        farmers=[c for c,(b,_) in prev.items() if b.startswith('farm.camp')]
        if len(farmers)<3: continue
        # leavers over r..r+1
        lv=set()
        for k in (0,1):
            for c,(b,w) in byr.get(r+k,{}).items():
                if c in farmers and leaving(b): lv.add(c); reason[b+': '+re.sub(r'[\d(),]+','#',w)[:80]]+=1
        if len(lv)>=2:
            back=sum(1 for c in lv if any(byr.get(r+k,{}).get(c,('',))[0].startswith('farm.camp') for k in range(2,8)))
            cnt[(len(lv),back)]+=1; last=r
            if len(eg)<25: eg.append((os.path.basename(f),r,sorted(lv),back))
print(cnt.most_common()); print(); [print(n,k) for k,n in reason.most_common(25)]; print(eg)
