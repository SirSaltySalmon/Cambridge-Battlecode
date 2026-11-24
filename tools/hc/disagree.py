"""What our champions do while an ally fights nearby, from a local debug bench (decision logs + decoded replays).

usage: python tools/hc/disagree.py DIR BOT_SIDE_RULE [range=12] [-v]
  DIR             replays/hc/mini-BOT-OLD (files <map>-a.repl26a: BOT is team A; <map>-b: BOT is team B)
  BOT_SIDE_RULE   'bot' = the bot under test, 'old' = the other one, 'both'

An ally "fights" in a round when an enemy champion is within 4 tiles of it and one of the two lost health. For every
other living champion of ours within RANGE tiles of a fighting ally that is not fighting itself: its branch.
Also: turn classes of contact turns (enemy within 8, nobody hurt, no camp hit) by branch.
"""
import collections, json, os, re, subprocess, sys, zipfile, glob
a = [x for x in sys.argv[1:] if x != '-v']; verbose = '-v' in sys.argv
d = a[0]; rule = a[1] if len(a) > 1 else 'bot'; RANGE = int(a[2]) if len(a) > 2 else 12
N = ['Ryth', 'Vesper', 'Vend', 'Cassi', 'Issac', 'Stell', 'Prism', 'Marc', 'Willow', 'Ren', 'Varg', 'Tavi', 'HoR']
pat = re.compile(r'^r(\d+) c(\d+) \((\d+),(\d+)\)(?: IN-CHASE-RING)? ([\w.\-]+)(?:: ([^|\n]*))?', re.M)
def cheb(a, b, c, e): return max(abs(a - c), abs(b - e))
def decoded(z):
    o = z[:-8] + '.jsonl'
    if not os.path.exists(o) or os.path.getsize(o) < 1000:
        import tempfile
        t = tempfile.mkdtemp(); zipfile.ZipFile(z).extract('replay.repl26a', t)
        with open(o, 'w') as f: subprocess.run(['node', 'tools/replay/replay.mjs', os.path.join(t, 'replay.repl26a')], stdout=f)
        os.remove(os.path.join(t, 'replay.repl26a'))
    rows = [json.loads(l) for l in open(o) if l.strip()]
    return [r for r in rows if r['ch']]
away = collections.Counter(); away_why = collections.defaultdict(collections.Counter); contact = collections.Counter(); contact_why = collections.defaultdict(collections.Counter)
gapd = collections.Counter(); n_fight_rounds = 0; n_away = 0; turns = 0; games = 0; ex = []
for z in sorted(glob.glob(os.path.join(d, '*.repl26a'))):
    side_a = z.endswith('-a.repl26a')
    teams = [0 if side_a else 1] if rule == 'bot' else [1 if side_a else 0] if rule == 'old' else [0, 1]
    rows = decoded(z); zf = zipfile.ZipFile(z); games += 1
    for mine in teams:
        s = zf.read('team-%s.stdout' % 'ab'[mine]).replace(b'\0', b'').decode('utf-8', 'replace')
        br = {(int(m.group(1)), int(m.group(2))): (m.group(5), (m.group(6) or '').strip()) for m in pat.finditer(s)}
        prev = None
        for r in rows:
            if prev:
                pp = {c[0]: c for c in prev['ch']}
                pc = {(c[1], c[2]): c[3] for c in prev['camps']}; now = {(c[1], c[2]): c[3] for c in r['camps']}
                hits = [k for k, v in pc.items() if now.get(k, 0) < v]
                hurt = {c[0] for c in r['ch'] if c[0] in pp and c[5] < pp[c[0]][5]}
                live = [c for c in r['ch'] if c[7]]; ours = [c for c in live if c[1] == mine]; foes = [c for c in live if c[1] != mine]
                fighting = {c[0] for c in ours if any(cheb(c[3], c[4], e[3], e[4]) <= 4 and (c[0] in hurt or e[0] in hurt) for e in foes)}
                n_fight_rounds += bool(fighting)
                for c in ours:
                    turns += 1
                    b, why = br.get((r['r'], c[2]), ('?', ''))
                    stem = '.'.join(b.split('.')[:2])
                    if c[0] not in fighting:
                        near = [f for f in ours if f[0] in fighting and cheb(c[3], c[4], f[3], f[4]) <= RANGE]
                        if near:
                            n_away += 1; away[stem] += 1; away_why[stem][re.sub(r'\d+', '#', why)[:90]] += 1
                            g = min(cheb(c[3], c[4], f[3], f[4]) for f in near); gapd[min(g, 12) // 3] += 1
                            if verbose and len(ex) < 40 and stem.startswith('farm'): ex.append('%s r%d %s %s: %s' % (os.path.basename(z)[:-8], r['r'], N[c[2]], b, why[:100]))
                        elif not any(cheb(c[3], c[4], h[0], h[1]) <= 4 for h in hits) and any(cheb(c[3], c[4], e[3], e[4]) <= 8 for e in foes):
                            contact[stem] += 1; contact_why[stem][re.sub(r'\d+', '#', why)[:90]] += 1
            prev = r
print('%d games, %d living champion-turns; rounds with a fight %d' % (games, turns, n_fight_rounds))
print('ally fights within %d tiles, we do not: %d turns (%.1f%% of turns); gap to the fighting ally 0-2: %d, 3-5: %d, 6-8: %d, 9-11: %d, 12: %d' % (
    RANGE, n_away, 100 * n_away / max(1, turns), gapd[0], gapd[1], gapd[2], gapd[3], gapd[4]))
for k, v in away.most_common(22):
    print('  %5d %4.1f%%  %-22s %s' % (v, 100 * v / n_away, k, ' || '.join('%s (%d)' % kv for kv in away_why[k].most_common(2))))
nc = sum(contact.values())
print('contact turns (enemy within 8, nobody hurt, no camp hit, no ally fighting near): %d (%.1f%% of turns)' % (nc, 100 * nc / max(1, turns)))
for k, v in contact.most_common(22):
    print('  %5d %4.1f%%  %-22s %s' % (v, 100 * v / nc, k, ' || '.join('%s (%d)' % kv for kv in contact_why[k].most_common(2))))
for e in ex: print('   ', e)
