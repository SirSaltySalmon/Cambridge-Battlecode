# v148-147

v148-147 = v147 + the user's Varg farm plan (switches in `src/constants.h`, block "v148 switches"). bots/v148-145 is
the same code on v145 (the user's second base; bots/v148-144 was a mistaken v144 port and lacks the later switches).
Plan: `~/.claude/plans/lexical-watching-piglet.md`.

## What it does
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

## Spawn loops fixed (after the first bench; the user saw Varg and Vesper stuck at spawn)
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

## Results before the fixes (22 plan maps, both sides, release; non-plan maps play as the base)
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
