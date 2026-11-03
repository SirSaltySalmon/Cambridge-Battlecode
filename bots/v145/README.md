# v145

v145 = v144v (CONTEST_VALUE on, MIDDLE_FIRST off) + AWAKE_READ, in `src/constants.h` after CONTEST_ANYWHERE.

## The problem (user's ladder replay, 01corridors)
Willow and Vesper took the first elk hits (r82-84); Willow died to two elks after five hits in a row (r1222-1227). v143
debug corridors: 46 Willow turns of camp damage outside fights. Causes found in logs:
- willow_exposed / under_threat counted a full-health camp as asleep; damage wakes the whole group, so the pair's
  second elk hit her while she "held" or kept attacking.
- ranged champions open a camp with no melee in sight (RANGED_HOLD tried).
- respawn waits stand WAIT_RANGE 2 off a group whose camps are out of sight (bushes): a woken hidden elk hit Willow
  "respawning in 9" for 5 rounds (WAIT_OUTSIDE tried).

## Results (vs v144v, 420 games each)
| Variant | Record | Banked | Per seed | camp deaths |
|---|---|---|---|---|
| all three | 177-231-12 | -5.1% | -6.6 / -0.7 / -7.9 | 253 vs 288 |
| **AWAKE_READ only (v145)** | **215-193-12** | **+0.9%** | -0.0 / -1.3 / +4.0 | **243 vs 277** |
| RANGED_HOLD only | 214-196-10 | +1.7% | +0.3 / +4.3 / +0.5 | 290 vs 271 |
| WAIT_OUTSIDE only | 200-208-12 | +0.7% | +3.8 / +5.7 / -7.1 | 257 vs 240 |
| AWAKE_READ + RANGED_HOLD | 213-195-12 | +0.2% | -1.1 / -2.2 / +4.1 | 242 vs 273 |
Fuel (AWAKE_READ, all but 01boss-battle): +1.0% a turn, out-of-fuel 7759 vs 7574.
