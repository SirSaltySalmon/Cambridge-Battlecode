"""Platform test set 2 (2026-11-11): 5 teams just above us and the top 5, 5 maps each. Run from the repo root.

  python tools/hc/plat2.py run NAME [PIN]     queue the 10 groups for the bot that is live now -> results/plat/NAME.json
                                              PIN = an earlier run's name: each opponent plays the bot version it used there
  python tools/hc/plat2.py show NAME [BASE]   record, by class (near / top); with BASE, the paired cells that differ
  python tools/hc/plat2.py wait NAME          poll until every match has finished (or 20 minutes)
  python tools/hc/plat2.py test BOT NAME PIN RESTORE   submit bots/BOT, run pinned to PIN, wait, re-select RESTORE, show vs PIN
  Add --b to any command to use map set B (25 other maps, same opponents); keep A and B runs under different names.

The 25 maps are every fifth map of the 128-map pool (the ladder draws from the whole pool); each near team and each top
team plays the same five, on opposite sides. Unranked groups use our selected version, so the bot under test must be
live while its groups are queued and built.
"""
import json
import subprocess
import sys
import time

import plat

NEAR = [  # ladder ranks 131, 127, 123, 119, 115 on 2026-11-11 (we were 133, rating 1934)
    ('Still Testing', '67f45d6d-9cd8-403d-ade2-14aad9202201'), ('The Mania', '2728f025-48ee-4adb-bd24-d7d26878a47e'),
    ('Sauron', 'a4431057-ba98-473a-8f7c-20b41b603d6b'), ('Hesitation is Defeat', '81857de3-44fc-4a81-ae7c-f0cecf522d11'),
    ("jeanne d'arc", '953e5c18-ec79-4305-bdb9-b6d27060ea51'),
]
TOP = [  # ladder ranks 1-5
    ('stellar best girl', '547efb5b-57e5-4355-8501-3b331f506d62'), ('Mite', 'ee83f5e5-bcad-4923-86b4-b79ea6083d2e'),
    ('Intercontinental Fish', '1890540d-1ffc-4847-adbf-d0d08772f268'), ('BattleCats', '285f4eb5-e97f-4ea4-93c5-19e1590a1e5a'),
    ('Ad Astra', 'de0b1e3c-39fb-473c-8244-61c1133ad612'),
]
MAPS = ['00breadcrumbs', '00crater', '00expanse', '00gem_fields', '00interlock',
        '00monolith', '00pilgrimage', '00ribs', '00rushbait_v2', '00swirl',
        '01big_world', '01corridors', '01gold_rush', '01starfish', '01yellow_brick_road',
        '02famine', '02gift_wrapping', '02tangle', '02uncharted_lands', '03contours',
        '03membrane', '03riverbed', '04atoll', '04bullseye', '04crossroads']
# Set B: the same opponents on 25 other maps (every fourth of the 103 maps not in set A), to double a sample.
MAPS_B = ['00anvil', '00dunes', '00galaxy', '00lanes', '00quadrants', '00rows', '00vault', '01elk_arena', '01shortcut', '02crossing', '02guardians', '02the_pond', '03fan', '03reef', '03twisty', '04braided_river', '04comb', '04dunes', '04gem_rush', '04keyhole', '04mountain_pass', '04plaza', '04standing_stones', '04two_kingdoms', '04yin_yang']
TOP_NAMES = {n for n, _ in TOP}


def plan():
    groups = []
    for cls, teams in (('near', NEAR), ('top', TOP)):
        for i, (name, tid) in enumerate(teams):
            first = 'a' if (i + (cls == 'top')) % 2 == 0 else 'b'
            other = 'b' if first == 'a' else 'a'
            groups.append(dict(opponent=name, team=tid, cls=cls, maps=MAPS[i * 5:i * 5 + 5],
                               sides=[first, other, first, other, first]))
    return groups


