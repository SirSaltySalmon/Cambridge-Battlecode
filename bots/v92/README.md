# v92

`v91` with `MAGE_FINISH_SHARE` 0.10 -> 0.20: a mage on an untanked camp basic attacks from inside its reach when finishing
it costs at most 20% of its health (was 10%), instead of standing off.

## Results (34 games vs v91, normal fuel; `analyze_finish.py`, `../v91/analyze_squishy.py`)

| | record | gems | camp groups cleared | camp damage done / game (casters) | mage cheap-finish turns | caster camp damage taken / game | deaths to camps (Cassi) | caster hurt recalls / dodges |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| v92 | 18-16 | 27,552 | 933 | 64,182 (34,175) | 259 (1.8% of mage camp turns) | 1,056 | 31 (12) | 941 |
| v91 | 16-18 | 26,166 | 892 | 61,593 (31,385) | 96 (0.7%) | 1,115 | 22 (8) | 1,151 |

- The finish branch fires 2.7x as often but is still rare (under 2% of mage camp turns), so the effect is small.
- Farming: 4.6% more clears, casters deal 9% more camp damage, gems +5.3%, 18-16. All within noise (+-2 games, ~18% gems).
- Risk: casters take 5% less camp damage (Cassi 283 vs 320, Stellar 294 vs 359 a game; Willow, whom the setting does
  not touch, 626 vs 570) and recall or dodge for health 18% less often. Deaths to camps rose 31 vs 22, but none of v92's
  12 Cassi deaths to camps came on a finish turn (they came standing off on cooldown, walking, or with a tank; Issac dies
  with her, hence the matching 12), and 17 of the 31 were on stampede.
- Fuel unchanged (28.3k vs 28.1k a turn).
- Result file: `results-v92-vs-v91.txt` (repo root).