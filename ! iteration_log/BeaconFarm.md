# Beacon farm: one blob vs a peel

Both bots are the scoring billboard built on `vesper-salmon-greedyimp`. They share one `bot.py`. The only difference is `SPLIT_FARM`.

- `bots/vesper-salmon-beacon`: `SPLIT_FARM = False`. Everyone follows farm slot A.
- `bots/vesper-salmon-beacon-split`: `SPLIT_FARM = True`. On-the-way loot can peel to slot B.

Played with `cambc` 2.1.6 against `vesper-salmon-greedyimp` on the 9 pool maps, both sides (18 games each). Side A is the beacon bot. Gem margin is our tributed gems minus greedyimp's, summed across the 18 games. Replays are in `replays/bench/`. Per-game statistics are in `replays/bench/stats/`.

| vs greedyimp | Wins–losses | Gem margin |
| --- | --- | --- |
| Central farm (`SPLIT_FARM = False`) | **4–14** | **−10,290** |
| Split farm (`SPLIT_FARM = True`) | **2–16** | **−11,332** |

Central farm is the better of the two. Both lose to greedyimp. Split farming did not pay for itself: the only split wins were collision as side A and packs as side A, and both of those maps were already wins, or close, for the central bot.

## Central farm, game by game

| Map | Side | Result | Our gems | Their gems | Round |
| --- | --- | --- | --- | --- | --- |
| collision | A | loss | 0 | 220 | 2000 |
| collision | B | win | 512 | 454 | 2000 |
| corners | A | loss | 0 | 1022 | 500 |
| corners | B | loss | 120 | 1010 | 618 |
| elkring | A | win | 1044 | 522 | 900 |
| elkring | B | loss | 108 | 1080 | 512 |
| expanse | A | loss | 152 | 1142 | 838 |
| expanse | B | loss | 902 | 1138 | 882 |
| interlock | A | win | 1000 | 654 | 734 |
| interlock | B | loss | 212 | 1008 | 589 |
| lanes | A | loss | 24 | 1336 | 839 |
| lanes | B | loss | 0 | 1026 | 534 |
| monolith | A | loss | 512 | 1000 | 822 |
| monolith | B | loss | 446 | 1002 | 722 |
| packs | A | win | 1008 | 796 | 593 |
| packs | B | loss | 0 | 1006 | 501 |
| rows | A | loss | 0 | 1056 | 605 |
| rows | B | loss | 198 | 1056 | 1261 |

## Split farm, game by game

| Map | Side | Result | Our gems | Their gems | Round |
| --- | --- | --- | --- | --- | --- |
| collision | A | win | 410 | 88 | 2000 |
| collision | B | loss | 44 | 344 | 2000 |
| corners | A | loss | 184 | 1026 | 711 |
| corners | B | loss | 192 | 1036 | 635 |
| elkring | A | loss | 306 | 1098 | 640 |
| elkring | B | loss | 846 | 1062 | 550 |
| expanse | A | loss | 466 | 1032 | 830 |
| expanse | B | loss | 528 | 1048 | 792 |
| interlock | A | loss | 262 | 1024 | 819 |
| interlock | B | loss | 742 | 1232 | 909 |
| lanes | A | loss | 0 | 1068 | 735 |
| lanes | B | loss | 22 | 1028 | 799 |
| monolith | A | loss | 88 | 1008 | 680 |
| monolith | B | loss | 146 | 1008 | 715 |
| packs | A | win | 1090 | 570 | 597 |
| packs | B | loss | 0 | 1006 | 501 |
| rows | A | loss | 0 | 1056 | 605 |
| rows | B | loss | 198 | 1122 | 876 |

## Notes

Engine `botFaults` were 0 on every game. No fuel freeze showed up in the batch (36 games, 0 failed).

Stdout still logs caught vision errors (`Unit is not in vision`, `No visible entity of this kind occupies the tile`). Greedyimp logs the same class of error. They are worst on lanes (about 20–30 skipped turns a side). A skipped turn is not an engine crash.

The loss pattern is tributes, not crashes. Central farm banked 0 gems on collision A, corners A, lanes B, packs B, and rows A. Greedyimp reached 1,000 first on most maps. Weights were left at the first-cut values; this pass was the A/B of one blob versus peel-to-B.

# Crazier iteration?
Okay so we're gonna compute like everything. Like EVERYTHING. Enemies and their skills, dps, effective health and all as such will matter in our calculations, as well as time taken to clear camps, and calculating if we should split up for routes or not.