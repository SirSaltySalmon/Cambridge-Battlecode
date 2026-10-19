# v82

`v81` + recall caution (`RECALL_SPELL_CAUTION`, `RECALL_REACH_MODE`) and the champion id on each log turn line.

## Recall caution (68 games each: 34 normal + 34 unlimited fuel, `analyze_recalls.py`)

| | record vs v80-5 | gems | recalls broken | died mid-recall | turns held by an enemy's reach | cancelled by us |
| --- | --- | --- | --- | --- | --- | --- |
| caution off | 35-33 | +3.4% | 87 / 1,374 (6.3%) | 32 | 0 | 123 |
| mode 0: casters by name | 39-29 | +6.9% | 16 / 1,395 (1.1%) | 14 | 2,368 | 234 |
| mode 1: kit, all ready | 40-28 | +9.7% | 17 / 1,464 (1.2%) | 15 | 2,540 | 278 |
| mode 2: kit, live cooldowns (v82) | 37-31 | +5.8% | 14 / 1,478 (0.9%) | 14 | 1,645 | 299 |

- Caster caution cuts broken recalls ~83% and deaths mid-recall ~55%.
- Dashes barely matter: with casters covered almost nothing breaks, and planning for dashes (mode 1) or not (mode 0,
  which still cancels once an enemy is close) gives the same rate.
- Live cooldowns hold recalls back 30% less often than mode 0 at the same safety, so v82 uses mode 2. Records and gems
  are within noise (about +-2 games, +-9% gems per 34 games).
