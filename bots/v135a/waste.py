# waste.py LOGDIR [glob]: back-and-forth moves, exit->re-entry, by branch
import glob,re,collections,sys,os
LINE=re.compile(r"^\s*r(\d+) c(\d+) \((\d+),(\d+)\) (\S+): (.*?) \| (.*)$")
pat=sys.argv[2] if len(sys.argv)>2 else '*.log'
tot=0; osc=0; pair=collections.Counter(); brall=collections.Counter(); br=collections.Counter()
reent=collections.Counter(); exits=collections.Counter(); gaps=collections.Counter(); permap=collections.Counter()
reent_after=collections.Counter()
for f in glob.glob(os.path.join(sys.argv[1],pat)):
    seq=collections.defaultdict(list)
    for line in open(f,encoding='utf-8',errors='replace'):
        m=LINE.match(line.replace(chr(0),' '))
        if m: seq[int(m.group(2))].append((int(m.group(1)),(int(m.group(3)),int(m.group(4))),m.group(5),m.group(6),m.group(7)))
    for c,s in seq.items():
        for i in range(len(s)):
            tot+=1; brall[s[i][2]]+=1
        for i in range(len(s)-3):
            r0,p0,b0,_,a0=s[i]; r1,p1,b1,w1,a1=s[i+1]; r2,p2,b2,w2,a2=s[i+2]; r3,p3,_,_,_=s[i+3]
            if not (r1==r0+1 and r2==r1+1 and r3==r2+1): continue
            if p1!=p2 and p3==p1 and ('move' in a1 or 'dash' in a1 or 'hop' in a1) and ('move' in a2 or 'dash' in a2 or 'hop' in a2) and 'attack' not in a1+a2 and 'cast' not in a1+a2:
                osc+=1; pair[(b1,b2)]+=1; br[b1]+=1; br[b2]+=1; permap[os.path.basename(f)]+=1
        # exit then re-enter: an *.exit / *.stray / fight.share turn, then within 8 turns a farm.camp/fight.hit/guard.camp turn that moves back into tile distance <=1 of exit start
        for i in range(len(s)):
            r,p,b,w,a=s[i]
            if not (b.endswith('.exit') or b.endswith('.share') or b=='hurt.dodge'): continue
            if i>0 and s[i-1][2]==b and s[i-1][0]==r-1: continue  # first of a run
            exits[b]+=1
            for j in range(i+1,min(len(s),i+12)):
                rj,pj,bj,_,aj=s[j]
                if rj-r>10: break
                if bj.endswith('.exit') or bj=='hurt.dodge' or bj.startswith('hurt') or 'dead' in bj: continue
                if max(abs(pj[0]-p[0]),abs(pj[1]-p[1]))<=1 and j>i+1:
                    reent[b]+=1; gaps[rj-r]+=1; reent_after[(b,s[j-1][2])]+=1; break
print('turns',tot,'back-and-forth',osc,'(%.2f%%)'%(100*osc/max(1,tot)))
print('by branch:'); [print('  %-24s %6d  %5.1f%% of branch'%(b,n,100*n/max(1,brall[b]))) for b,n in br.most_common(16)]
print('pairs:'); [print('  %-22s -> %-22s %5d'%(a,b,n)) for (a,b),n in pair.most_common(16)]
print('maps:',permap.most_common(10))
print('exit starts and re-entries (back within 1 tile of the exit start within 10 rounds):')
for b,n in exits.most_common(): print('  %-18s %6d exits, %6d back (%4.1f%%)'%(b,n,reent[b],100*reent[b]/n))
print('gap rounds:',sorted(gaps.items()))
print('branch that walked back in:',reent_after.most_common(14))
