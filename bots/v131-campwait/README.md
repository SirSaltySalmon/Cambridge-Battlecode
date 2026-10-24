# v131-campwait

`v130` + `CAMP_WAIT`: after stepping out of awake camps, a champion does not walk back into them until each camp sleeps
again or turns on an enemy. Not submitted.

## Why
v130 logs (80 games): 3,626 times a champion left an awake camp's reach and was back inside within 10 rounds, e.g.
`farm.walk` -> `camp.exit` -> `farm.walk` -> `camp.exit`: the walk to its farm target crossed the awake camp, it stepped
out, walked straight back in, was hit, stepped out again. After a fight or retreat exit it walked back into the awake
camps of its own farm target.

## Change (`combat.cpp`)
- `remember_hitting_camps` (called by `camp_exit`, i.e. every camp exit: camp, fight, retreat, hurt, ping): the awake camps
  hitting us (nearest champion in their reach) are remembered with their spawn tile and reach.
- `camp_wait_avoid` (called at the top of `farm()`, so farm walks, farm camps and the team retreat's farming): the reach of
  each remembered camp is added to the walk's avoid zone while the camp is still awake and no enemy champion stands inside
  its reach (then it hits them, not us at its edge). A camp drops out once seen asleep, or after `CAMP_WAIT_ROUNDS` (20).
  The group we farm is exempt unless an enemy champion is near (`CAMP_WAIT_TARGET_FIGHT`).

## Results (vs v130)
| | camp wait (first try: target always exempt) | camp wait (= v131-campwait) | v130 |
|---|---|---|---|
| re-entries into a camp just left, within 10 rounds (per game) | 36.8 | **31.7** | 43.8-45.8 |
| of which via farming / retreat / fight | 11.8 / 5.8 / 10.4 | **10.4 / 3.7 / 9.0** | 20.2-20.6 / 5.2-6.1 / 10.9 |
| camp-exit turns per game | 103.9 | 96.5 | 108.5-113.3 |
| 160 games (release, 40 maps, both sides, normal + unlimited) | 79-81, -2.4% | **86-74, +5.0%** | |
| gems collected (normal, unlimited) | -1.2%, +2.7% | **+6.0%, +5.0%** | |
| kills / deaths (normal, unlimited) | 0% / +1%, -2% / +2% | +1% / +1%, +7% / -6% | |
Debug team fights (one 80-game run each): won 5% / lost 8% (v130 6% / 5%); champions fighting 1.89 vs 1.91 per round.
