# v203 (v202 + multiclear)

v203 = bots/v202 + multiclear: BUSY_JOBS side jobs (`src/jobs.cpp`; switches and notes at the v202 (BUSY_JOBS)
block of `src/constants.h`). A champion takes a camp group of its own instead of walking to, or waiting at, the team
target while that respawns. Locally it lost on v170 (Fish-style splitting, 158-186, -7.0%). Platform v211.

Ladder test 2026-11-03 (unranked, 25 games each against ranks 34-60; same maps, sides and frozen opponents;
medians per game; `results/fish-farming-study.txt`):

| | v209 (live entry) | v202 (platform v210) | v203 (platform v211) |
|---|---|---|---|
| Record | 2-23 | 4-21 | 3-22 |
| Banked | 9466 | 9805 | 9285 |
| Kills / deaths | 146 / 233 | 125 / 332 | 146 / 373 |
| Camp gems by r600 | 192 | 221 | 245 |

Multiclear farms more but adds deaths, as it did locally; not adopted. v209 was restored as the live and tournament
entry.