def run(name, pin=None):
    import os
    p = plat.path(name)
    out = json.load(open(p)) if os.path.exists(p) else dict(groups=[])
    done = {g['team'] for g in out['groups']}
    pinned = {g['team']: g['groupId'] for g in json.load(open(plat.path(pin)))['groups']} if pin else {}
    for g in plan():
        if g['team'] in done:
            continue
        cmd = ['cambc', 'match-groups', 'create', g['team'], '--maps', ','.join(g['maps']), '--sides', ','.join(g['sides']), '--json']
        if g['team'] in pinned:
            cmd += ['--source-group', pinned[g['team']]]
            g['source'] = pinned[g['team']]
        for attempt in range(8):
            r = subprocess.run(cmd, capture_output=True, text=True)
            if r.returncode == 0 and r.stdout.strip().startswith('{'):
                g['groupId'] = json.loads(r.stdout)['groupId']
                out['groups'].append(g)
                json.dump(out, open(p, 'w'), indent=1)
                break
            print('retry', g['opponent'], (r.stdout + r.stderr).strip()[:200])
            time.sleep(3 + 2 * attempt)
        else:
            print('FAILED', g['opponent'])
    print(len(out['groups']), 'groups queued')


def show(name, base=None):
    cells, pending = plat.fetch(name)
    json.dump({'|'.join(k): v for k, v in cells.items()}, open(plat.path(name + '-cells'), 'w'), indent=1)

    def rec(keys):
        w = sum(cells[k]['win'] for k in keys)
        return '%d-%d' % (w, len(keys) - w)
    top = [k for k in cells if k[0] in TOP_NAMES]
    near = [k for k in cells if k[0] not in TOP_NAMES]
    print('%s: %s  (near %s, top %s)  pending %d' % (name, rec(list(cells)), rec(near), rec(top), pending))
    for opp in [n for n, _ in NEAR + TOP]:
        ks = sorted(k for k in cells if k[0] == opp)
        print('  %-22s %s  %s' % (opp, rec(ks), ' '.join('%s%s' % ('W' if cells[k]['win'] else 'L', '' if cells[k]['end'] in ('gems', 'tribute', 'GemsTributed') else '') for k in ks)))
    if base:
        b = {tuple(k.split('|')): v for k, v in json.load(open(plat.path(base + '-cells'))).items()}
        both = [k for k in cells if k in b]
        gained = [k for k in both if cells[k]['win'] and not b[k]['win']]
        lost = [k for k in both if not cells[k]['win'] and b[k]['win']]
        print('paired cells %d: %s %d wins, %s %d wins; flipped to win %d, to loss %d' % (
            len(both), name, sum(cells[k]['win'] for k in both), base, sum(b[k]['win'] for k in both), len(gained), len(lost)))
        print('  won now:', ', '.join('%s@%s' % (k[1], k[0][:10]) for k in gained))
        print('  lost now:', ', '.join('%s@%s' % (k[1], k[0][:10]) for k in lost))


def wait(name):
    for _ in range(80):
        cells, pending = plat.fetch(name)
        if pending == 0:
            break
        time.sleep(15)
    print('finished %d, pending %d' % (len(cells), pending))


def test(bot, name, pin, restore):
    r = subprocess.run(['cambc', 'submit', 'bots/' + bot, '--note', '%s (unranked test %s)' % (bot, name)], capture_output=True, text=True)
    print((r.stdout + r.stderr).strip().splitlines()[-1])
    time.sleep(3)
    run(name, pin)
    wait(name)
    plat.activate(restore)
    show(name, pin)


if __name__ == '__main__':
    a = sys.argv[1:]
    if '--b' in a:
        a.remove('--b')
        MAPS = MAPS_B
    if a[0] == 'run':
        run(a[1], a[2] if len(a) > 2 else None)
    elif a[0] == 'show':
        show(a[1], a[2] if len(a) > 2 else None)
    elif a[0] == 'wait':
        wait(a[1])
    elif a[0] == 'test':
        test(a[1], a[2], a[3], a[4])
