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

## Test folders removed in final-stretches

These folders were local test variants that never shipped. Their READMEs are kept below; where they say `bots/<folder>/README.md` for a removed folder, the text is in this section. The code is in git history (vargvale-strikes-back, commit a31dc30 and before).

### bots/v148-147 (Varg farm plan on v147)

v148-147 = v147 + the user's Varg farm plan (switches in `src/constants.h`, block "v148 switches"). bots/v148-145 is
the same code on v145 (the user's second base; bots/v148-144 was a mistaken v144 port and lacks the later switches).
Plan: `~/.claude/plans/lexical-watching-piglet.md`.

#### What it does
- Draft (`draft::varg_plan_map`): a group whose fighters are exactly two elks on our half (mirror split of the spawn
  centres, the line counts as ours). 22 of 70 maps: packs, broken-curve, the-farm (3); interlock, reverse-spiral, swirl,
  shortcut, guardians (4); 14 five-player maps. Varg takes Marcella's place on 3/5, Ryth's on 4; on 5 Prism takes
  Ryth's place on maps over PRISM_MIN_GEMS_5. Other maps draft as v147.
- Stage 0: loot funnelled to Varg (rank 1) until his core (Monster Strength + Jungle Health), bought in one trip.
- Stage 1 (4/5, VARG_PAIR): Vesper recalls with him (TARGET2 = VARG_RECALL_MARK), follows him, never opens a sleeping
  camp, both stand on capture tiles (one-tile rewards go to her). Team keeps to our half and leaves his group.
- Stage 2 (after Vesper's 4 emblems): Varg farms our half alone, picked by gems per round of his own time
  (walk + respawn wait + his simulated clear + capture) behind a damage gate; Vesper funnel off.
- Varg: shield before the first hit, recall at 30%, banks at 45, own fight score -5, allies answer his pings from 20.
- Shared memory: his group rides in TARGET2 (byte 5 bits 0-5, unused since no roster drafts Cassi & Issac).

#### Spawn loops fixed (after the first bench; the user saw Varg and Vesper stuck at spawn)
Found with an idle check on release stats (alive, no camp or champion damage, gold or gem change) and debug replays:
- 01yellow-brick-road: Varg priced s0, a road of 30 gold clusters (radius 15) whose end tile touches our spawn, at
  ~1 step. spawn.leave stepped onto that tile, farm.off ("due, we stand on its spawn") stepped back onto our spawn:
  ~150 rounds for both. Fix: VARG_SPREAD 4 (his picks are compact groups only; every pair is radius <= 2), and an apart
  champion's step-off never lands on our spawn.
- 01the-farm (v148-145): the bank-after-shop rule held only on spawn: one step toward the altar, then the off-spawn
  bar (45) sent him back across a spawn tile to farm, every round for 450+ rounds. Fix: varg_bank_due holds the bank
  off spawn too until his bag is empty.
After both, only 01boss-battle shows long stalls (the whole team, also in the base: the monolith trap).
Seed 0 after the fixes: v148-147 vs v147 9-33-2, -48.3%; v148-145 vs v145 11-32-1, -44.4%.

#### Results before the fixes (22 plan maps, both sides, release; non-plan maps play as the base)
| vs base | Record | Banked | K/D (ours vs theirs) |
|---|---|---|---|
| v148-147 vs v147, 3 seeds | 26-96-10 | -49.9% | 959/2761 vs 2564/1138 |
| v148-144 vs v144, 3 seeds | 20-103-9 | -56.0% | 940/2917 vs 2722/1101 |
| v148-145 vs v145, seed 0 | 12-31-1 | -43.2% | 367/859 vs 801/426 |
By team size (v148-147): 3-player -50%, 4-player -65%, 5-player -45%. Only boss-battle (+69%) and treasure-box (+94%)
gained.

Ablations (v148-147 vs v147, seed 0, 44 games each):
| Variant | Record | Banked |
|---|---|---|
| full plan | 8-34 | -51.4% |
| VARG_FIGHT_PENALTY 0 | 11-31 | -44.7% |
| VARG_PAIR off | 7-33 | -48.6% |
| VARG_SOLO off | 9-34 | -48.1% |
| VARG_FUNNEL off | 15-25 | -26.5% |
| penalty 0 + pair off | 11-29 | -36.0% |
| funnel, pair, solo off, penalty 0 | 14-27 | -19.0% |
| VARG_STRATEGY off (draft + build only) | 15-27 | -16.6% |
The Varg swap itself loses ~17% on these maps; the funnel to the tank is the costliest part (he stands on rewards
while the squishies open the next camp, debug 00swirl r66-74). Out-of-fuel turns are level with the base except the
funnel-off variants (~1,100 vs ~550, not looked into).

### bots/v148-145 (Varg farm plan on v145)

v148-145 = v145 + the Varg farm plan, the same code as bots/v148-147 (see its README for what it does and the
ablations). The two folders differ only by v145 -> v147 (VESPER_LEASH / HOR_DIVE_FOLLOW / RETREAT_LEAD and their code).

#### Results (22 plan maps, both sides, release, seed 0)
vs v145: 44 games 12-31-1, banked -43.2%, K/D 367/859 vs 801/426, camp deaths 58 vs 59, out of fuel 592 vs 502.
After the spawn-loop fixes (see bots/v148-147/README.md): 11-32-1, -44.4%, K/D 389/865 vs 799/440, out of fuel 585 vs 525.

### bots/v148-144, v148-144-s1, v148-144-s2

The Varg farm plan ported to v144 by mistake (it lacks v145-v147's later switches), with its two fuel-burn seed copies: the same code with `{ volatile int sink = 0; for (int i = 0; i < 300; ++i) sink += i; }` before `cambc::end_turn()` in `src/main.cpp`, so fuel timing (and every timing-dependent outcome) reshuffles. Benches pool seed 0 (this folder) with the two copies; recreate them by adding that line. Result above: v148-144 vs v144, 3 seeds, 20-103-9, banked -56.0%.

### bots/v148-evo (EVO COPY)

EVO COPY of v148-normal for testing tools/evotune: identical play; ten fight-score constants moved from
`src/constants.h` to `src/evo_constants.h`. Not a release candidate. tools/evotune's checks ran on it before v149 got its own `src/evo_constants.h`.
