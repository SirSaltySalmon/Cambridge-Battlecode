# v90

The starting copy (v82 + the v86 zone, phase, split, band, finish and mage-cheese code) + the fix for "bot error: Unit
is not in vision.", and an analysis script, `analyze_errors.py`.

## The vision error

Every one of these errors (both bots, 34 games) came from two log notes in the v81 mage code (`kits.cpp`). Each one
read the unit right after the spell, so a spell that killed it threw. That ended the rest of the turn and dropped the
turn's log:

| branch (where the error came from) | v90 before | v82 |
| --- | --- | --- |
| `fight.hit.cast`: `mage_fight()` note read the enemy's position after the cast | 123 | 114 |
| `farm/retreat/mono.camp.standoff`: `mage_camp()` note read the camp's health after the cast | 77 | 108 |
| total | 200 (5.9 a game) | 222 (6.5 a game) |

Fixes:
- Both notes now sit in `debug_block` and read the unit only after `alive()` says it is still there. In release builds
  (the platform) they are skipped entirely, so the string building costs no fuel either.
- `MAGE_KILL_FOLLOWUP`: when a mage's fight spell kills its target and the mage stands outside enemy reach, the unspent
  basic attack goes to the next living enemy in reach (before, a kill ended the turn). It fired only 3 times in 34
  games.

## Results (34 games vs v82, normal fuel, debug build, `analyze_errors.py v90 v82 DIR`)

| | record | gems | vision errors | mean fuel a turn | p99 | turns >= 90k | out-of-fuel turns (`fuelExhausted` events) | games with any |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| v90 | 18-16 | 26,674 vs 24,338 | 0 | 32.2k | 79.2k | 0.37% | 180 (258 turns at the limit in the `.fuel` log) | 33 |
| v90 before the fix | 18-16 | 26,614 vs 24,262 | 200 | 32.2k | 79.7k | 0.38% | 181 (259) | 33 |
| v82 (in the v90 games) | 16-18 | | 216 | 36.9k | 85.2k | 0.64% | 241 (405) | 34 |

- The fix removes every vision error and changes nothing else measurable: record, gems and fuel match the unfixed copy.
  The error only struck after the turn's spell, so little was lost each time.
- Fuel vs v82: about 13% less a turn, and out-of-fuel turns about 25% fewer (events) or 36% fewer (turns at the
  limit). This comes from the starting copy (the unfixed copy has the same numbers), not from the fix. Both bots still
  run out somewhere in nearly every game, mostly on contest, lanes and swirl (v90: contest-a 27, lanes-a 21, swirl-b 15).
- `analyze_errors.py` reads per-turn fuel from each replay's `team-<a|b>.fuel` (13-byte header, eight u64 limits, the
  draft turns, the 200k first turns, then one u64 per champion turn; 100,000 means the turn ran out). The `.fuel` log
  counts more turns at the limit than the stats JSON has `fuelExhausted` events; both counts are listed.
- Result files: `results-v90-vs-v82.txt`, `results-v90-pre-vs-v82.txt` (repo root).

# Base: v82

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
