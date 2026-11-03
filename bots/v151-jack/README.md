# v151-jack

bots/v150-jack (v149 with Optuna study `fight` trial 43) with Optuna study `economy` trial 77 (score 0.7194). Only two
constants differ from v150-jack; the study's other values equal v150-jack's:

| Constant | v150-jack | v151-jack |
|---|---|---|
| GEM_BANK | 40 | 50 |
| IDLE_BANK | 10 | 15 |

Bench (tuner/evaluate.py, 70 maps x 2 sides): 71/140 vs v151-salmon, 76/140 vs v150-jack, 70/140 vs v149, 86/140 vs v148,
95/140 vs v144; fuel mean 33,622 (v150-jack 33,749), 0 faults. The v151-salmon emblem-ratio term was tuned on top of this
bot (study `emblem_ratio`, 60 trials) and every ratio-on trial scored lower (best 0.6575), so it is not included.
