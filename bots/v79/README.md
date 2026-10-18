# v79

`v78` + the ping rally and the user's carrier settings. Not submitted.

## Changes
- Carrier rules (`FIX_CARRIER`): `CARRY_GEMS` 20 -> 60, `CARRY_RECALL_HEALTH` 0.5 -> 0.35 (same as everyone).
- `PING_RALLY`: the leader points the team's target at a fresh ping when `RALLY_HELPERS` = 2 visible allies could
  answer it; never during an escort or into the enemy chase ring. Farming champions then walk there.
- `PING_REFRESH`: the pinger re-sends its ping every turn while still under attack (outcome log stays on the first).
- `PING_COLLECTOR`: a collector who can answer gives its claim back at once and goes (the fight beats the reward).
- Who can answer (`can_answer()`): health >= `PING_ANSWER_HEALTH` = 60%, within `PING_ARRIVE` = 12 tiles, bag under
  `CARRY_GEMS` (a built Ren: under `REN_ANSWER_GEMS` = 100), not channelling, not the pinger. Farm-mode answering
  now runs right after the fight step, before any banking.

## Results (68 games = 34 normal + 34 unlimited fuel)

| Opponent | Normal | Unlimited | Total | Gems |
| --- | --- | --- | --- | --- |
| v78 | 18-16 | 19-15 | 37-31 | -2.7% |
| v76 | 16-18 | 18-16 | 34-34 | -6.7% |
| brawler | 14-20 | 13-21 | 27-41 | -15.6% (v76 31-37, v77 27-41, v78 29-39) |

Out-of-fuel turns +29% vs v76 at normal fuel, 0 crashes.

## Did help arrive? (vs v76, 68 games; the 5 rounds after each ping)

| | v78 | v79 |
| --- | --- | --- |
| Pings per game | 12.6 | 11.3 |
| Team in fight mode | 22% | 26% |
| Someone came (answer, rally, collector) | 12% | 37% |
| Pinger dead 5 rounds later | 39% | 34% |
| Enemy deaths per ping | 0.055 | 0.055 |

Help now arrives three times as often and a few more pingers survive, but no more enemies die and wins and gems
do not improve; against the always-fighting brawler it is worse. The carrier settings changed at the same time,
so part of the gem drop may come from them.
