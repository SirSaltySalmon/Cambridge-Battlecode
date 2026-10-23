"""Back-and-forth turns: a champion that moved on two turns in a row and stands where it stood two turns ago.
usage: python analyze_shuffle.py DIR  (bot under test: team a in -a files, team b in -b files)"""
import glob,zipfile,re,collections,sys
LINE=re.compile(r"^\s*r(\d+) c(\d+) \((\d+),(\d+)\) (\S+): (.*?) \| (.*)$",re.M)
d=sys.argv[1]
turns=0; moved=0; osc=0
pair=collections.Counter(); br=collections.Counter(); brall=collections.Counter(); runs=collections.Counter()
reason=collections.Counter()
for f in glob.glob(d+'/*.repl26a'):
    side='a' if f.endswith('-a.repl26a') else 'b'
    t=zipfile.ZipFile(f).read(f'team-{side}.stdout').decode(errors='replace').replace('\x00','')
    seq=collections.defaultdict(list)
    for m in LINE.finditer(t):
        seq[int(m.group(2))].append((int(m.group(1)),(int(m.group(3)),int(m.group(4))),m.group(5),m.group(6),m.group(7)))
    for c,s in seq.items():
        # positions logged are at turn start; position after turn r = start of r+1
        streak=0
        for i in range(len(s)-3):
            r0,p0,b0,_,a0=s[i]; r1,p1,b1,w1,a1=s[i+1]; r2,p2,b2,w2,a2=s[i+2]; r3,p3,_,_,_=s[i+3]
            if not (r1==r0+1 and r2==r1+1 and r3==r2+1): streak=0; continue
            turns+=1; brall[b1.split('.')[0]+'.'+b1.split('.')[1] if '.' in b1 else b1]+=1
            # turn i+1 moved p1->p2, turn i+2 moved p2->p3 back to p1
            if p1!=p2 and p3==p1 and 'move' in a1 and 'move' in a2 and 'attack' not in a1+a2 and 'cast' not in a1+a2:
                osc+=1; streak+=1
                k1=b1; k2=b2
                pair[(k1,k2)]+=1; br[k1]+=1
                reason[(b1, re.sub(r'\(\d+,\d+\)|s\d+|\d+','#',w1)[:90])]+=1
            else:
                if streak: runs[min(streak,10)]+=1
                streak=0
print(d,'turns',turns,'back-and-forth turns',osc,'(%.1f%%)'%(100*osc/max(1,turns)))
print('streak lengths (turns in a row):',sorted(runs.items()))
print('\nby branch (share of back-and-forth turns; share of that branch\'s turns):')
for b,n in br.most_common(14): print('  %-26s %6d %5.1f%%  %5.1f%% of branch'%(b,n,100*n/osc,100*n/max(1,brall.get(b,0)) if b in brall else 0))
print('\ntop branch pairs (move there, move back):')
for (a,b),n in pair.most_common(14): print('  %-24s -> %-24s %6d'%(a,b,n))
print('\ntop reasons:')
for (b,w),n in reason.most_common(14): print('  %6d %-22s %s'%(n,b,w))
