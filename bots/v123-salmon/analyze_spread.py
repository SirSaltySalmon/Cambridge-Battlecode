"""How spread out our fights are, from debug replays (needs the decision logs).

usage: python analyze_spread.py DIR [DIR...]   (team A of <map>-a and team B of <map>-b is the bot under test;
       pass --both to count both teams of every replay, labelled by side)

Per team: deaths to champions, how many were lone (no living ally within LONE tiles the round before), the nearest
ally's distance at those deaths, closing turns (fight.hit ... closing), lone closing turns, and kills.
"""
import collections
import glob
import json
import os
import re
import sys
import zipfile

LINE = re.compile(r'^\s*r(\d+) c(\d+) \((\d+),(\d+)\) (\S+): (.*?)(?: \| (.*))?$')
LONE = 4


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def positions(text):
    # round -> {champ: (x, y, branch, why)}
    by = collections.defaultdict(dict)
    for line in text.splitlines():
        m = LINE.match(line)
        if m:
            by[int(m.group(1))][int(m.group(2))] = (int(m.group(3)), int(m.group(4)), m.group(5), m.group(6))
    return by


def analyse(repl, team):
    z = zipfile.ZipFile(repl)
    text = z.read(f'team-{team}.stdout').decode('utf-8', 'replace').replace(chr(0), ' ')
    pos = positions(text)
    stats = json.load(open(repl[:-len('.repl26a')] + '.json'))
    me = 'teamA' if team == 'a' else 'teamB'
    c = collections.Counter()
    gaps = []
    for group in stats.get('events', []):
        if group.get('kind') != 'death':
            continue
        # A death event's owner is the champion that died; the entry's target is the killer.
        for e in group['entries']:
            if e.get('context') != 'champion':
                continue
            if group['owner'].get('team') != me:
                c['kills'] += 1
                continue
            r, spot = e['timestamp']['round'], (e['x'], e['y'])
            before = pos.get(r - 1) or pos.get(r - 2) or {}
            alive = {k: (v[0], v[1]) for k, v in before.items()}
            if not alive:
                continue
            dead = min(alive, key=lambda k: cheb(alive[k], spot))
            others = [cheb(alive[dead], p) for k, p in alive.items() if k != dead]
            c['deaths'] += 1
            near = min(others) if others else 99
            gaps.append(near)
            if near > LONE:
                c['lone'] += 1
    for r, champs in pos.items():
        for k, (x, y, branch, why) in champs.items():
            if branch.startswith('fight.hit') and 'closing' in why:
                c['closing'] += 1
                others = [cheb((x, y), (v[0], v[1])) for j, v in champs.items() if j != k]
                if not others or min(others) > LONE:
                    c['lone_closing'] += 1
            if branch.startswith('fight.join'):
                c['join'] += 1
        c['turns'] += len(champs)
    drops = text.count('team chase dropped')
    c['chase_drops'] = drops
    return c, gaps


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    both = '--both' in sys.argv
    total = collections.defaultdict(collections.Counter)
    gaps_all = collections.defaultdict(list)
    for d in args:
        for repl in sorted(glob.glob(os.path.join(d, '*.repl26a'))):
            side = repl[-len('a.repl26a')]
            teams = [('test', side)] if not both else [('a', 'a'), ('b', 'b')]
            for label, team in teams:
                try:
                    c, g = analyse(repl, team)
                except (KeyError, FileNotFoundError, zipfile.BadZipFile):
                    continue
                total[label] += c
                gaps_all[label] += g
    for label, c in total.items():
        d = max(1, c['deaths'])
        g = sorted(gaps_all[label])
        med = g[len(g) // 2] if g else 0
        print(f"{label}: deaths {c['deaths']}, lone {c['lone']} ({100 * c['lone'] / d:.0f}%), median ally gap {med}, "
              f"kills {c['kills']}, closing {c['closing']} ({100 * c['closing'] / max(1, c['turns']):.1f}% of turns), "
              f"lone closing {c['lone_closing']} ({100 * c['lone_closing'] / max(1, c['closing']):.0f}%), "
              f"join {c['join']}, chase drops {c['chase_drops']}")


if __name__ == '__main__':
    main()
