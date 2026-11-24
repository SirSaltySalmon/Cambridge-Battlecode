# v235 (v231 + RALLY_HIT)

Parent: bots/v231 (v230 + RALLY + WILLOW_SHOOT; its README has the ladder study and the test method). Built by
`tools/hc/p235.py` on a copy of v231. Not live: platform version 230 (bots/v230) is selected.

## Switch (the "v231" block at the top of src/constants.h)

| Switch | Default | What it does |
| --- | --- | --- |
| `RALLY_HIT` | true | The rally ping also goes out from a champion about to hit an enemy champion (target in reach, a melee one step more), under the same numbers test as RALLY (enemies within 6 at least as many as allies within 4). v231 only called after losing 60+ health in a round, so a fight we opened, or one fought from range, called nobody. Someone else's fresh ping stands; our own is refreshed while the fight goes on. |

## Why

In v231's logs the champions of a team in fight mode that see no enemy still farm on ("fight mode, but no enemy to
fight: go on", 199 turns a game) because nothing says where the fight is unless someone took a hit of 60 or more with
the numbers against us. The ping byte is the only shared place a fight has (bots/v231/README.md).

## Benches

Local, 25 maps of test set 2, both sides, debug with logs, against bots/live218 (`tools/hc/mini.py`):

| Bot | Record | Gems | K/D | Fuel mean / p95 / out-of-fuel |
| --- | --- | --- | --- | --- |
| v230 | 22-28 | -8.5% | 466/420 | 39,480 / 70,483 / 0.30% |
| v231 (parent) | 27-23 | +5.7% | 495/396 | 39,914 / 70,912 / 0.30% |
| v235 | 29-21 | +18.7% | 457/342 | 40,237 / 71,328 / 0.32% |

Per game in the logs (v231 in brackets): pings 33 (26), 16 of them from a hit; answer turns 48 (32); pings that ended
with a kill 4.3 (2.0), with the pinger dead 4.0 (4.3); claims given back to answer 7.6 (6.2); outnumbered fall-back
turns 13.9 (15.4). Deaths a game: Heart of Ruin 1.72 (2.04), Ryth 1.63 (1.65), Willow 1.33 (1.59), Marcella 1.35
(2.45), Vesper 1.04 (1.13). Bot errors 0.3 a game (0.3).

One 50-game bench swings about ±5 wins. The death counts and the ping outcomes move the same way as the record.

Platform (platform version 233), test set 2 with opponents pinned to the control runs, sets A and B (bots/v231/README.md):

| Run | Record | Near teams | Top five | Near: kills / deaths | Near: margin change on the control |
| --- | --- | --- | --- | --- | --- |
| control (v230), both sets | 24-76 | 22-28 | 2-48 | 8.7 / 4.9 | |
| v231, both sets | 25-75 | 24-26 | 1-49 | 10.8 / 5.6 | better in 27, worse in 21 |
| p235, set A | 11-39 | 11-14 | 0-25 | 10.4 / 5.3 | mean +9 gems, median -10; better in 11, worse in 13 |
| p235b, set B | 14-36 | 13-12 | 1-24 | 9.1 / 4.8 | mean +54, median +25; better in 15, worse in 8 |
| v235, both sets | 25-75 | 24-26 | 1-49 | 9.8 / 5.1 | better in 26, worse in 21 |

Verdict: the same platform record as v231 and one win of 100 over the control: level. RALLY_HIT is the best local
build (deaths -14% on v231) and costs 0.8% fuel, so v235 is the folder to build on; none of the three fight switches
moves the ladder. What does is in "Where the gap is" of bots/v231/README.md.
