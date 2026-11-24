"""python cmp218.py GAME[,GAME..] DIR... : per bot dir, same games: record, banked, deaths, answers, ping.retreat, pings,
contract flips, botFault, fuelExhausted, mean fuel per turn (team-X.fuel)."""
import collections, json, os, re, struct, sys, zipfile
LINE = re.compile(r'^\s*r(\d+) c(\d+) \((\d+),(\d+)\) (\S+): (.*?)(?: \| (.*))?$')
games = sys.argv[1].split(',')
def turn_fuel(raw):
    v = [struct.unpack_from('<Q', raw, 13 + 8 * i)[0] for i in range((len(raw) - 13) // 8)]
    rest = v[8:]
    s = rest.index(200000) if 200000 in rest else 0
    while s < len(rest) and rest[s] == 200000: s += 1
    return rest[s:]
for d in sys.argv[2:]:
    agg = collections.Counter(); fuel = []
    for g in games:
        side = g[-1]
        z = zipfile.ZipFile(os.path.join(d, g + '.repl26a'))
        st = json.load(open(os.path.join(d, g + '.json')))
        team = 'team' + side.upper()
        agg['games'] += 1
        agg['wins'] += st['match'].get('outcome') == team
        for t in st['teams']:
            agg['bank_us' if t['team'] == team else 'bank_them'] += int(t['submittedGems'])
        for e in st.get('events', []):
            ours = e.get('owner', {}).get('team') == team
            for en in e['entries']:
                if e['kind'] == 'death':
                    if ours: agg['camp deaths' if en.get('context') == 'camp' else 'deaths'] += 1
                    elif en.get('context') != 'camp': agg['kills'] += 1
                elif ours and e['kind'] == 'fuelExhausted': agg['out-of-fuel'] += 1
                elif ours and 'fault' in e['kind'].lower(): agg['botFault'] += 1
        f = turn_fuel(z.read(f'team-{side}.fuel')); fuel += f
        text = z.read(f'team-{side}.stdout').decode('utf-8', 'replace').replace(chr(0), ' ')
        for line in text.splitlines():
            m = LINE.match(line)
            if m:
                b = m.group(5)
                if 'answer' in b: agg['answers'] += 1
                if b.startswith('ping.retreat'): agg['ping.retreat'] += 1
                continue
            s = line.strip()
            if s.startswith('- ping s'): agg['pings'] += 1
            mm = re.match(r'- contract: (\w+) -> (\w+)', s)
            if mm:
                agg['transitions'] += 1
                if {mm.group(1), mm.group(2)} == {'engage', 'disengage'}: agg['e<->d'] += 1
            if s.startswith('- contract: keeps'): agg['held'] += 1
            if 'error' in s.lower() and ('bot' in s.lower() or 'exception' in s.lower()): agg['error lines'] += 1
    n = agg['games']
    print(f"== {d}: {agg['wins']}-{n - agg['wins']}, banked {agg['bank_us']} vs {agg['bank_them']}, K/D {agg['kills']}/{agg['deaths']}, camp deaths {agg['camp deaths']}")
    print(f"   answers {agg['answers']}, ping.retreat {agg['ping.retreat']}, pings {agg['pings']}, transitions/game {agg['transitions']/n:.1f}, e<->d/game {agg['e<->d']/n:.1f}, holds {agg['held']}")
    print(f"   fuel mean {sum(fuel)/max(1,len(fuel)):.0f} over {len(fuel)} turns, out-of-fuel turns {sum(1 for x in fuel if x >= 100000)} ({100*sum(1 for x in fuel if x >= 100000)/max(1,len(fuel)):.2f}%), fuelExhausted {agg['out-of-fuel']}, botFault {agg['botFault']}, error lines {agg['error lines']}")
