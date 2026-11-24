# v214-ward-catchup (v213 + probe + SCOUT_CATCH)

W1 of `WARD_PLAN.md`. This folder is v213 with the zz-wardprobe log lines (`probe.patch`) and one new flag. Two
settings were benched.

## Recommendation

**Keep SCOUT_REFRESH = 16** (catch-up on, `SCOUT_CATCH = true`), with one condition: stack it with W3 (mask) and watch
fuel. That is v213 plus one changed constant. **Drop SCOUT_CATCH = false.** This folder ships with it false, as
benched. Don't stack W2 or W3 on it.

| Setting | Record | Win % | Prism games | Other games | Out-of-fuel turns |
|---|---|---|---|---|---|
| baseline (zz-wardprobe, = v213) | 624-635-1 | 49.5% | 53.9% | 47.4% | 18,129 |
| a. `SCOUT_CATCH = false` | 563-688-9 | 44.7% | **38.4%** | 47.8% | 20,570 (+13.5%) |
| b. `SCOUT_REFRESH = 16` | **662-587-11** | **52.5%** | 53.6% | **52.0%** | 20,959 (+15.6%) |

Out-of-fuel turns here come from fuel_bench (.fuel files). The bench.py counts are in the per-setting tables below.

## What changed

- **SCOUT_CATCH** (constants.h, next to SCOUT_REFRESH). In `scout_ward` (warding.cpp),
  `behind = SCOUT_CATCH && (own.empty() || distance(position, own.front()) > SCOUT_REFRESH)`.
  - With the flag off, the ward never moves to catch up with us. It moves only into an unseen bush within placement
    reach, and "no bush in reach" returns before the `ward_positions()` read.
  - `SCOUT_CATCH = true` plays as v213.
- **Variant b** is `SCOUT_CATCH = true` with `SCOUT_REFRESH` 8 → 16 (bots/v214-ward-catchup-r16). The ward catches
  up only once it is more than 16 tiles behind, or when we have none.
- **Why.** In the zz-wardprobe baseline, catch-up was 61% of placements. 65% of them added zero tiles beyond own
  sight at placement, 0.6% revealed an enemy, and each gave 0.008 ward-needed hits. They had spent the charge in
  1,286 of the 2,996 ghost chases that found it gone.

## Bench (3 seeds x v213 / v210 / v208, all 70 maps, both sides: 1,260 games per build)

The baseline is bots/zz-wardprobe{,-s1,-s2} (replays/bwardbase-*). Variant a is v214-ward-catchup{,-s1,-s2}
(replays/bward-catchup-*). Variant b is v214-ward-catchup-r16{,-s1,-s2} (replays/bward-catchup-r16-*). The tool is
`bots/zz-wardprobe/split_summary.py`.

| Split | Build | Record | Win % | Banked | Collected | Gems lost to kills (ours vs theirs) | K/D |
|---|---|---|---|---|---|---|---|
| Prism (414) | baseline | 223-191-0 | 53.9% | +2.0% | +2.0% | 83,587 vs 89,762 | 2,766/2,730 |
| Prism (414) | a. off | 159-246-9 | 38.4% | -25.3% | -15.0% | 65,193 vs 54,020 | 1,999/3,227 |
| Prism (414) | b. r16 | 222-190-2 | 53.6% | +9.6% | +7.0% | 82,107 vs 89,800 | 3,161/2,701 |
| no Prism (846) | baseline | 401-444-1 | 47.4% | -1.6% | -1.4% | 79,057 vs 76,506 | 8,636/9,134 |
| no Prism (846) | a. off | 404-442-0 | 47.8% | -4.5% | -3.8% | 72,506 vs 75,781 | 9,283/9,225 |
| no Prism (846) | b. r16 | 440-397-9 | 52.0% | +2.3% | -0.1% | 78,576 vs 82,979 | 9,166/9,386 |

