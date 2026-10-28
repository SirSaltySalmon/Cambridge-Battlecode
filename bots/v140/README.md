# v140

v140 = v138 + residual vision (GHOST, GHOST_WARD), in the "v140 switches" block of `src/constants.h`. Benches: all 70
maps in `maps/`, both sides, release, seed 0 plus fuel-burn copies s1 and s2 of both bots (`bots/v133t/bench.py`), vs v138.

## The idea (the user's)
In v138 a target that stepped into a bush or around a wall left `seen::champions`, so the next turn `fight()` found
nobody and the champion went back to farming at once. In LoL you keep chasing into the fog: it is probably still going the
way it was going, and you ward the bush it went into.

## What it does
- **The ghost** (`mark_ghost`, at `engage` in `fight()`): the champion we fight is remembered by its tracker slot. The
  GO_KITE tracker (`track_enemies`) already holds its last tile and last step, so the ghost costs no engine reads. Willow
  (backline) never marks one.
- **Where it may be** (`ghost_where`): for GHOST_ROUNDS (5) rounds after its last sighting, anywhere we can't see within
  its walk since then (+1 for a dash). The best guess is where its last step leads (`lead_path`: straight on, sliding
  along walls, or down its walk field home). The ghost is dropped when the enemy is seen dead, every tile it could be
  on is in our sight, the guess lies in its home chase ring, or the sighting is too old.
- **GHOST** (`chase_ghost`, in `play()` after the fight step, in fight or farm mode with no enemy near): walk to the
  unseen tiles beside the guess (else any it could be on), taking the step nearest the guess. A melee champion hits the
  target at once if the step shows it. Skipped when the team chase is off, under 60% health, or as a hurt carrier.
- **GHOST_WARD** (before the scout ward): the ward (no action) goes onto the unseen tile in reach nearest the guess.
  A refused ward means the enemy stands there, and it is revealed. Either way the fight logic sees it the same turn.

## Results (vs v138; 420 games a line)
| Bot | Record | Banked | Per seed | K/D ours vs theirs | Camp deaths |
|---|---|---|---|---|---|
| **v140 (5 rounds, health 0.6)** | **225-182-13** | **+10.0%** | +12.5 / +8.3 / +9.3 | **6938/6451** vs 6199/7207 | 252 vs 269 |
| 5 rounds, health 0.4 | 221-185-14 | +9.9% | +10.1 / +7.4 / +12.3 | 6897/6489 vs 6231/7167 | 258 vs 270 |
| 3 rounds (health 0.4) | 214-193-13 | +8.6% | +9.7 / +7.5 / +8.7 | 6951/6466 vs 6203/7235 | 263 vs 284 |
| 4 rounds (health 0.4) | 221-185-14 | +9.0% | +13.9 / +4.6 / +8.8 | 6835/6394 vs 6138/7106 | 256 vs 271 |
| 6 rounds | 214-194-12 | +4.3% | +7.8 / -1.1 / +6.6 | 6724/6711 vs 6444/7029 | 267 vs 305 |
| 4 rounds, GHOST_WARD off | 226-180-14 | +6.2% | +8.9 / +4.9 / +5.0 | 6637/6592 vs 6309/6939 | 283 vs 302 |
| 4 rounds, GHOST_DASH on | 216-192-12 | +8.4% | +9.1 / +5.5 / +10.6 | 7001/6520 vs 6273/7280 | 247 vs 279 |
| 4 rounds, GHOST_NEAR 6 | 205-203-12 | +3.1% | +4.0 / +8.5 / -2.9 | 6597/6721 vs 6453/6874 | 268 vs 277 |
| 5 rounds, GHOST_JOIN on | 213-195-12 | +7.3% | +6.1 / +5.1 / +10.7 | 6841/6541 vs 6286/7097 | 255 vs 256 |

Replays: `replays/b140s*` (round one, 4-round base), `replays/b140t*` (round two, 5-round base; `v140h6` is the final v140).
The ghost pays on every seed at 3-5 rounds. 6 rounds chases too long. The ward is worth ~3 points. Remembering only
targets fought within 6 tiles (so not those that walked out of sight range) loses most of the gain. Requiring an ally
near, or dashing after the ghost, adds nothing.

In 4 debug games before the bench, 21 of 39 ghost runs ended in `fight.hit` (target found again). Willow then made 30 of
the runs, alone from the backline, so she was excluded before the bench.

## Fuel
Seed 0 (no burn), mean fuel per champion turn, ours vs v138 in the same games: 33.6k vs 33.0k (+1.8%). Out-of-fuel turns
1,682 vs 2,953 (v138's include 1,065 on one boss-battle side); turns at 90k+ 4,345 vs 5,231.
