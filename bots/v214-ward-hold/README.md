# v214-ward-hold (v213 + probe + WARD_HOLD): KEEP the catch-up-only hold (WARD_HOLD_BUSH=false, ROUNDS 10)

W2 of `WARD_PLAN.md`. This folder is v213 with the zz-wardprobe log lines (`probe.patch`) and three changes in
`scout_ward` (warding.cpp). Its defaults are the recommended setting, the same as `bots/v214-ward-hold-hb10`.

- **WARD_HOLD, WARD_HOLD_ROUNDS, WARD_HOLD_RANGE** (constants.h, next to SCOUT_REFRESH).
  - Every call notes an enemy champion within WARD_HOLD_RANGE (10, Chebyshev) of us in `last_near_enemy_round`
    (bot.h). It reads the cached `seen::positions(Team::enemy)` before the cooldown check, so the window also covers
    rounds while the charge recharges.
  - With the charge ready, the hold is on when `mode == FIGHT`, an enemy is near now, or
    `turn_round - last_near_enemy_round < WARD_HOLD_ROUNDS`.
- **WARD_HOLD_BUSH** (added after the first sweep).
  - true is the plan's design: the hold returns false, so no scout ward at all.
  - false holds only the catch-up ward (`behind` is forced false). A look into a hidden bush within reach still goes
    ahead.
- `ghost_ward` and `answer_hidden` are unchanged.
- WARD_HOLD=false compiles the new code out. The base-rule check was skipped at the user's request. A normal-fuel run
  vs v210 matched the probe baseline in 119 of 140 games. The off build costs +3 fuel a turn, even in the draft. That
  is code layout from the LOG-only probe changes, and it reshuffles fuel timing.
- Probe additions (all inside LOG):
  - `place scout-...` lines gain `near=0/1 fight=0/1 ago=N`. `ago` is rounds since an enemy was within 10, tracked
    at turn start in `probe::last_near`.
  - A new line, `held fight= near= ago=`, prints on each ready turn the hold blocked.
  - `wp_hold.py` (this folder) splits scout placements by those fields.

## Folders

