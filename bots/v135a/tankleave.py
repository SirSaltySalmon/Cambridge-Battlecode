# find rounds where a champion takes a hurt.* branch while it was farm.camp the round before and no enemy near (no fight/retreat in team)
import glob,re,collections,sys,os
LINE=re.compile(r"^\s*r(\d+) c(\d+) \((\d+),(\d+)\) (\S+): (.*?) \| (.*)$")
cnt=collections.Counter(); after=collections.Counter(); eg=[]
for f in glob.glob(os.path.join(sys.argv[1],sys.argv[2])):
    byr=collections.defaultdict(dict)
    for line in open(f,encoding='utf-8',errors='replace'):
        m=LINE.match(line.replace(chr(0),' '))
        if m: byr[int(m.group(1))][int(m.group(2))]=(m.group(5),(int(m.group(3)),int(m.group(4))),m.group(7))
    for r in sorted(byr):
        for c,(b,p,a) in byr[r].items():
            prev=byr.get(r-1,{}).get(c)
            if not prev or not prev[0].startswith('farm.camp') or not (b.startswith('hurt') or b=='camp.exit'): continue
            if any(x[0].startswith(('fight','retreat')) for x in byr[r].values()): continue
            cnt[(c,b)]+=1
            # what do teammates who were farm.camp at r do over r+1..r+4
            for c2,(b2,p2,a2) in byr[r].items():
                if c2==c or not b2.startswith('farm.camp'): continue
                seq=[byr.get(r+k,{}).get(c2,('?',))[0] for k in range(1,5)]
                after[tuple(x.split(':')[0] for x in seq)]+=1
            if len(eg)<12: eg.append((os.path.basename(f),r,c,b))
print(cnt.most_common(20)); print(); [print(n,k) for k,n in after.most_common(20)]; print(eg)
