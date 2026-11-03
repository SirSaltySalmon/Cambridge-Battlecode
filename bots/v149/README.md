# v149

v149 = v148-normal + FIGHT_THRESHOLD 5 -> -5 (`src/constants.h`): the team fights from a lower fight score. Nothing else
changed. The fight score and its terms are in `score_mode` (`src/route.cpp`); once fighting the team stays in until the
score falls FIGHT_HYSTERESIS (10) below the threshold, so the retreat bar moves from -5 to -15 as well.

## Why
The emblem ratio tests (v149-ratio, now in `bots/v151-which-is-v149tuned/README.md`) showed a retreat call is the worst thing the team does: in the 10 rounds after
one, ~0.04 kills and ~0.44 deaths, against ~0.43 kills and ~0.28 deaths after a fight call (debug logs, every bot,
v148-normal included). So the user asked for a sweep of the fight-score parameters toward aggression.

## Results (vs v148-normal, all 70 maps both sides, release, seed 0, 140 games each)
| Change (one at a time on v148-normal) | Record | Banked | Collected | Gold earned | K / D (ours vs theirs) |
|---|---|---|---|---|---|
| **FIGHT_THRESHOLD 5 -> -5 (v149)** | **78-57-5** | **+13.7%** | **+11.0%** | **+11.5%** | 2420/2151 vs 2056/2520 |
| FIGHT_THRESHOLD 5 -> 0 | 77-59-4 | +9.8% | +5.4% | +11.7% | 2504/2246 vs 2157/2586 |
| EMBLEM_BONUS 3 -> 5 | 74-62-4 | +9.4% | +6.3% | +1.6% | 2375/2492 vs 2405/2469 |
| OUTNUMBER_BONUS 10 -> 15 | 71-63-6 | +4.9% | +2.0% | +10.0% | 2374/2167 vs 2080/2461 |
| MISSING_BODY_PENALTY 5 -> 2 | 69-67-4 | +2.2% | +1.1% | -0.6% | 2287/2403 vs 2308/2380 |
| FIGHT_HYSTERESIS 10 -> 20 | 64-71-5 | -7.1% | -6.3% | -4.6% | 2154/2353 vs 2263/2243 |

The sweep was stopped before DISTANCE_PENALTY 0.25, OVEREXTEND_PENALTY 5, POWER_EDGE 2 and EMB_STAY_STRICT off ran.
Only seed 0 so far (one seed swings about 10%); the fuel-burn seeds s1 / s2 and a ladder check are still to do.
The bar is the strongest lever found: lower starts more fights; a wider stay-in band (hysteresis 20) loses.

Replays: `replays/b150sweep/v148n-thrm5` (same code as this folder) and the other `v148n-*` folders; test bots
`bots/v148n-*`.

Fight-score numbers (with the emblem-ratio ones) live in `src/evo_constants.h`, included from `src/constants.h` with the same values; `// evo:` comments give tools/evotune each one's step and range.
