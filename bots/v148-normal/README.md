# v148-normal

v148-normal = v145 + team farming swaps (user) + the measured camp tie rule. Switches are in `src/constants.h` under
"v148-normal switches".

## The problem (v148-145 vs v145 debug replay, v145's side)
v145's tank swap treated "leave the camp's reach" as the only way to lose aggro:
- r62-77 s0 (two elks): HoR tanked to 430, then "too low to walk out" walked out for 6 rounds under both elks and
  died at r77. Vesper stood 3 off the whole time.
- r74-84: Vesper dashed in adjacent to an elk (ping answer) and tanked it from 1000 to 220 while Ryth stood one tile
  further; Ryth only stepped in once Vesper was under the 35% bar.
- r206-226 s8 (two elks): HoR, Ryth and Marcella each tanked alone in turn and walked out. Ryth walked out at
  480/1200 although Marcella stood level beside him and the elks had already moved to her (the tie counted as his
  hit; SWAP_HIT refused a tile one step behind her, since a melee cannot hit from there). The group never died.

## How camps pick a target (measured)
Engine replay stepped per unit turn (scratchpad `vis/steps.mjs` + `ties3.py`, rebuild from the replay-decoding memory):
camps act in a block at round start (step offsets 4-14), so each camp hit is its own step and is attributed to the one
camp that could make it. 25 games:
- the nearest champion by Chebyshev distance is hit: 8909 of 8949 hits;
- on a tie between allies, the one that acts first in turn order (higher priority, then draft order): 1253 of 1254;
- not the previous target (the camp switched off it 195 times for an earlier-acting ally level with it); lowest id
  only matches where id order is turn order;
- ties between our champion and an enemy (59) are unresolved: treated as ours.

## What changed
| Switch | Rule |
|---|---|
| CAMP_TIE_ORDER | An ally level with us is the camp's target when it acts before us (priority_of, then roster order): exit cost, camp heat, foreign hits, camp_tanked, swaps, tank steps, finishing. |
| SWAP_STEP | A hurt champion steps one tile so a healthier ally (over 50%, more health than us) is nearer, or level and acting first, and hits on. A melee may stand one tile out with no hit for up to 3 rounds. |
| RANGED_COVER | A ranged champion (not Willow) that an awake camp of the group hits, with a sturdy melee or healthier ally in that camp's reach, steps to a covered tile it still hits from. |
| TANK_TAKE_EARLY | A sturdy ally steps in front of (or level with, acting first) a camp hitting an ally under 60% with less health share than ours, or hitting a ranged ally while we are melee. |
| STAY_TANKED | A tank under the recall bar (over 20%) still counts as the camp's tank while another sturdy ally over 50% is in that camp's reach. |
| FINISH_ORDER | finish_ok counts only the camps that hit us, each until it dies in kill order (weakest first). |
| FINISH_RANGED | A ranged champion (not Willow) with no tank keeps hitting a started group it finishes above 20% health. |
| CAMP_STICKY (removed) | A tie counted as the other's when we lost no health since our last turn: the wrong rule. |
| TANK_PRIORITY (off) | Varg / Marcella / Vendetta priority 6, Willow 4, so tanks take ties. |

First cut let hurt champions (35-45%) cover each other: on corridors four of them swapped a 2-elk 2-wolf group around
until all were under the bar and Ryth died. Covers now need more health than us and 50%+.

## Results (all 70 maps both sides, release, 3 seeds = fuel burnt at turn end, 140 games each)
Step 1, vs v145:
| Config | Record | Banked | Per seed | Camp deaths | Fuel / turn |
|---|---|---|---|---|---|
| swaps + CAMP_STICKY | 212-196-12 | +1.2% | +1.5 / -2.1 / +4.3 | 270 vs 280 | +0.8% |
| swaps, no sticky | 214-196-10 | +5.6% | +9.3 / +6.8 / +0.9 | 295 vs 236 | +0.8% |
| swaps, no sticky, no RANGED_COVER | 215-193-12 | +2.7% | +5.7 / +0.0 / +2.5 | 309 vs 291 | |

Single-switch ablations (step 1, seed 0 only, full set 72-64-4 +1.5%): sticky off +9.3%, RANGED_COVER off +5.7%,
SWAP_STEP off +4.5%, FINISH_RANGED off +1.5% (65-71), STAY_TANKED off identical, FINISH_ORDER off -0.6%,
TANK_TAKE_EARLY off -0.3%. One seed swings about 10%.

Step 2, vs step 1's shipped bot (swaps, no sticky), head to head:
| Config | Record | Banked | Per seed | Camp deaths | Fuel / turn | Out of fuel |
|---|---|---|---|---|---|---|
| **CAMP_TIE_ORDER (shipped)** | **214-196-10** | **+5.2%** | +9.1 / +1.6 / +5.0 | 291 vs 315 | +0.9% | 7319 vs 6754 |
| CAMP_TIE_ORDER + TANK_PRIORITY | 206-204-10 | +2.8% | +4.6 / +3.7 / +0.0 | 279 vs 307 | +0.9% | 7630 vs 6543 |
| TANK_PRIORITY only | 213-202-5 | +3.5% | -4.2 / +11.3 / +3.9 | 336 vs 324 | | 7522 vs 6931 |

Debug corridors vs step 1: 924-356, deaths 12 vs 27; all 28 ally ties went to the champion acting first.

Open: the priority change helps alone but not on top of tie order; enemy-vs-ally tie rule; out-of-fuel turns +8%.
Benches: replays/bn148s0-2, bn148abl, bn148c1-2 (step 1), bn149s0-2 (step 2); debug replays/n148dbg, n148dbg2.
