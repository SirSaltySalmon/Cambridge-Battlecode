# v81

`v80` + suggestion 2, `PING_RETREAT` (switch in `constants.h`). Not submitted.

## Rule
A champion whose own ping is still fresh, under `PING_RETREAT_HEALTH` = 75% health and outnumbered nearby (more
enemy champions within 5 tiles than allies within 3, itself included) stops fighting or farming and walks toward
help: the nearest visible ally, else the team's target, else our spawn. A melee still hits what is in reach on the
way. It ends when help arrives (no longer outnumbered), when the ping expires, or under 35% health (recall/dodge).
Runs right after the recall/dodge step (`retreat_to_allies()`, combat.cpp; branch `ping.retreat`).
Retreating at equal numbers too (1v1) was weaker: 37-31 vs v80, against 42-26 for the strict rule.

## Results (68 games = 34 normal + 34 unlimited fuel)

| Opponent | v81 | gems | v80 for comparison |
| --- | --- | --- | --- |
| v80 | **42-26** | **+12.0%** | - |
| v76 | **42-26** | +4.1% | 37-31 (+2.2%) |
| v74 | 35-33 | -2.4% | |
| brawler (always fights) | 33-35 | -6.2% | 31-37 (-9.0%); v76 31-37 |
| camper (rushbait_v2) | 2-0 | | |

vs v76: deaths -13%, gems lost on death +19%.

## Pings (vs v76, 68 games)
11.4 pings and 8.9 retreat turns per game. Pinger dead 5 rounds later: 40% (v80: 34%); enemy deaths per ping
0.044 (v80: 0.040). The gain does not show up as pinger survival in the 5-round log; it shows as fewer deaths
overall and more wins.

## Fuel (unlimited fuel, all 17 maps x2)
v81 mean 46.0k per turn, median 47.8k, p99 93.5k, 0.53% of turns over 100k (v80: 46.4k / 47.9k / 93.4k / 0.52%).
Retreat turns cost ~60k each but are rare (~700 of ~147,000 turns).