| Folder | Setting |
|---|---|
| v214-ward-hold-off | WARD_HOLD=false |
| v214-ward-hold-r0 / v214-ward-hold-r10 | full hold (bush too), ROUNDS 0 / 10 |
| v214-ward-hold (sweep run) | full hold, ROUNDS 5 |
| v214-ward-hold-hb5, -hb10 (+ -s1, -s2) | catch-up-only hold, ROUNDS 5 / 10 |
| v214-ward-hold-hb10-r16 (+ -s1, -s2) | hb10 + SCOUT_REFRESH 16 (W1's kept setting); built, **not benched** |

The r0, r10 and off copies predate the WARD_HOLD_BUSH flag. Their code is the same as WARD_HOLD_BUSH=true. The
v214-ward-hold results below are from the full-hold, ROUNDS 5 build, run before this folder was set to hb10.

Replays: `replays/bward-hold-<opponent>/<bot>/`. Baseline: `replays/bwardbase-<opponent>/zz-wardprobe{,-s1,-s2}`.

## What the plan's hold gives up (probe, WARD_HOLD off, 140 games vs v210)

| Scout placement when... | Count | Revealed an enemy | Ward-needed hits credited |
|---|---|---|---|
| bush, FIGHT | 2,212 | 443 (20%) | 561 |
| bush, enemy within 10 | 1,419 | 155 (11%) | 272 |
| bush, enemy 1-4 rounds ago | 1,047 | 218 | 107 |
| bush, enemy 5-9 rounds ago | 643 | 54 | 41 |
| bush, clear | 8,789 | 326 (3.7%) | 340 |
| catch-up, FIGHT or near | 4,623 | 74 | 96 |
| catch-up, otherwise | 18,352 | 74 | 65 |

Scout-bush wards placed in a fight are the best scout wards there are. They are 26% of scout-bush placements, but
they earn 63% of its ward-needed hits (833 of 1,321) and 43% of all of them (1,921). The full hold throws these away.
The catch-up ward in a fight is nearly worthless (0.02 hits each), so the catch-up-only hold keeps that charge at
almost no cost.

## Bench

Records use `bots/zz-wardprobe/split_summary.py` and are given as wins-losses-draws.

**Seed 0, the full hold (bush too), v213 + v210 + v208 (420 games each):**

| Build | All | Win % | Prism (138) | No Prism (282) | No-Prism gems lost to kills (ours vs theirs) | K/D |
|---|---|---|---|---|---|---|
| baseline | 202-217-1 | 48.1% | 73-65 (52.9%) | 129-152 (45.7%) | 25,464 vs 24,233 | 3,703/4,052 |
| full, ROUNDS 0 | 193-221-6 | 46.0% | 67-65 (48.6%) | 126-156 (44.7%) | 29,701 vs 24,463 | 3,311/4,311 |
| full, ROUNDS 5 | 192-225-3 | 45.7% | 63-72 (45.7%) | 129-153 (45.7%) | 28,338 vs 30,379 | 3,668/4,317 |
| full, ROUNDS 10 | 197-220-3 | 46.9% | 76-59 (55.1%) | 121-161 (42.9%) | 32,739 vs 28,537 | 3,376/4,522 |

All three settings lost. Deaths rose 6-12%, and more of our gems went to enemy kills. **Dropped.**

**Catch-up-only hold, v213 + v210 x seeds 0-2 (840 games, complete):**

| Build | All | Win % | Banked | Prism (276) | No Prism (564) | No-Prism banked / collected | Gems lost to kills, Prism / no-Prism (ours vs theirs) | K/D |
|---|---|---|---|---|---|---|---|---|
| baseline | 421-418-1 | 50.1% | +1.0% | 148-128 (53.6%) | 273-290 (48.4%) | +0.9% / +0.5% | 52,247 vs 58,423 / 53,508 vs 51,380 | 7,489/7,875 |
| hb5 | 442-382-16 | 52.6% | +4.6% | 147-123 (53.3%) | 295-259 (52.3%) | +3.6% / +3.0% | 39,289 vs 44,301 / 47,391 vs 51,552 | 7,545/8,164 |
| **hb10** | **464-370-6** | **55.2%** | +12.5% | 172-98 (62.3%) | 292-272 (51.8%) | +7.6% / +6.7% | 43,680 vs 57,288 / 50,477 vs 46,678 | 7,698/7,670 |

hb10 is up on every seed (all games, v213 + v210):

| Seed | hb10 | Baseline |
|---|---|---|
| 0 | 159-119 | 137-142 |
| 1 | 154-124 | 143-137 |
| 2 | 151-127 | 141-139 |

- hb10 vs v208 at seed 0 (complete): 82-57-1 against 65-75. Prism games went 32-13 against 24-22, the others 50-44
  against 41-53.
- hb10 vs v208 at seed 1 (partial, 90 of 140 games, compared on the same 90 maps and sides): 50-40 against 44-46.
- hb5 has no v208 games at seeds 1-2.
- Prism games don't show W1's cohesion failure: the hold applies only near enemies, so catch-up wards near allies
  remain elsewhere.

**Fuel** (`bots/v114/fuel_bench.py`, our turns; the seed copies burn 300N a turn on both sides):

| Build | Mean fuel per turn | Out-of-fuel turns |
|---|---|---|
| baseline, v213 + v210 x 3 seeds | 38,984 | 12,214 |
| hb5, same | 39,136 | 13,354 |
| hb10, same | 39,337 (+0.9%) | 12,991 (+6.4%) |
| baseline, seed 0 x 3 opponents | 36,267 | 4,933 |
| hb10, same | 36,672 (+1.1%) | 5,121 |
| full hold, ROUNDS 10, same | 35,663 | 4,209 |

The out-of-fuel rise is mostly on 00lanes. A probable cause: with catch-up held, more ready turns reach scout-bush,
and scout refusals rose from 36.5k to 40.5k at seed 0. W3's camp-tile mask would cut those.

## Ward metrics (wp_analyze, v213 + v210, seeds 1-2, 560 games)

| | Baseline | hb5 | hb10 |
|---|---|---|---|
| Charge ready (share of alive turns) | 0.51 | 0.56 | 0.57 |
| Catch-up placements | 89,950 | 76,447 | 71,383 |
| Scout-bush placements | 56,375 | 57,570 | 55,877 |
| Ghost placements | 1,701 | 1,810 | 1,940 |
| Ghost episodes blocked at start | 12,407 of 13,379 (93%) | 8,781 of 9,948 (88%) | 8,728 of 9,971 (88%) |
| Hidden-attacker turns with no charge | 1,304 | 1,023 | 1,000 |
| Champion hits needing the ward | 7,224 | 7,483 | 7,399 |
| Catch-up's share of charges spent before a blocked ghost | 5,083 | 1,347 | 1,079 |

The full hold (seed 0, three opponents, against the baseline's 1,200 ghost placements, 92% blocked and 5,701
ward-needed hits) moved the mechanism much further:
- ghost placements 1,774 / 2,412 / 2,249 for ROUNDS 0 / 5 / 10;
- ghost chases blocked 80% / 71% / 68%;
- but ward-needed hits fell to 4,461 / 4,881 / 4,439, because fight-time bush reveals were lost.

The hb mechanism is smaller (ghost placements +14%, hidden blocks -23%, ward-needed hits +2-4%) than its win gain.
Part of the gain is unexplained, so treat the size of the effect with care.

## Not tested

- hb10 + SCOUT_REFRESH 16 (stack on W1's kept setting). The build exists; the run was stopped before it started.
- hb10 vs v208 at seeds 1 (50 games) and 2. hb5 vs v208 at seeds 1-2.
- ROUNDS above 10; WARD_HOLD_RANGE other than 10; Willow exempt; no hold in RETREAT.
- The base-rule check at unlimited fuel (skipped at the user's request).
