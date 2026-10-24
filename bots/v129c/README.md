# v128

v123-salmon's fight score as benched best (README of v123-salmon, benches 5-7): FIGHT_LOCAL 12 tiles,
FIGHT_ENEMY_WEIGH on, HEALTH_EDGE 2; FIGHT_ALLY_WEIGH and FIGHT_COMMIT_PING off. New: PING_IDLE_ONLY (off).

## PING_IDLE_ONLY
Only a free ally answers a ping: not the reward's collector, no awake or hit camp within its attack range + 1; a
collector no longer gives its claim back for a ping. Applies to every ping (the byte does not say which kind).

## Screen (`replays/tune4`, seed 0, vs v122 + v120, 40 maps both sides = 160 games each)

| variant                                   | wins | banked | K/D       | teamfights |
|-------------------------------------------|------|--------|-----------|------------|
| defaults (enemy weighting, HEALTH_EDGE 2) | 93   | +21.6% | 3211/2275 | 613-344    |
| + ally weighting                          | 81   | +2.4%  | 2795/2629 | 487-475    |
| + ally weighting, FIGHT_THRESHOLD 0       | 75   | 0.0%   | 2992/2559 | 514-450    |
| + ally weighting, MISSING_BODY_PENALTY 3  | 84   | +6.3%  | 2861/2502 | 498-451    |
| + ally weighting, OUTNUMBER_BONUS 15      | 91   | +10.1% | 2930/2485 | 515-425    |
| HEALTH_EDGE 1                             | 78   | -1.7%  | 2782/2616 | 509-498    |
| HEALTH_EDGE 0                             | 79   | +3.0%  | 2824/2614 | 503-468    |
| GRADED_EDGES, EDGE_WEIGHT 5               | 81   | +4.4%  | 2874/2517 | 539-439    |
| FIGHT_THRESHOLD 10                        | 83   | +9.1%  | 2767/2278 | 516-345    |
| commit ping + PING_IDLE_ONLY              | 86   | +10.9% | 2777/2609 | 481-472    |

- Nothing beat the defaults. Making up for ally weighting with a bigger outnumber bonus came closest (91/160) but
  banked half as much.
- The idle-only ping gathers: far allies (9-12 tiles) joined 44% vs 34%, farming through a fight 22.8% vs 29.0%,
  and it no longer costs the farming (the plain commit ping: -12.1%). The team still wins fewer teamfights.
- HEALTH_EDGE: 5 = 83/160, 2 = 93, 1 = 78, 0 = 79. A peak that sharp may be partly this seed's luck.
