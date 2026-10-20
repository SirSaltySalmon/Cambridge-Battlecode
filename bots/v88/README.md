# v88

`v87` + `FARM_GOLD_WEIGHT` = 0.4 (was 1.5) in `src/draft.h`. Submitted as platform version 88.

## Why
On the ladder (v87's 80 games) both teams make about the same number of captures a game (27.0 against 26.2), but
the opponents' are worth more: 32.8 camp gems a capture against our 30.6. Our normal route weighs gold at 1.5x a gem
(elk 36 + 1.5 x 160 = 276, wolf 157, gem cluster 24), so it prefers gold-rich groups early; the aggressive route
already uses 0.4. The win condition is gems.

## Results (34 games each)

| Bot | vs v82 | vs v82 (unlimited) | vs v87 (unlimited) |
| --- | --- | --- | --- |
| v88 | 20-14 (+12.6%) | 20-14 (+14.9%) | 18-16 (-0.4%) |
| v87 | 18-16 (+9.7%) | 23-11 (+23.2%) | |
| 0.8 instead | 20-14 (+12.4%) | 20-14 (+17.6%) | 18-16 (+2.8%) |

Neutral locally; the ladder decides.

## Ladder: rejected
33-47 on the same 16 groups (v87 36-44): camp gems 807 a game against v87's 825, and down at rounds 200 and 400 too
(223 / 445 against 236 / 471). Gold buys the emblems that win fights; the gem-richer groups are not worth it. v87 was
resubmitted as the live bot (platform version 89 = bots\v87). Details: `results/results-v88-ladder.txt`.
