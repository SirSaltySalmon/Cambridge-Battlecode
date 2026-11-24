"""Platform A/B test set (unranked groups against fixed opponents, maps and sides). Run from the repo root.

  python tools/hc/plat.py run NAME            queue every group of the plan for the bot that is live now -> results/plat/NAME.json
  python tools/hc/plat.py show NAME [BASE]    results so far: record, per class; with BASE, the paired cells that differ
  python tools/hc/plat.py wait NAME           poll until every match has finished (or 15 minutes)
  python tools/hc/plat.py test BOT NAME [V]   submit bots/BOT, run + wait, re-select version V (218), show vs base218
  python tools/hc/plat.py activate V          select version V (retries on the rate limit)

The plan (PLAN below) is 20 opponents x 5 maps. Unranked groups always use the team's selected version, so activate the
bot under test first (cambc submissions activate N) and put the old one back after `wait`.
"""
import json
import os
import subprocess
import sys
import time

CLUSTER = ['04gem_rush', '04atoll', '04gem_caverns', '00gem_fields', '04tidepools', 'adgato_gemcombat4',
           'adgato_gemcombat5', '00galaxy', '00quadrants', '00vault', 'adgato_gemcombat1', 'adgato_gemcombat2',
           'adgato_gemcombat3']
# Ordinary maps: our worst ladder maps first, then a spread of the rest.
OTHER = ['03contours', '02tangle', '00reverse_spiral', '03membrane', '04yin_yang', '04gap', '00the_line', '01big_world',
         '01hooks', '03shrines', '00breadcrumbs', '01yellow_brick_road', '00rushbait_v2', '04plaza', '04terraces',
         '02famine', '00road_of_riches', '03colosseum', '00ambush', '00monolith', '00corners', '04keyhole', '00split',
         '04hollows', '00expanse', '01islands', '04dunes', '03reef', '04bridges', '00eight', '02fields', '04wilds',
         '01fracture', '00great_wall', '04comb', '02cuts', '00lanes']
OPPONENTS = [
    ('DIET CODE', 'bee75a64-fae5-48aa-8751-73d4e78ebfb5'), ('Tzunami', '9a848453-f90f-4e97-988f-367378a79d1b'),
    ('Kingstonks', 'b50b3aa9-a0fb-4388-93d2-3459e1a350a9'), ('Oogway', '7a21fed9-4c5c-419c-8a31-ba3177b5edb0'),
    ('POFPOF', '3a198334-2725-4d97-99cd-d285932cdf80'), ('ClosedAI', 'b9b2f439-fc05-437f-907e-e8380921f9a4'),
    ('Stranger Danger', 'd61e374a-4f07-4bca-a3a8-fe7b77089dbd'), ('The Onkeys', 'aa795cbd-97f0-489d-868b-2988d7b2e394'),
    ('Live Love Larp', '292625c5-0f3a-49f0-a08a-9757b8f58658'), ('Serious-Wyso', '0c75a4f1-2e92-48e4-b1c1-384ddad80450'),
    ('DLLM', '4620d7df-a9ac-4747-aa77-dce12395dc0c'), ('Tubingen Quants', '91b991b0-c974-4ffe-a1f8-4c83c171e9f4'),
    ('Mouth Breathers', 'dd747c09-0574-4689-808a-1d475026a252'), ('Rhodes Island', '192c0f8e-3816-4c24-ade1-379c20f2843a'),
    ('TTS', '13ab0d63-ddfe-45c6-ac7a-22cf0bda68f7'), ('E=mc^2+AI^3', '21b63196-9d0e-436f-bf5d-25c55bdc4eaf'),
    ('test4', 'c00b682e-e45a-476b-96ca-470205e9dd44'), ('Trzy Kwaterki', '36258abf-3fd0-4827-8fa5-2a217438cf6f'),
    ('Nyx', '6f7fd199-4115-4fe0-8752-525bd8ccad8d'), ('conqueror_of_openai', '1570a113-57fd-4f15-84fb-0d764b12799b'),
]
ME = '7b219a14-b935-46b6-8c2d-ad2aaaf21f25'


def plan():
    maps = CLUSTER + OTHER  # 50 maps, each used twice
    groups = []
    for i, (name, tid) in enumerate(OPPONENTS):
        five = [maps[(i * 5 + k) % len(maps)] for k in range(5)]
        sides = ['a', 'b', 'a', 'b', 'a'] if i % 2 == 0 else ['b', 'a', 'b', 'a', 'b']
        groups.append(dict(opponent=name, team=tid, maps=five, sides=sides))
    return groups


def path(name):
    os.makedirs('results/plat', exist_ok=True)
    return 'results/plat/%s.json' % name