**Non-Prism banked by round** (us vs them, analyze_gold's `banked_at`):

| Build | By r300 | By r600 |
|---|---|---|
| baseline | -5.3% | -6.3% |
| a. off | -3.1% | -5.1% |
| b. r16 | +0.1% | -1.3% |

**b. r16 by opponent**, three seeds pooled:

| Opponent | No-Prism record | Baseline | Prism record | Baseline |
|---|---|---|---|---|
| v213 | 140-139 | 132-150 | 69-68 | 70-68 |
| v210 | 150-129 | 141-140 | 73-65 | 78-60 |
| v208 | 150-129 | 128-154 | 80-57 | 75-63 |

**b. r16 by seed**, all games: 211-206 vs 202-217 (s0), 229-188 vs 215-205 (s1), 222-193 vs 207-213 (s2). Every
seed is up.

**a. off by opponent:**
- The Prism loss holds against every opponent: v213 70-68 down to 52-83, v210 78-60 down to 56-79, v208 75-63 down
  to 51-84.
- The other games are level.

**All games** (bench.py summary):

| Build | Record | Banked | K/D (ours vs theirs) | Camp deaths | Out-of-fuel turns (ours vs theirs) |
|---|---|---|---|---|---|
| baseline | 624-635-1 | -0.4% | 11,402/11,864 vs 10,843/12,384 | 1,021 | 20,740 vs 16,552 |
| a. off | 563-688-9 | -12.1% | 11,282/12,452 vs 11,539/12,137 | 913 | 23,095 vs 17,700 |
| b. r16 | 662-587-11 | +4.8% | 12,327/12,087 vs 11,052/13,328 | 1,035 | 23,569 vs 17,378 |

**Fuel** (bots/v114/fuel_bench.py, our turns):

| Build | Mean per turn | Turns at 90k+ | Out-of-fuel turns |
|---|---|---|---|
| baseline | 38,963 | 45,195 | 18,129 |
| a. off | 39,532 (+1.5%) | 50,251 | 20,570 (+13.5%) |
| b. r16 | 39,533 (+1.5%) | 52,197 | 20,959 (+15.6%) |

- For b, 00lanes alone accounts for 1,813 of the 2,830 extra out-of-fuel turns (3,138 → 4,951). Without 00lanes the
  rise is +6.8% (14,991 → 16,008). 37 maps went up, 24 went down.
- **Why fuel rises.** A spent charge was v213's cheapest exit: `scout_ward`, `ghost_ward` and `answer_hidden` each
  return after one `ward_cooldown()` read. With the charge ready on 69-80% of turns instead of 51%, their full
  bodies run far more often.
- Scout refusals also rose 31-43% (pooled: 107,560 baseline, 141,226 r16). The same occupied bush tiles, mostly
  camps, get retried on every ready turn.
- W3 (mask camp spawn tiles out of `scout_ward`'s candidates) targets exactly that, so **b should be stacked with
  W3**, and the stack checked for out-of-fuel turns.

## Ward metrics (wp_analyze; pooled over 1,260 games unless marked)

| Metric | Baseline | a. off | b. r16 |
|---|---|---|---|
| Charge ready, share of alive turns (v210 s0) | 0.51 | 0.80 | 0.69 |
| Catch-up placements | 204,476 | 0 | 76,360 (-63%) |
| Ghost placements | 3,795 | 4,657 | 4,435 (+17%) |
| Ghost episodes blocked by a spent charge | 92.6% | 84.4% | 87.3% |
| Hidden-attacker turns with no charge | 2,913 | 1,651 | 1,897 (-35%) |
| Champion hits | 256,794 | 254,873 | 263,883 |
| Ward-needed hits | 16,764 | 18,778 | 18,591 (+11%) |
| Scout refusals | 107,560 | 147,967 | 141,226 |

Split by Prism:

| Metric | Prism, base | Prism, off | Prism, r16 | No Prism, base | No Prism, off | No Prism, r16 |
|---|---|---|---|---|---|---|
| Champion hits | 58,346 | **46,415** | 63,001 | 198,448 | 208,458 | 200,882 |
| Ward-needed hits | 4,292 | 3,252 | 4,638 | 12,472 | 15,526 | 13,953 |
| Ghost episodes | 5,878 | 3,503 | 5,106 | 24,333 | 17,392 | 18,424 |
| Catch-up mean gain (tiles) | 5.1 | – | 2.9 | 5.5 | – | 4.8 |

## Why `SCOUT_CATCH = false` loses Prism games (what the probe misses)

With the flag off, the Prism team hit enemy champions 20% less often, killed 28% less and died 18% more.
- Every champion died more: Ryth +131, Marcella +109, Vesper +85, Prism +73, Willow +69, HoR +30.
- Deaths rose in every 300-round bucket up to r900.

The probe measures a catch-up ward only **at placement**: tiles gained and enemies revealed that turn. Two effects
are invisible there:

1. **The ward stays behind as we walk on.** It is placed 2 tiles ahead toward the target, then watches the ground
   behind us until the next catch-up.
2. **It shows allies as well as enemies.** Vision is not shared, so a champion sees allies only through its own
   sight and ward.
   - `seen::champions(Team::ally)` feeds Willow's follow (`willow_follow`: "stay with Prism" when she is seen, else
     walk to the target), `together()` (the centre gate), join and fight-score ally counts, and `PRISM_GUARD`.
   - `last_enemy_round` counts ward sightings at any range. It gates the satellites (`SAT_QUIET`) and the splits
     (`SPLIT_QUIET`).

**Debug pair** (adgato-gemcombat3-a vs v213): flag on won 1029-56, flag off lost.
- The first divergence was at r5, in Willow's follow.
- The flag-off team then fought split up and switched between the "group" and "centre" phases 13 times. The
  flag-on team raided.
- One game is chaotic, but the 414-game split agrees.

Variant b keeps a ward that exists and trails behind us, catching up at 16 tiles. It still frees the charge for
ghost and hidden wards. The Prism games stay level with better K/D, and the rest gain.

## Base-rule check

- **Normal fuel.** `SCOUT_CATCH = true` (bots/v214-ward-catchup-base) against the probe baseline: 20 games vs v213,
  10 maps both sides, seed 0. Identical game for game: winner, gems, rounds, final input and the full stats `events`
  list, fuelExhausted included (idcmp.py).
- **Unlimited fuel.** Skipped at the user's request.

## Not tried / open

- **A SCOUT_REFRESH sweep** (12, 24; and "never catch up except with no ward") to find the best distance. Only 16
  was run.
- **Catch-up kept only when it gains ≥ K tiles.** The plan's open question. Not tried: the gain at placement is the
  wrong measure (above).
- **Catch-up off for non-Prism teams only.** Not tried: those games were level on record with flag off, and b beats
  that.
