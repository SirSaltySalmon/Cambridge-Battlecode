# v149

v149 = v148-normal + EMBLEM_RATIO (user): the fight score's emblem term compares the two sides instead of counting ours.
Switches in `src/constants.h` next to EMBLEM_RELATIVE; the term is in `score_mode` (`src/route.cpp`).

## What changed
| | Emblem term |
|---|---|
| v148-normal | `3 x our fighters' emblems` (the enemy's are not counted) |
| v149 | `10 x ln((ours + 1) / (theirs + 1))`, both sides weighted like the bodies |

The idea: one emblem of difference matters less as both sides build. Examples: 1v0 +7, 4v2 +5, 10v5 +6, 10v9 +1,
0v3 -14, level 0. v148-normal gives level sides with 10 emblems each +30.

## Result (all 70 maps both sides, release, seed 0, 140 games): rejected
| | v149 | v148-normal | |
|---|---|---|---|
| Record | 50-86-4 | | |
| Banked gems | 80,465 | 105,067 | -23.4% |
| Gems collected | 107,669 | 130,298 | -17.4% |
| Gold earned | 827,943 | 1,026,517 | -19.3% |
| Kills / deaths | 1682 / 2512 | 2414 / 1768 | |
| Camp deaths | 98 | 86 | |
| Fuel per turn (mean) | 33,795 | 35,420 | -4.6% |
| Out-of-fuel turns | 2328 | 2135 | |

Not close, so the extra fuel-burn seeds were not run. Same failure as EMBLEM_RELATIVE in v115 (34-46): with the flat term
gone, even fights score near the bar, the team breaks off more and gets run down (deaths +42%, kills -30%).
Replays: `replays/b149ratio/v149`; summary in `results/results-v149-vs-v148-normal.txt`.
