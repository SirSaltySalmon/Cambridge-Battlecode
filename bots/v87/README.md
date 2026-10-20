# v87

`v86` + two changes, both in `src/constants.h`. Submitted as platform version 87.

## Why
On the ladder v86 matched v82's record (33-47 on the same 16 groups). It was ahead early and died less, but the
opponents farmed more against it (906 camp gems a game against 826 vs v82): v86 left their side alone unless a raid
fired, and raids almost never fired (1 in 34 local games; "more of us together than they have alive" rarely holds 6v6).

## What changed
- **Raids on the man advantage** (`RAID_BY_ALIVE`, `RAID_MIN_BODIES` = 3, `RAID_BY_RATE`): within 15 rounds of a kill,
  with more of us alive than them and 3+ healthy (60%+) champions together, the team takes one unsafe group when it pays
  better per round (walk and wait included) than the normal target, and it still has to be reached, cleared and
  captured 5 rounds before the dead enemy respawns. Raids: 17 per 34 local games (v86: 1).
- **No kill-window penalty** (`KILL_WINDOW` = false): v82's -20 to the fight score once 2+ enemies are dead. Kills are
  what open the raids now. (Mike's dead-enemy term, `DEAD_ENEMY_SCORE`, stays 0: +5 per dead enemy went 37-31 and -5
  44-24 vs v82, but -5 lost 16-18 to v86 and 79-57 overall against 82-54 for the kill window alone.)

## Results (34 games each)

| Bot | vs v82 | vs v82 (unlimited) | vs brawler | vs v80-5 | vs v86 (unlimited) | Total |
| --- | --- | --- | --- | --- | --- | --- |
| v87 | 18-16 (+9.7%) | 23-11 (+23.2%) | 18-16 (+3.7%) | 21-13 (+16.1%) | 19-15 (+8.9%) | 80-56 (4 sets) |
| v86 | 18-16 (+7.5%) | 21-13 (+16.8%) | 17-17 (-2.9%) | 20-14 (+19.3%) | | 76-60 |
| v82 | | | 15-19 | 21-13 | | |

## Ladder (unranked: the same 16 groups, team + 5 maps, as v82 and v86; `results/results-v87-ladder.txt`)

| | record | tributed / game | camp gems by r200 / r400 / total | opp camp gems | kills / deaths |
| --- | --- | --- | --- | --- | --- |
| v87 | 36-44 | 770 vs 819 | 236 / 471 / 825 | 860 | 9.4 / 7.0 |
| v86 | 33-47 | 745 vs 868 | 231 / 453 / 807 | 906 | 8.8 / 7.8 |
| v82 | 33-47 | 711 vs 816 | 201 / 421 / 799 | 826 | 10.7 / 8.8 |

+3 wins over v86 and v82 (inside the noise of 80 games, about +-4.5), with the opponents' camp gems down 46 a game.
Monolith is still 0-5 for all three; the two teams make the same number of captures a game (27 vs 26), but theirs
are worth more (32.8 gems against 30.6).
