"""Per-champion totals for the bot under test in a bench folder (files <map>-a.json: the bot is team A; -b: team B).
usage: python tools/hc/champs.py DIR [DIR...]   -> per kind: games, deaths, kills, champion damage done / taken, healing"""
import collections, glob, json, os, sys
for d in sys.argv[1:]:
    acc = collections.defaultdict(lambda: collections.Counter()); games = 0
    for f in sorted(glob.glob(os.path.join(d, '*.json'))):
        j = json.load(open(f, encoding='utf-8')); us = 'teamA' if f.endswith('-a.json') else 'teamB'; games += 1
        for c in j['champions']:
            if c['team'] != us: continue
            m = c['metrics']; a = acc[c['name']]
            a['n'] += 1; a['d'] += int(m['deaths']); a['k'] += int(m['kills'])
            a['dd'] += float(m['championDamageDone']); a['dt'] += float(m['championDamageTaken']); a['h'] += float(m['healingDone'])
            a['md'] += float(m['monsterDamageDone'])
    print('%s (%d games)' % (d, games))
    for k, a in sorted(acc.items(), key=lambda kv: -kv[1]['n']):
        n = a['n']
        print('  %-14s games %3d  deaths %.2f  kills %.2f  champion damage done %5.0f taken %5.0f  healing %5.0f  camp damage %6.0f' % (
            k, n, a['d'] / n, a['k'] / n, a['dd'] / n, a['dt'] / n, a['h'] / n, a['md'] / n))
