# v97

`v96` + two fixes for champions stuck behind holes, plus a diagnosis of where farming time goes. **Not a proven win:**
against the v96 mirror it flips 1 game for and 3 against (within the +-3 noise; see below), and only islands moves
for a visible reason.

## Diagnosis

### Ladder (55 v95 games, `ladder_pace.py`, summaries in `replays/ladder/`)

- About 13 of the 21 losses are pure gem pace: few kills either way (corners, monolith x3, broken_curve x3,
  islands-b x2, corridors, fracture, gold_rush, interlock). Our bags stay small; we bank steadily. The opponents
  **collect** faster: islands-b ~176 gems / 150 rounds against our ~50, broken_curve ~110 against ~85.
- islands-b (97cd92c5): the opponent recaptured its richest group (2 elks + 4 gem clusters, 58 gems) 12 times, every
  ~75 rounds. We took our mirror of it at r86 and r167, then not again until r903.
- 3-pick maps: Varg/Ren/Vesper went 1-3 against Cassi & Issac + Stellar + Ren (two bodies for one pick). Copying
  that roster does not help: every C&I roster lost to v96 (deaths double: C&I die together).

### Turn use (all 21 maps, v96 vs v95 with debug logs, `analyze_waste.py`)

Only 23% of champion turns hit camps. Idle 11.5%, waiting 8%, walking 23.5%. By map:

- **Waiting on respawns** after the aggressive route switches on: 40-55% of turns after it on ambush, corridors,
  big-world, gold-rush, fragment. `best_goal()` only looks at the centre pocket (`AGGRESSIVE_RADIUS` 12) once
  aggressive: on ambush its groups rate 0.0-0.1 gems a round while groups at 0.6-1.0 are 20-50 steps away. On ambush
  every group is an elk pair or bigger, so nothing can be taken by a few champions while the target respawns.
- **Walking**: concentrated-wealth 62%, fracture 55%, expanse 45%. On concentrated-wealth the rich centre group is
  walled off from our spawn: the team walks ~100 steps round the top of the map each trip, where a one-tile row
  of holes at (23-30,17) would take one dash. Movement never plans a dash over holes.
- **Boxed in** (no walking path to the goal): islands 18%: see Changes.
- **boss-battle**: the only altar (25,20) is set in a wall; its one standable neighbour is a monolith spawn tile
  (2 x 20,000 health). Both teams collect thousands of gems and bank almost none; `bank_gems` walks at the altar
  forever. The game goes to the gold tiebreak; we won all 3 on the ladder.
- **contest** (no gem camps, only the monoliths): v96 loses the first skirmish (~r22) and snowballs (80 deaths);
  v95 won it. Not looked at further.

## Changes

- **FIX_WALK_CLAIM** (`route.cpp` `update_capture`, `self_claim`, `collect`; `Bot::walkable`): a capture claim only
  goes to an ally with a walking path to the reward (the route group's walking field, built from spare fuel), and a
  collector without one gives it back unless it can dash and is within `WALK_CLAIM_DASH` (3) of it. On islands a
  gold-cluster pair in a hole pocket kept its reward coming back (ranged hits and splash clear it from outside), and
  the claim went to one walker after another: 920 turns idle at the edge, one champion in five, all game.
- **FIX_WALK_TARGET** (`combat.cpp` `walk_target`, `draft::group_load`): a champion with no walking path to the
  team's target (a dashing leader picked a pocket) farms the best group it can walk to, with a load (strength x
  health) of at most `WALK_TARGET_LOAD` (one or two wolves, an elk, clusters). Islands: boxed turns 18% -> 1%.
- Tried and dropped (`results-v97-vs-v96.txt`): farming small groups while the target respawns (20-22), letting the
  aggressive route aim anywhere (21-21; both together 18-24, +12% gems by r600 but 17% more deaths), letting dashers
  take pocket claims (islands -600 gems a game), and the C&I rosters above.

## Results (vs v96, 42 games, release; `analyze_flips.py` against the v96 mirror `replays/bench-v97/mirror96`)

- 19-23. Same result as the mirror in 38 of 42 games. Flipped: lanes-a (gained) and lanes-b (lost), fracture-a
  (lost), boss-battle-b (lost, 0-0 gems both ways, decided on gold).
- islands: 1001-612 as A (mirror 1025-855), 957-1015 as B (mirror 855-1025). In a debug run islands-b is a win,
  1012-845.
- fracture-a: in debug mode v97 plays it identically to v96 for 20,000 log lines, so this flip is release-mode chaos
  (fuel), not these rules.

## Follow-ups (biggest first)

1. Dash over holes in `move_towards_closest` for dashers when it saves many steps (concentrated-wealth, islands,
   fracture). Walkers (Ryth, Willow) would still go round, so it splits the team.
2. The aggressive pocket: score `best_goal` over every open group (tried: more gems, more deaths), or make the
   centre pocket bigger, or let the aggressive route end when the pocket's rate drops.
3. boss-battle: treat a camp group standing on every standable altar neighbour as the bank target while carrying.
4. Mirror benches against v96 cannot show pace against other teams; most results are fixed by map side. A
   different-style opponent would measure pace better (v93 is too weak: 88-1014).