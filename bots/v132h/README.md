# v132h

`v132g` + `GUARD_NEAR`: the walk guard only reads camps whose reach is within `GUARD_NEAR` (4) tiles of us. Not submitted.

## Lineage (all on `v131-campwait`; the other versions were deleted, their code is described here)
- **v132** `CAMP_WAIT_FIGHT`: in `fight()`, camps we stepped out of (remembered by `remember_hitting_camps`) are kept out
  of until seen asleep: walk / `join_first` / dashes go around them, else hold (`fight.wait`) unless an enemy can
  already hit us. `dash_towards` takes an avoid board.
- **v132b** `CAMP_WAIT_FIGHT_ROUNDS` = 4: that fight wait lasts 4 rounds after our last exit (v132 used 20).
- **v132c** `CAMP_WAIT_FARM` = false: v131's 20-round farm keep-out off. (Everything below builds on this.)
- **v132e** `CAMP_SHARE`: leave a camp that hits an ally beside us; `CAMP_WAIT_WALK`: `bank_gems` walks go around the
  fight-wait camps, else wait.
- **v132g** `CAMP_REACT_ANY`: reacting to an enemy (every `camp_exit` stem and `fight()`), leave every awake camp's
  reach we stand in, hitting us or an ally. `WALK_GUARD` (`build_walk_guard`, applied in `move_towards_closest` and
  `dash_towards`): every walk and dash keeps out of awake camps' reach - with an enemy near, every awake camp; otherwise
  camps no ally stands in, except the group we farm. Asleep or dead camps are never guarded; no guard while we already
  stand in one. The fight wait holds the guard too.
- **v132h** `GUARD_NEAR`: v132g read every camp in sight each turn (+~2k fuel a turn, +6%; ~30% more out-of-fuel
  turns). Only camps within reach + 4 of us now.

## Why v132 (the loop)
Two allies in an awake camp's reach: the camp hits the nearest; A steps out, the camp turns on B, B steps out as A
walks back in, and so on until both are worn down.

## Results (vs v131-campwait, 65 maps `maps/0*`, both sides, release; seeds = fuel burn at turn end)
| version | games | record | banked | kills / deaths (ours vs v131) | camp deaths ours / v131 | fuel / turn, out-of-fuel (seed 0, vs v131) |
|---|---|---|---|---|---|---|
| v132 | 130 | 54-69-7 | -8.5% | 1,855 / 2,283 vs 2,044 / 2,087 | 239 / 232 | 29.6k vs 30.0k, 1,278 vs 1,380 |
| v132b | 390 | 192-180-18 | +2.8% | 6,170 / 6,372 vs 5,627 / 6,876 | 745 / 706 | 29.9k vs 29.6k, 1,250 vs 1,369 |
| v132c | 390 | 179-190-21 | -1.6% | 5,894 / 6,738 vs 6,081 / 6,603 | 657 / 709 | |
| v132d (v132b + share/walk) | 390 | 183-186-21 | -1.6% | 5,603 / 6,692 vs 5,966 / 6,343 | 726 / 740 | 29.8k vs 30.1k, 1,398 vs 1,172 |
| v132e | 390 | 170-203-17 | -5.4% | 5,781 / 6,810 vs 6,130 / 6,532 | 680 / 751 | |
| v132f (v132d + react/guard) | 390 | 175-198-17 | -7.1% | 5,845 / 6,548 vs 5,911 / 6,569 | 637 / 724 | 31.4k vs 29.8k, 1,703 vs 1,353 |
| v132g | 390 | 182-187-21 | +0.4% | 6,043 / 6,248 vs 5,599 / 6,839 | 649 / 796 | 31.7k vs 29.6k, 1,537 vs 1,189 |
| **v132h** | 390 | **191-182-17** | **+5.2%** | 6,043 / 6,245 vs 5,605 / 6,818 | **640 / 775** | 31.5k vs 29.8k, 1,892 vs 1,208 |

v132h (`replays/b132h`): +7.3% / +3.9% / +4.5% gems by seed, camp deaths -17% vs v131. The fuel did not drop (31.5k vs
v132g's 31.7k, more out-of-fuel turns), so the gain over v132g is the nearer guard itself (fewer far-camp detours), not
fuel. The guard's fuel cost is still open.

v132 debug audit (`replays/b132d`): holds were short (~4 turns a game, longest 8) but late (80 of 139 ended over 5 rounds
after the exit); 1,891 fight turns walked around camps; repeat exits within 10 rounds were not reduced.
