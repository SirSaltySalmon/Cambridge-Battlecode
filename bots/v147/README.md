# v147

v147 = v146 with RETREAT_LEAD off (user: retreats were chaotic, the enemy-step predictions not in our favour).
Benches: all 70 maps, both sides, release, seed 0 + fuel-burn s1 / s2, vs v146.

## Results (vs v146)
420 games 217-190-13, banked +3.8% (+5.3 / +10.2 / -3.6), K/D 6416/6330 vs 6052/6700, camp deaths 278 vs 284.
Fuel (all but 01boss-battle): +0.45% a turn, out-of-fuel 7813 vs 8053. When it went in (v138, b138x) it was 207-204,
-0.7% banked with better K/D and +12% out-of-fuel turns.

## Seed copies (removed in final-stretches)

`bots/v147-s1` and `bots/v147-s2` were fuel-burn seed copies: the same code with `{ volatile int sink = 0; for (int i = 0; i < 300; ++i) sink += i; }` before `cambc::end_turn()` in `src/main.cpp`, so fuel timing (and every timing-dependent outcome) reshuffles. Benches pool seed 0 (this folder) with the two copies; recreate them by adding that line.
