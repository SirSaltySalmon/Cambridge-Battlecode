# v78

`v77` + four ping refinements (switches in `constants.h`). Not submitted.

- `PING_SCORE`: a fresh ping within `PING_RANGE` of the leader adds `PING_SCORE_BONUS` = +5 to the fight score
  instead of forcing fight mode (needs visible enemies, like any fight).
- `PING_ANSWER_LIMIT`: only champions within `PING_ARRIVE` = 8 tiles answer (in farm mode too), never a carrier,
  one under 50% health or the pinger.
- `PING_FILTER`: no ping with an ally within `PING_ALONE` = 3 tiles or while the team is in fight mode.
- `PING_LOG`: the pinger logs "ping result" 5 rounds later (died, or health then/now and enemy deaths seen);
  answerers log "ping answered" once per ping. Fuel: 47.83k vs 47.82k per turn with it off (10 heavy-map games).

## Results (68 games = 34 normal + 34 unlimited fuel)

| Opponent | Normal | Unlimited | Total | Gems |
| --- | --- | --- | --- | --- |
| v77 | 15-19 | 16-18 | 31-37 | -0.8% |
| v76 | 18-16 | 15-19 | 33-35 | -6.5% |
| brawler | 14-20 | 15-19 | 29-39 | -11.0% (v77 27-41, v76 31-37) |

Out-of-fuel turns +19% vs v76 at normal fuel (more fight turns), 0 crashes.

## What the outcome log shows (136 games)
Per game: 13.6 pings (v77: 28), 17.5 leader turns with the ping bonus, 0.3 answer turns (v77: ~49). Of 1,363 pings,
538 pingers (39%) were dead 5 rounds later, and only 59 enemy deaths followed (4%). A lone champion that pings is
usually losing a duel nobody can reach in time: help rarely arrives, so the hurt champion moving toward help
(suggestion 2) is the more promising lever than answering.
