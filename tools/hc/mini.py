"""Debug mini-bench on the 25 maps of platform test set 2 (tools/hc/plat2.py), both sides, with decision logs.
usage: python tools/hc/mini.py BOT OLD [jobs=16]   -> replays/hc/mini-BOT-OLD/<map>-<side>.repl26a + .json
       python tools/hc/mini.py sum BOT OLD         record, gems, K/D, out-of-fuel (as bench.py h2hsum)"""
import glob, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import bench, plat2

def run(bot, old, jobs=16):
    d = 'replays/hc/mini-%s-%s' % (bot, old); bench.clean(d); rows = []
    have = {os.path.basename(m)[:-7].replace('-', '_'): m.replace(chr(92), '/') for m in bench.maps()}
    for n in plat2.MAPS:
        m = have[n]
        rows.append('bots/%s bots/%s %s %s/%s-a.repl26a --statistics %s/%s-a.json --json' % (bot, old, m, d, n, d, n))
        rows.append('bots/%s bots/%s %s %s/%s-b.repl26a --statistics %s/%s-b.json --json' % (old, bot, m, d, n, d, n))
    bench.run(rows, 'mini-%s-%s' % (bot, old), jobs, False)

if __name__ == '__main__':
    a = sys.argv[1:]
    if a[0] == 'sum':
        d = 'replays/hc/h2h-%s-%s' % (a[1], a[2]); src = 'replays/hc/mini-%s-%s' % (a[1], a[2])
        # h2hsum reads replays/hc/h2h-*: point it at the mini folder through a junction-free copy of the name.
        orig = bench.glob.glob
        bench.glob.glob = lambda p: orig(p.replace(d, src))
        bench.h2hsum(a[1], a[2])
    else:
        run(a[0], a[1], int(a[2]) if len(a) > 2 else 16)
