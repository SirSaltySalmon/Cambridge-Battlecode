# v75

`v74` + `FIX_FIGHT_BODIES` (switch in `constants.h`), now **off**: over 68 games no setting beats v74. Not submitted.

## The problem (found by a teammate)
The fight/retreat score (`score_mode()`, route.cpp) added +3 per emblem, plus health, power and the body count, for
every ally the leader could see. A dead Ren was already left out (dead champions are not visible), and aggressive
mode already needs the whole team alive, but a recalling or tributing Ren still brought his emblems, health and
power into the score, and so did an ally 8+ tiles away behind a wall. The team then took fights with bodies that
could not join them.

## The fix
Only allies who can join the fight count: not channelling (recall, tribute, teleport) and within
`FIGHT_ALLY_RANGE` = 8 tiles of the enemy nearest the fight point (the target, or our own tile for a personal
rescore). Bodies, health, power and emblems all use that set.

## Results (34 games each, all 17 maps from both sides)

| Opponent | v74 | v75 (range 8) | range 12 | range 5 | channelling only |
| --- | --- | --- | --- | --- | --- |
| v74 | - | 19-15 | 19-15 | 14-20 | 16-18 |
| v71 | 16-18 | 20-14 | 17-17 | | |
| v70-stellar-ren | 23-11 | 25-9 | 22-12 | | |
| v64-bushward | 27-7 | 23-11 | 24-10 | | |
| v64 | 28-6 | 27-7 | 24-10 | | |
| Total vs those four | 94-40 | 95-39 | 87-49 | | |

v75 vs v74: gems -0.1%, gems lost -16%, deaths +3%. A small gain: the range is what matters (dropping only the
channelling allies lost 16-18; 5 tiles is too tight). An earlier test on v70 (allies within 5 tiles) was 18-16.

## Re-test over 68 games (34 normal fuel + 34 unlimited fuel) vs v74
The 19-15 above was fuel-timing luck: a build with identical logic but slightly more code went 15-19, and with
unlimited fuel both went 13-21. Each setting over 68 games:

| Setting | Normal | Unlimited | Total | Gems |
| --- | --- | --- | --- | --- |
| cutoff 6 | 7-27 | 15-19 | 22-46 | -17.0% |
| cutoff 7 | 17-17 | 12-22 | 29-39 | -9.7% |
| cutoff 8 | 15-19 | 13-21 | 28-40 | -7.7% |
| cutoff 9 | 20-14 | 12-22 | 32-36 | -15.8% |
| cutoff 10 | 19-15 | 17-17 | 36-32 | -1.8% |
| cutoff 12 | 16-18 | 16-18 | 32-36 | -7.7% |
| reach 6 (distance minus attack range) | 14-20 | 15-19 | 29-39 | -17.8% |
| reach 8 | 16-18 | 18-16 | 34-34 | 0.0% |
| soft (full within 4, none at 12) | 15-19 | 16-18 | 31-37 | -6.6% |
| channelling allies only | 15-19 | 16-18 | 31-37 | -7.5% |

The tighter the filter, the worse: counting fewer allies makes the team retreat more, and retreating costs lives
(the same lesson as the v70 fight-score tests). `FIGHT_WEIGHTING` (0 cutoff, 1 reach, 2 soft) stays in the code.
