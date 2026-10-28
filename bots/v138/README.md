# v138

v138 = v137 + a diagonal chase (CHASE_STEP, CHASE_DASH, CATCH_ACROSS) and a getaway that keeps clear of where chasers
are heading (RETREAT_LEAD), in the "v138 switches" block at the top of `src/constants.h`. Benches: all 70 maps in
`maps/`, both sides, release, seed 0 plus fuel-burn copies s1 and s2 of both bots (`bots/v133t/bench.py`), vs v137.

## The idea (the user's)
Moves cost the same in all 8 directions and reach is Chebyshev (the larger axis gap). An enemy running up, with us to
its right, is caught by going up-left: the y gap stays 0 and the x gap shrinks a tile a turn. Going straight at it
(left) lets it pull away on y. An enemy running up-left is followed up-left: the x gap can't shrink, but the y gap
stays 0, so one step left catches it when a wall stops it. v137 walked the shortest path to the tiles in reach of
the enemy's current tile; in Chebyshev two or three steps tie, and the tie went to whichever tile came first in the
engine's tile order: right for some escape directions, wrong for others.

## What it does
- **The enemy's step** (`track_enemies`, `enemy_step`): the GO_KITE tracker keeps every enemy's tile this round and
  last round; its step (one tile per axis at most) is known only when both sightings exist and it moved. The tracker
  now reads types from the ident cache and tiles from `seen::positions` (no engine reads of its own).
- **CHASE_STEP**: a champion target out of reach whose step we know, stepping away or sideways, is chased at the tiles
  in reach of where that step puts it next (one step, no longer prediction). Among equally short steps, and equal dash
  landings, the one nearest that tile: Chebyshev first, then the other axis (`Go::align`). Step unknown, the tile a
  wall, or no move toward it: v137's chase at where it stands. Melee (HoR too), ranged, Vesper; mages keep their kite.
- **CHASE_DASH** (user): in those chases the dash is never held back (VESPER_KEEP_DASH, JOIN_KEEP_DASH); any dash
  that gains ground goes (`Go::free_dash`). CHASE_DASH_VESPER switches it for Vesper alone.
- **CATCH_ACROSS**: CATCH_MODEL assumed an enemy walking away can never be closed on foot, and dropped the chase.
  That holds only when its step takes it farther from the chaser; a target whose last step kept the gap is catchable.
  Applied in the team catch check and chase_line.
- **RETREAT_LEAD** (in `retreat`): enemies stepping toward us or across have their reach drawn again at their next
  RETREAT_LEAD_STEPS (2) tiles (`lead_path`: down their walk field home when the step went homeward, else straight on,
  sliding along walls). The getaway goal (to the ally, else out of reach) keeps out of that too. Being out of reach
  (then farm) is judged as before; an empty goal falls back to v137's.

## Results (vs v137; 420 games a line)
| Bot | Record | Banked | Per seed | K/D ours vs theirs | Camp deaths |
|---|---|---|---|---|---|
| **v138 (all on)** | **223-188-9** | **+9.2%** | +9.6 / +8.9 / +9.2 | **6655/6260** vs 5981/6949 | 279 vs 294 |
| CATCH_ACROSS off | 211-199-10 | -0.4% | -2.2 / -2.4 / +3.6 | 6221/6547 vs 6261/6508 | 286 vs 287 |
| CHASE_DASH_VESPER off (Vesper keeps her dash) | 202-209-9 | -3.1% | -0.6 / -2.3 / -6.5 | 6339/6916 vs 6617/6627 | 299 vs 288 |
| RETREAT_LEAD only (no chase changes) | 207-204-9 | -0.7% | -1.6 / -4.4 / +4.2 | 6437/6124 vs 5835/6724 | 289 vs 287 |

Replays: `replays/b139a*` (all on, CATCH_ACROSS off), `b139b*` (Vesper keeps her dash), `b138x*` (RETREAT_LEAD only);
the folder names are from before the rename. The step alone is level; with the catch fix it pays on every seed.
Vesper dashing in chases is worth ~12 points. RETREAT_LEAD alone is level on banked gems but shifts K/D our way.

Lost and removed: CHASE_LEAD, aiming the chase several steps ahead along the enemy's predicted path home
(179-230, -7.9%, every seed down, K/D worse). Keep chase changes one step ahead.

## Fuel
Seed 0 (no burn), mean fuel per champion turn and out-of-fuel turns, ours vs v137 in the same games:
33.5k vs 32.8k (+2.1%), out-of-fuel 1,516 vs 1,452. With the burn seeds the out-of-fuel counts rise more
(7,727 vs 6,042 over all three): more fight turns, which are the dear ones, pushed over the limit by the burn.
The new code itself is small: the tracker reads no engine values of its own, and the tie-break is ~8 field reads.