def run(name):
    out = json.load(open(path(name))) if os.path.exists(path(name)) else dict(groups=[])
    done = {g['team'] for g in out['groups']}
    for g in plan():
        if g['team'] in done:
            continue
        cmd = ['cambc', 'match-groups', 'create', g['team'], '--maps', ','.join(g['maps']), '--sides', ','.join(g['sides']), '--json']
        for attempt in range(8):
            r = subprocess.run(cmd, capture_output=True, text=True)
            if r.returncode == 0 and r.stdout.strip().startswith('{'):
                g['groupId'] = json.loads(r.stdout)['groupId']
                out['groups'].append(g)
                json.dump(out, open(path(name), 'w'), indent=1)
                break
            print('retry', g['opponent'], (r.stdout + r.stderr).strip()[:160])
            time.sleep(3 + 2 * attempt)
        else:
            print('FAILED', g['opponent'])
    print(len(out['groups']), 'groups queued')


def fetch(name):
    out = json.load(open(path(name)))
    cells = {}
    pending = 0
    for g in out['groups']:
        r = None
        for attempt in range(6):
            r = subprocess.run(['cambc', 'match-groups', 'show', g['groupId'], '--json'], capture_output=True)
            if r.returncode == 0:
                break
            time.sleep(2 + attempt)
        try:
            d = json.loads(r.stdout.decode('utf-8'))
        except Exception:
            pending += 5
            continue
        for m in d['matches']:
            me = [t for t in m['teams'] if t['teamId'] == ME][0]
            key = (g['opponent'], m['map'], me['side'])
            if m['status'] != 'finished':
                pending += 1
                continue
            cells[key] = dict(win=me['result'] == 'win', end=m['endReason'], id=m['matchId'], draft=me['draftPicks'])
    return cells, pending


def show(name, base=None):
    cells, pending = fetch(name)
    json.dump({'|'.join(k): v for k, v in cells.items()}, open(path(name + '-cells'), 'w'), indent=1)

    def rec(keys):
        w = sum(cells[k]['win'] for k in keys)
        return '%d-%d' % (w, len(keys) - w)
    cl = [k for k in cells if k[1] in CLUSTER]
    ot = [k for k in cells if k[1] not in CLUSTER]
    top = [k for k in cells if k[0] in ('test4', 'Trzy Kwaterki', 'Nyx', 'conqueror_of_openai')]
    print('%s: %s  (cluster maps %s, other maps %s; vs top-25 teams %s)  pending %d' % (name, rec(list(cells)), rec(cl), rec(ot), rec(top), pending))
    if base:
        b = {tuple(k.split('|')): v for k, v in json.load(open(path(base + '-cells'))).items()}
        both = [k for k in cells if k in b]
        gained = [k for k in both if cells[k]['win'] and not b[k]['win']]
        lost = [k for k in both if not cells[k]['win'] and b[k]['win']]
        print('paired cells %d: %s %d wins, %s %d wins; flipped to win %d, to loss %d' % (
            len(both), name, sum(cells[k]['win'] for k in both), base, sum(b[k]['win'] for k in both), len(gained), len(lost)))
        print('  won now:', ', '.join('%s@%s' % (k[1], k[0][:10]) for k in gained))
        print('  lost now:', ', '.join('%s@%s' % (k[1], k[0][:10]) for k in lost))


def wait(name):
    for _ in range(60):
        cells, pending = fetch(name)
        if pending == 0:
            break
        time.sleep(15)
    print('finished %d, pending %d' % (len(cells), pending))


def activate(version):
    for attempt in range(20):
        r = subprocess.run(['cambc', 'submissions', 'activate', str(version)], capture_output=True, text=True)
        if 'rror' not in (r.stdout + r.stderr):
            print('selected', version)
            return True
        time.sleep(4 + attempt)
    print('ACTIVATE FAILED', version)
    return False


def test(bot, name, restore):
    """Submit bots/BOT, run the set as NAME, wait, put RESTORE back, show against base218."""
    r = subprocess.run(['cambc', 'submit', 'bots/' + bot, '--note', '%s (unranked test %s)' % (bot, name)], capture_output=True, text=True)
    print((r.stdout + r.stderr).strip().splitlines()[-1])
    time.sleep(3)
    run(name)
    wait(name)
    activate(restore)
    show(name, 'base218')


if __name__ == '__main__':
    a = sys.argv[1:]
    if a[0] == 'run':
        run(a[1])
    elif a[0] == 'show':
        show(a[1], a[2] if len(a) > 2 else None)
    elif a[0] == 'wait':
        wait(a[1])
    elif a[0] == 'activate':
        activate(a[1])
    elif a[0] == 'test':
        test(a[1], a[2], a[3] if len(a) > 3 else '218')
