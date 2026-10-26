# v135

v135 (built as v134-move) = v134 + movement fixes from a log audit of v134 self-play (65 maps `maps/0*`, debug, full logs). Not submitted.
Every fix is a switch in `constants.h` (section "v134-move", plus CAPTURED_MARK by the shared-memory notes).

## Results (vs v134, 65 maps both sides, release, seed 0 + fuel-burn seeds s1, s2 = 390 games)
| bot | record | banked | per seed | K/D ours vs theirs | camp deaths |
|---|---|---|---|---|---|
| **v134-move** (EXIT_COMMIT off) | **218-155-17** | **+19.9%** | +22.6 / +23.2 / +14.2 | 6028/5634 vs 5114/6663 | 520 vs 635 |
| EXIT_COMMIT on, fight wait off | 209-159-22 | +12.0% | +9.9 / +6.9 / +19.7 | 5871/5944 vs 5442/6499 | 502 vs 628 |
| EXIT_COMMIT on (all fixes) | 201-167-22 | +9.7% | +6.8 / +4.7 / +17.9 | 5754/6038 vs 5528/6384 | 510 vs 630 |

Fuel (seed 0, release, from the replays' .fuel): v134-move 32.9k mean / p99 79.2k / 1,204 out-of-fuel turns,
v134 in the same games 32.3k / 80.2k / 1,152. About +2% mean, out-of-fuel level.
Self-play (both sides the same bot, debug): moves undone the next turn 37,980 of 463,850 (8.2%) in v134, 22,787 of
433,255 (5.3%) in v134-move; Heart of Ruin camp deaths 71 -> 38.

## What the logs showed, and the fix for each
A "bounce" = a move undone the next turn (tile at t-1 == tile at t+1). Counts are from the v134 self-play.
| switch | cause (log evidence) | fix |
|---|---|---|
| CAPTURE_WAIT | 5,111 farm.walk <-> farm.off: a cleared group being captured counted as "up but out of sight", so the team walked onto its tiles, then FIX_POCKET stepped them off (00ambush r416-422: 4 champions, 7 rounds) | claimed by an ally: wait on the ring off its tiles; unclaimed reward: stand on it |
| CAPTURE_NEWS | respawn records are private: leaders who never saw a capture pointed the team back at the group (00ambush s0 <-> s11 every 3 rounds) | with no claim open, bytes 3/6 carry "group s respawn timer started at round r" |
| SHARE_LOOT_COOL, SHARE_LOOT_TEAM | 2,829 bounce turns: a follower sees loot, points the team at it, the leader (who can't see it) points back (00great-wall: Ryth 11 tiles from the team, 60 rounds) | no re-point within 20 rounds; only with an ally within TEAM_RANGE |
| FINISH_TANK | hurt.dodge <-> farm.camp: finish_ok called an ally the tank when it was nearer than *our current* tile, so outside "finish", inside "leave" (02the-pond) | the gap we will have while hitting (our reach) |
| BANK_OPEN | 2,225 bank.near.walk <-> farm.walk: go() seeds its field from camp tiles, so a bank walk headed for a camp-held submit tile and failed one step short (02guardians 662, 01shortcut 755 bounces) | camp tiles are no bank goal; altars with every submit tile a wall or living camp are skipped |
| HOP_STICKY | the hop planner only wakes with a thin hole within HOP_NEAR, so on one tile it took a 35-step hop route and on the next the 147-step walk the other way (02tangle ~400 bounces) | a dasher that took a hop route keeps planning hops next turn |
| TARGET_HOLD, INVADE_OWN_KEEP | 2,249 of 12,162 team target changes went back within 6 rounds; 947 were invasions flipping because a loot pick visible from only some tiles lowered the "normal" rate (02tangle: every round) | a new target holds 6 rounds (not when it went down, a kill raid/split/ping, or the new one is mid-clear); invasions also rate our current target |
| HOR_DASH_SAFE | Heart of Ruin's route/hop/escape dashes had no camp check (only the hop path checked camps in sight), so the landing stun woke groups, monoliths included | movement dashes never land within 1 of a camp spawn tile from the map (bushes too); no landing: no dash. Dive and camp dash skip camps out of sight |
| AOE_MAP_CLEAR | AOE_CLEAN only kept clear of camps in sight | camps out of sight within 6 from the map too |
| EXIT_COMMIT, EXIT_COMMIT_FIGHT (off) | 1,320 fight.exit <-> farm.camp: an exit is often 3 steps; one step later the mode flipped back to farm and walked back in. 432 of 500 such exits were for an enemy 6+ tiles away | after leaving awake camps, keep leaving and stay out until they sleep (10 rounds max). Cleaner movement, but -8 to -10 points banked: off |

## Log additions (debug only)
- Lead lines print every change of target (the dedup key used to strip the slot number).
- `| guard (x,y) ...`: the walk went around the walk guard, and which camps it held.
- `| hop ... : hop/walk`, `| hop: <reason>`: hop route vs walk lengths, or why a dasher did not plan a hop.
- `- exit: nearest enemy N away, team <mode>, camp heat on us N` on every camp exit.
- `- near bank at (x,y) skipped: <reason>` and a field dump on "no closer step" (steps to goal around us, # wall, u unit).
- `- camp wait: ...`: why remembered camps are or are not kept out of.
- `- team news: sN respawns in N`.

## Not fixed
- Exits triggered by an enemy 6+ tiles away (own sight flips fight/farm at the vision edge).
- Willow's hold/follow and ranged holds react to allies' steps (~1,200 hold <-> camp bounces); HoR's CAPTURE_HELP
  join/leave while tanking feeds them.
- Claim give-back loop: the leader claims a reward for Willow each round, she gives it back ("too hurt").
- Ranged champions idle "boxed in" by the walk guard while an enemy is near (00dunes).

---

# v133t

The current bot. Built up in steps, newest first below:
- **v133t** = v133s + fuel fixes only (same decisions).
- **v133s** = v133q2 + GO_EXIT_DASH (camp exits count a ready dash).
- **v133q2** = v133a + GO_URGENT_DASH, GO_KITE, GO_GUARD_DASH, GO_HOP_SAVE 2.
- **v133a** = v132h with every walk, dash and hop through one mover, `go()`.
- **v132h** and before: see the last section.

Not submitted. Every v133 version but this one was deleted (and v132h, which stays in git at d5e60f7). Each v133
experiment is a GO_* switch in `constants.h`, so any of them can be rebuilt from this code with
`python bots/v133t/bench.py variant v133t NAME GO_...=...` (v133a = every GO_* off and GO_HOP_SAVE = HOP_SAVE; the fuel
fixes stay in, they do not change decisions). Tools here: `bench.py` (variants, fuel-burn seed copies, manifests,
summary), `profpatch.py` / `profsum.py` (per-function fuel profile of a throwaway copy).

## v133t: fuel
### Where the fuel went (v133s, profiled: `profpatch.py` + `profsum.py`, 12 + 3 maps, release)
Mean 32.3k a turn; out-of-fuel turns were mostly 00lanes, 02the-pack and 01boss-battle. On those turns: the team
decision (sync_decision ~34k: travel ~15k, drop_hard ~9k, score_mode ~7k, best_goal ~4.5k), update_team_chase ~7k, the
walk guard's first vision reads ~12k, observe ~5k, vision lists ~8k. The common waste: `champion::position()`,
`attack_range()`, `is_recalling()` are each a full engine read (~100 fuel, `camp::attack_range()` ~220 through
base_stats), and hot loops re-read them per pair (camps x allies, allies x enemies).

### Fixes
- `seen::positions(team)`: the tiles `visible_living_into` already reads, kept beside the vision lists (same retire
  rules); `seen::self_position()`: our tile once per vision generation. Used in near_by_steps, the camp loops
  (`other_nearer` in camp_heat / foreign_camp_hit / stray_heat, the walk guard's tank check), update_ping,
  remember_allies, within(), go().
- update_team_chase: every unit's tile, range and channel read once (was per ally x enemy pair, twice with CATCH_MODEL);
  `short_of` takes values.
- squishy_overextended: enemy and tank tiles read once into stack arrays (was per comparison).
- drop_hard: hard_left's free part once for both passes (was a roster and round read per group, ~27 calls a turn).
- ability_of: our own two slots matched once (abilities() copied both slots on every call).
- observe: a per-pass stamp dedupes groups (was a sort + unique over every visible spawn tile).
- scout_ward: reused boards and `*_into` reads (each `~`, `|`, new board and `positions()` allocated, ~8 a call).

### Results (vs v132h, 65 maps both sides, release; fuel from the replays' .fuel, `bots/v123/fuel_bench.py`)
| seed | v133s mean / p99 / out-of-fuel | v133t mean / p99 / out-of-fuel |
|---|---|---|
| 0 | 32,560 / 84,045 / 2,129 | 30,830 / 77,419 / **818** (-62%) |
| s1 | 35,061 / 86,209 / 2,374 | 33,848 / 81,468 / **1,194** (-50%) |
| s2 | 37,696 / 89,308 / 2,982 | 36,000 / 82,701 / **1,567** (-47%) |

Turns at 90k or more: 5,111 -> 2,157 (seed 0). Record over the 390 games: v133t 196-175-19, +6.4% banked (v133s
184-187-19, +7.1%): level. Replays `replays/b133t`.

### Equivalence
Debug games v133s vs v133t on 4 maps: 00gemfight (no out-of-fuel turn) logs identical line for line; the others
diverge right after a turn that ran out of fuel and resumed next round. There v133s re-read positions live while v133t
uses the snapshot taken with the vision list (the list of which units are visible was already a snapshot).

### Not done
- go()'s distance field (~3k a call, the engine's own move_towards is ~6k) could be cached per goal board over static
  terrain; it changes routing around units, so it needs a bench.
- travel/best_goal/drop_hard are O(groups) passes (~45 fuel a group); on maps with ~60 groups they stay ~4-8k.
- update_enemies / update_invasion read every champion's life every turn (~1.4k); reading less often changes timing.

## v133s: GO_EXIT_DASH (on v133q2's switches)
Leaving awake camps (`camp_exit`, `stray_exit`, `fight.share`) counts a ready dash as this turn's reach (dash range + the
walk) when it picks the exit tile, goes even when the walk is spent (dash only), and moves as a getaway (`leave_camps`).
v133q3 = the same code with GO_EXIT_DASH off (the control; same switches as v133q2). Replays `replays/b133x`, `b133xh`.

| match | record | banked | per seed | K/D ours vs theirs | camp deaths |
|---|---|---|---|---|---|
| v133s vs v132h | 184-187-19 | +7.1% | +6.3 / +5.8 / +9.3 | 6170/5872 vs 5330/6824 | **542** vs 654 |
| v133q3 vs v132h | 194-178-18 | +10.1% | +14.9 / +4.7 / +11.0 | 6316/5969 vs 5356/7017 | 613 vs 701 |
| v133s vs v133q3 | **198-175-17** | **+2.8%** | -1.7 / +9.3 / +0.9 | 6076/6635 vs 6000/6742 | 635 vs 666 |

Head to head v133s wins (and dies less, to camps too); against v132h it is 3 points under the control, inside the noise.
Camp deaths vs v132h fall 12% (542 vs 613). Kept on, as the user asked: exiting camps as fast as possible is the rule.

## v133a to v133r2: one mover, and the movement experiments
`v132h` with one mover: `go(goals, Go)` in `actions.h` / `actions.cpp`. Every walk, dash and hop goes through it;
callers pick the goals and how the dash may be spent (`DashUse`: keep / route / finish / urgent), plus an optional
landing ranking of their own (`Go::rank`). `move_towards_closest(goals, avoid)` is now `go(goals, {avoid})`.

v133a keeps v132h's movement: the GO_* switches in `constants.h` are all off, and each one is a v133 experiment.

### What moved into go()
- `dash_towards` (melee dashers and Vesper closing on a target): now `close_in()` with a straight-line ranking (or
  `GO_FINISH_PATH`: path steps to the target's ring).
- `dash_escape` (the hurt dodge's dash): now `Bot::escape_rank()`, a ranking handed to `go(~zone)` (or `GO_ESCAPE_PATH`).
- `vesper_kite`: the same out-of-reach ranking, through `go()` with `walk = false`.
- `dash_along` / `hop_route`: unchanged, inside `go()`; `HOP_SAVE` there is `GO_HOP_SAVE`.
- Deleted dead code: `escape()` (`FIX_DODGE` off since v72), `vesper_close_safely` (`VESPER_SAFE_DASH` off since v115).

### Experiments (each v133a + one switch)
| bot | switch | what |
|---|---|---|
| v133b | GO_URGENT_DASH | retreat, camp exits and the hurt dodge spend the dash even under VESPER_KEEP_DASH |
| v133c | GO_HOP_SAVE 2 | a hop route need only beat walking by 2 steps plus the dash cooldown (was 10) |
| v133d | GO_HOP_AVOID | walks with a keep-out board hop too (they never did) |
| v133e | GO_ESCAPE_PATH | the hurt-dodge dash lands on the shortest path out of reach, not the farthest straight-line tile |
| v133f | GO_GUARD_DASH | the escape and kite dashes keep out of awake camps' reach (the walk guard) |
| v133g | GO_FINISH_PATH | closing dashes count path steps, not a straight line (no landing behind walls) |
| v133h | GO_SHORT_DASH | a route walk does not dash when its walking step already reaches the goal |
| v133k | GO_KITE | Vesper: in a fight, fire then dash after a fleeing target; retreating from enemies with no dash ready, dash out then fire |

Bench tools: `bots/v133t/bench.py` (variant copies, seeded copies, manifests, summary).

### Results
All vs v132h, 65 maps `maps/0*` both sides, release; 3 runs = seed 0 plus fuel-burn copies s1, s2 (390 games).
Banked = our tributed gems over theirs. Replays: `replays/b133`, `b133s`, `b133g2`, `b133m`, `b133q`, `b133z`.

| bot | contents | record | banked | per seed | K/D ours vs theirs | camp deaths |
|---|---|---|---|---|---|---|
| v133a | refactor only | 181-183-26 | -0.3% | +1.1 / -2.3 / +0.4 | 5460/5987 vs 5360/6032 | 627 vs 572 |
| v133b | GO_URGENT_DASH | 178-187-25 | +3.2% | +4.3 / -6.8 / +13.1 | 5800/5982 vs 5356/6543 | 626 vs 743 |
| v133c | GO_HOP_SAVE 2 | 187-183-20 | +0.4% | +2.7 / -2.6 / +1.1 | 5553/6057 vs 5416/6202 | 641 vs 649 |
| v133d | GO_HOP_AVOID | 182-182-26 | -2.8% | -2.7 / -6.9 / +1.4 | 5356/6026 vs 5380/5966 | 646 vs 610 |
| v133e | GO_ESCAPE_PATH | 193-175-22 | +0.1% | -0.7 / -1.5 / +2.7 | 5624/6005 vs 5382/6233 | 623 vs 609 |
| v133f | GO_GUARD_DASH | 186-179-25 | +0.5% | -1.2 / +0.8 / +2.0 | 5610/5982 vs 5397/6207 | 585 vs 597 |
| v133g | GO_FINISH_PATH | 177-190-23 | -9.0% | -12.1 / -10.4 / -4.4 | 5303/6618 vs 5932/5982 | 686 vs 679 |
| v133g2 | g + straight-line tie-break | 182-182-26 | -0.8% | | 5481/5966 vs 5326/6071 | 640 vs 590 |
| v133h | GO_SHORT_DASH | 182-188-20 | -6.7% | -0.3 / -15.8 / -3.1 | 5505/7026 vs 6331/6178 | 695 vs 673 |
| v133k | GO_KITE | 186-177-27 | +1.3% | +0.3 / -0.2 / +3.8 | 5760/5911 vs 5305/6405 | 606 vs 645 |
| v133m | b + k | 190-176-24 | +5.9% | +2.0 / +0.7 / +15.5 | 6030/6094 vs 5473/6730 | 621 vs 700 |
| v133p | b + k + f | 187-177-26 | +7.5% | +2.7 / +3.9 / +16.4 | 6239/5935 vs 5316/6947 | 619 vs 708 |
| v133n | p + c + e | 183-190-17 | +4.5% | +5.7 / +4.9 / +2.9 | 6235/5817 vs 5194/6919 | 623 vs 684 |
| v133q | p + c | 191-180-19 | +7.6% | +7.4 / +5.5 / +9.9 | 6324/6008 vs 5391/7034 | 617 vs 710 |
| v133r | p + e | 193-174-23 | +8.2% | +5.0 / +6.4 / +13.4 | 6173/5683 vs 5055/6871 | 628 vs 698 |
| **v133q2** | q, with the vesper_retreat fix | **193-179-18** | **+9.5%** | +13.2 / +5.5 / +10.2 | 6316/5922 vs 5311/7024 | 611 vs 708 |
| v133r2 | r, with the fix | 193-175-22 | +7.6% | +7.3 / +6.1 / +9.6 | 6095/5693 vs 5066/6796 | 627 vs 701 |
| v133n2 | n, with the fix | 185-189-16 | +5.3% | +8.7 / +5.6 / +1.9 | 6214/5677 vs 5053/6896 | 624 vs 682 |

v133p on seeds s3-s5 (9-15k fuel burnt a turn, out-of-fuel turns doubled): 188-178-24, -0.1%; those copies are too
fuel-starved to compare.

Notes
- g lost because the path ranking scores every ring tile 0 steps and took the first in engine order (often at the edge
  of reach); g2's straight-line tie-break makes it level, not better.
- h lost, likely because a dash that already reaches the goal leaves the walk unspent, and for Vesper that walk is her attack (not checked in logs).
- c and e each help on p but not together (n, n2 lower than q/r in both rounds).
- Out-of-fuel turns, seed 0: v133a 1,741 (v132h 1,988 in those games), q2 2,123 (v132h 1,552), r2 1,983. Not explained
  yet (more fights per game, or the lower hop bar building more hop fields).
- The fix in q2/r2/n2: `vesper_retreat` read enemies out of sight after its dash ("Unit is not in vision").

## v132h and before
**v132h** = `v132g` + `GUARD_NEAR`: the walk guard only reads camps whose reach is within `GUARD_NEAR` (4) tiles of us. Not submitted.

### Lineage (all on `v131-campwait`; the other versions were deleted, their code is described here)
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

### Why v132 (the loop)
Two allies in an awake camp's reach: the camp hits the nearest; A steps out, the camp turns on B, B steps out as A
walks back in, and so on until both are worn down.

### Results (vs v131-campwait, 65 maps `maps/0*`, both sides, release; seeds = fuel burn at turn end)
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
