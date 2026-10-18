# v85

`v83` (the ladder bot) + Willow improvements (switches in `constants.h`). Not submitted.

Why: Willow died ~3.1 times a game (719 of 724 deaths to champions), mostly while the team retreated or while she
dodged at low health, and she cast something on only 9% of her turns: her heal ran only on attack turns.

- A `WILLOW_HEAL_STEP`: heal at the start of every turn (her heal allows walking), before recall/dodge/retreat/
  escort/farm. Fuel: cached vision is checked first (a hurt ally within 4 tiles), and only then is the ability read.
- B `WILLOW_HEAL_PICK`: heal the ally who gains most: effective heal (missing health, at most 3 x Focus), doubled for
  an ally under 50% health or with an enemy champion within 5 tiles. Any hurt ally can still be healed.
- C `WILLOW_DODGE`: under recall health and in reach, Willow steps out of reach toward the nearest visible ally.
- E priority: Willow acts third (after Ren and Cassi) instead of last, so her heal and retreat step come before more
  enemy champions have acted.

## Results (68 games = 34 normal + 34 unlimited fuel)

| Opponent | v85 | gems | v83 on the same test |
| --- | --- | --- | --- |
| v83 | 36-32 (18-16 both fuel modes) | +8.3% | - |
| brawler | 31-37 | -10.6% | 33-35 (-2.0%) |

Willow per game (vs v83): healing 9,485 vs 8,247 (+15%), deaths 3.04 vs 3.21; whole team deaths 1,054 vs 1,167 (-10%).
Vs the brawler: Willow healing 9,422 vs the brawler's Willow 7,762 (+21%), deaths equal (2.88).

## Fuel (unlimited fuel, all 17 maps x2)
v85 mean 46.4k, p99 94.6k, 0.61% of turns over 100k (v81/v83 profile: 46.0k / 93.5k / 0.53%). Willow's own turns
average 46.9k, the same as everyone else's: the heal step is cheap.
