# v81

`v80-5` + mage safety (Cassi and Stellar; switches in `constants.h`). Not submitted.

Why: mage basic attacks are weak (30-35) and every camp hits at 3, the same as a ranged basic attack, so each basic
attack on an untanked camp cost health. Their spells reach 4: one tile past every camp, and past an enemy melee's
walk-and-hit.

- `MAGE_STANDOFF`: on a camp that hits (or a cluster grouped with one), a mage casts from 4, outside camp reach, and
  basic attacks only when a sturdy ally tanks it (`camp_tanked`, FIX_ABSORB's rule) or the finish is cheap: rounds to
  kill with basic attacks (ours + allies in reach) x the strength of every camp in the group <= `MAGE_FINISH_SHARE`
  (10%) of max health. Mages no longer wait for a melee in sight: their spells open the camp.
- `MAGE_LEAVE`: inside an awake camp's reach, untanked and not finishing: step out to the cast ring.
- `MAGE_KITE`: in a fight a ready spell is cast from wherever the mage stands; it walks toward the fight only outside
  enemy basic-attack reach (a melee's walk-and-hit, 2; a ranged shot, 3), steps back out of it while spells cool, and
  basic attacks from outside it. Enemy spells reach 4 whether we stand at 3 or 4, so they do not move us. One threat
  board and at most one path search a turn.
- `MAGE_MONOLITH`: melees never hit a monolith or walk into an awake one's reach; mages take it with spells from 4
  (a ward into its bush, a ring tile with line of sight when walls hide it). Off the shared route; a mage takes one
  near the team's target, or near itself while that target respawns, and resumes one it worked on within 40 rounds.
- `MONOLITH_TEAM_STARVED` (on): on a gem-starved map (contest) the whole team still takes the monolith, with v80-5's
  gates. Two mages' spells alone do ~36 a round: contest's monolith went 20,000 -> 8,760 in 850 rounds, never died,
  and mage-only scored 0-0 there vs 800-0.
- `MONOLITH_REACH` is 3 (SDK), not the handbook's 5.

Logging: mage camp turns append to the branch (`farm.camp.standoff`, `.cooldown`, `.closing`, `.leave`, `.finish`,
`.tanked`), fights to `fight.hit.cast|step|basic|hold|back`; the mage job logs `mono.walk` / `mono.camp.*`, melees
`farm.guard`. Notes give camp health, finish cost, and why a tile is safe.

## Fuel
Profiled per turn phase (24 games): mage fight turns ran out of fuel no more often than any fight turn (0.72% vs 0.76%).
Expensive turns went to the shared decision (~29k), `score_mode` (~22k, run up to twice a turn) and the decision log
(~9k on every turn, ~21% of a typical turn).
- The decision log runs inside `debug_block`: free in local debug builds, skipped in release (the platform).
- `score_mode` works out its origin-free terms (bodies, health, power, emblems, overextension, spawn, kill window) and
  their reason text once per round and vision list; only distance and ping are per call. Unlimited-fuel games and
  logs are byte-identical with and without it (eight, lanes).

## Results (34 games, normal fuel, all 17 maps x2, `analyze_bench.py`)

| | record vs v80-5 | gems | fuel-outs / 100 rounds | mage camp dmg taken | mage deaths | mage champ dmg done / taken |
| --- | --- | --- | --- | --- | --- | --- |
| v80-5 mirror | 17-17 | 0% | 3.12 | 1,917 | 6.62 | 9,099 / 5,717 |
| v81 first draft | 18-16 | +0.7% | 3.86 | 974 | 5.94 | 6,558 / 4,339 |
| + log in debug_block | 16-18 | -6.4% | 0.83 | 930 | 6.71 | 7,774 / 4,900 |
| + score_mode cache | 18-16 | +11.8% | 0.85 | 916 | 6.24 | 8,818 / 4,568 |
| + simple mage fight (v81) | 20-14 | +17.8% | 0.78 | 769 | 6.35 | 9,270 / 5,361 |

Per game, except the fuel column. The two middle rows play identically with unlimited fuel, so their 2-game / 18%
gap is the noise level of one game per map side.
- Running out of fuel: -75% vs v80-5.
- Mages: 60% less camp damage than v80-5, champion damage back above v80-5's, 6% less champion damage taken.
- Contest (side A): 1,000-0 at round 1,686 with one death on our team (v80-5 mirror: 600-0 at 2,000).
- Monolith captures 14 (v80-5 mirror 6).
- Mage-only monoliths everywhere (`MONOLITH_TEAM_STARVED` off) scored 0-0 on contest-a in an earlier run.
