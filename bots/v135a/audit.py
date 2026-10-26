# audit.py LOGDIR GLOB : uses v135a @s / @land lines
import glob,re,collections,sys,os
TURN=re.compile(r"^\s*r(\d+) c(\d+) (?:\((\d+),(\d+)\)|dead) (\S+): (.*?)(?: \| (.*))?$")
S=re.compile(r"^\s*@s hp (\d+)/(\d+) mode (\d+) slot (-?\d+) cap (\d+)/c(-?\d+)(?: awake(.*?))?(?: tanks(.*))?$")
L=re.compile(r"^\s*@land (\S+) \((\d+),(\d+)\)(?: beside(.*))?$")
def parse(f):
    turns=[]; cur=None
    for line in open(f,encoding='utf-8',errors='replace'):
        line=line.replace(chr(0),' ').rstrip('\n')
        m=TURN.match(line)
        if m:
            cur=dict(r=int(m.group(1)),c=int(m.group(2)),pos=(int(m.group(3)),int(m.group(4))) if m.group(3) else None,b=m.group(5),w=m.group(6),a=m.group(7) or '',lands=[],notes=[])
            turns.append(cur); continue
        if cur is None: continue
        m=S.match(line)
        if m:
            cur.update(hp=int(m.group(1)),mx=int(m.group(2)),mode=int(m.group(3)),slot=int(m.group(4)),cap=int(m.group(5)),col=int(m.group(6)))
            aw=[]
            for k,x,y,who,tie in re.findall(r" (\w)\((\d+),(\d+)\)>(\w+?)(=[c\d]*)?(?= |$)",m.group(7) or ''): aw.append((k,(int(x),int(y)),who,tie))
            cur['awake']=aw; cur['tanks']=(m.group(8) or '').split(); continue
        m=L.match(line)
        if m:
            bes=re.findall(r"g(\d+)\((\d+),(\d+)\)(\w+)",m.group(4) or '')
            cur['lands'].append((m.group(1),(int(m.group(2)),int(m.group(3))),[(int(g),(int(x),int(y)),s) for g,x,y,s in bes])); continue
        if line.strip().startswith('- '): cur['notes'].append(line.strip()[2:])
    return turns
if __name__=='__main__':
    files=glob.glob(os.path.join(sys.argv[1],sys.argv[2]))
    STR={'w':20,'e':60,'g':120,'M':200}
    tie=collections.Counter(); tieid=collections.Counter(); mxs=collections.Counter(); hurtleave=collections.Counter(); hl_tanks=collections.Counter(); land=collections.Counter(); landeg=[]
    for f in files:
        t=parse(f); by=collections.defaultdict(list)
        for x in t: by[x['c']].append(x)
        for c,s in by.items():
            for i in range(len(s)-1):
                x,y=s[i],s[i+1]
                if y['r']!=x['r']+1 or 'hp' not in x or 'hp' not in y: continue
                aw=x.get('awake',[])
                if len(aw)==1 and y['hp']<=x['hp']:
                    k,p,who,ti=aw[0]
                    hit=(x['hp']-y['hp'])>=STR[k]*0.6
                    key=('me' if who=='me' else 'ally')+('=' if ti else '')
                    tie[(key,hit)]+=1
                    if who=='me' and ti:
                        ids=[int(z) for z in re.findall(r'c(\d+)',ti)]
                        lower = c < min(ids)
                        tieid[(lower,hit)]+=1
                        mxs[(x['mx']>=1000,hit)]+=1
            for i in range(1,len(s)):
                x,p=s[i],s[i-1]
                if x['b'].startswith(('hurt','camp.exit')) and p['b'].startswith('farm.camp') and 'hp' in x:
                    hurtleave[x['b']]+=1
                    if x.get('tanks'): hl_tanks[(x['b'],c)]+=1
        for x in t:
            for kind,at,bes in x['lands']:
                risky=[b for b in bes if b[2] not in ('awake','dead')]
                tgt=re.search(r'camp \((\d+),(\d+)\)',x['w'])
                if tgt: tp=(int(tgt.group(1)),int(tgt.group(2))); risky=[b for b in risky if max(abs(b[1][0]-tp[0]),abs(b[1][1]-tp[1]))>2]
                land[(x['c']==12, kind, bool(risky))]+=1
                if risky and x['c']==12 and len(landeg)<40: landeg.append((os.path.basename(f),x['r'],x['b'],kind,at,risky))
    print('tie test (one awake camp in reach; was I hit next round?)')
    for k in ('me','me=','ally','ally='): 
        a,b=tie[(k,True)],tie[(k,False)]; print('  %-6s hit %6d  not %6d  (%.0f%% hit)'%(k,a,b,100*a/max(1,a+b)))
    print('  ties I am in, my id lower than the tied allies:',dict(tieid),' my max hp 1000+:',dict(mxs))
    print('hurt/exit out of a farm:',hurtleave.most_common()); print('  ...with a sturdy healthy ally also in reach:',sum(hl_tanks.values()), hl_tanks.most_common(12))
    print('landings (is HoR, kind, beside a non-awake camp off the target):'); [print('  ',k,n) for k,n in sorted(land.items())]
    for e in landeg: print('  ',e)
