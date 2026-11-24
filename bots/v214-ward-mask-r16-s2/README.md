# v214-ward-mask (v213 + probe + SCOUT_MASK): KEEP (SCOUT_MASK = 2)

W3 of `WARD_PLAN.md`. This folder is v213 with the zz-wardprobe log lines (`probe.patch`), extra probe lines that
classify scout refusals and throws, and one new flag. The default here is the recommended `SCOUT_MASK = 2`.

- **SCOUT_MASK** (constants.h, next to SCOUT_REFRESH; int, `0` = v213).
  - At `0`, `scout_ward` (warding.cpp) is v213's, plus the probe lines.
  - At `1` or more, `scout_ward` hands over to `scout_ward_mask`, which is the same function with three changes:
    - **Refused tiles are remembered.** Each `place_ward` refusal sets that tile on a per-champion board
      (a file static in warding.cpp). Those tiles leave the candidates until `SCOUT_MASK_ROUNDS` (20) rounds after
      the latest refusal; then the board is cleared. Cost: one board op on scout turns while the board is live.
    - **Altars and the enemy spawn are masked too.** It uses `seen::fixed_blocks()` instead of `seen::terrain()`.
      This fixes the "Forbidden ward tile" throws: catch-up picks on an altar or the enemy spawn.
    - **At `2`, one same-turn retry.** After a refusal it tries the next-best candidate, but only if that is another
      hidden bush tile or we are behind (a catch-up). That is exactly when v213 would have placed at all.
- `SCOUT_MASK_ROUNDS = 20`. Not swept.

## Step 1: what the refused tiles are (no behaviour change)

140 games vs v210, seed 0, flag off, with extra probe lines (replays/bward-mask-off-v210). On each refusal, after
`seen::reset()`, the probe asked what was revealed on the tile. On each throw, it logged the engine message.
Tool: `python -I bots/v214-ward-mask/mask_analyze.py DIR`.

| Refusals: 12,114 (86.5 a game; 72% from a tile refused 5+ times by the same champion) | Count | Share |
|---|---|---|
| An allied ward stood on the tile (revealed) | 12,114 | **100%** |
| ...with an allied champion standing on it too | 984 | 8.1% |
| An enemy champion was revealed on the tile | **0** | 0% |
| The tile is a camp spawn tile (all of these also held an ally ward) | 49 | 0.4% |
| An enemy ward was also on the tile | 690 | 5.7% |
| Bush pick (hidden bush tile) / catch-up pick | 11,913 / 201 | 98.3% / 1.7% |

**The plan's guess (camps in bushes) is wrong.** Every refusal was an ally's ward hidden in a bush we are not in.
- Allies score bush tiles the same way, so they pick the same tile.
- `vision::ward_tiles(ally)` only returns wards we can see, and the engine refuses a ward on a tile that holds one.
- The revealed ward is visible only for that round, so v213 retried the same tile on every ready turn.
- No refusal ever revealed an enemy champion, so masking refused tiles costs no reveal.
- A camp-spawn mask (the plan's design) would have removed 0.4% of refusals.

| Throws (`bc::error` in `scout_ward`): 368 (2.6 a game) | Count |
|---|---|
| engine error 63: Champion cannot act (stunned or channelling when scout_ward runs) | 222 |
| engine error 64: Forbidden ward tile (catch-up picks on an altar or the enemy spawn: a candidate-mask bug) | 142 |
| engine error 117: An allied ward already occupies this tile | 4 |

- The camp-in-sight case the plan's open question asked about doesn't happen: 1 throw had a camp on the tile.
- Masking visible champions and camps isn't worth an op: 2 throws had a visible enemy champion on the tile.
- The 63 throws are left alone. Gating them would cost a state read (~100 fuel) on every scout turn, against about
  1 throw a game.

## Bench (3 seeds x v213 / v210 / v208, all 70 maps, both sides: 1,260 games per side)

- Baseline: bots/zz-wardprobe{,-s1,-s2} (replays/bwardbase-*).
- Variant: v214-ward-mask-m2{,-s1,-s2} (replays/bward-mask-m2-*).
- Tool: `bots/zz-wardprobe/split_summary.py`. Record is the primary metric.

| Split | Build | Record | Win % | Banked | Collected | Gems lost to kills (ours vs theirs) | K/D |
|---|---|---|---|---|---|---|---|
| all (1,260) | baseline | 624-635-1 | 49.5% | -0.4% | | | 11,402/11,864 |
| all (1,260) | SCOUT_MASK=2 | **670-590-0** | **53.2%** | +9.3% | | | 12,222/12,229 |
| Prism (414) | baseline | 223-191-0 | 53.9% | +2.0% | +2.0% | 83,587 vs 89,762 | 2,766/2,730 |
| Prism (414) | SCOUT_MASK=2 | 233-181-0 | 56.3% | +14.9% | +5.8% | 90,210 vs 91,674 | 3,120/2,946 |
| no Prism (846) | baseline | 401-444-1 | 47.4% | -1.6% | -1.4% | 79,057 vs 76,506 | 8,636/9,134 |
| no Prism (846) | SCOUT_MASK=2 | 437-409-0 | 51.7% | +6.5% | +4.0% | 82,573 vs 77,468 | 9,102/9,283 |

- **+46 wins of 1,260 (+3.7 points)**. Prism games +2.4 points, non-Prism +4.3.
- By opponent (wins of 420, baseline to variant):
  - v213: 202 to 227 (Prism 70 to 79, non-Prism 132 to 148);
  - v210: 219 to 217 (78 to 77, 141 to 140);
  - v208: 203 to 226 (75 to 77, 128 to 149).
- Rough noise: SE of a 1,260-game win rate is about 1.4 points, so the difference is about 1.8 SE. The gain is
  carried by v213 and v208; v210 is level.
- Non-Prism banked by round, us vs them (`gold_noprism.py`, analyze_gold's `banked_at`):

  | Build | By r300 | By r600 |
  |---|---|---|
  | baseline | -5.3% | -6.3% |
  | SCOUT_MASK=2 | -3.0% | -4.1% |

- Camp deaths 924 vs 1,021 (-9.5%). Enemy K/D against us: 11,305/13,344 vs 10,843/12,384.
- **Fuel** (bots/v114/fuel_bench.py, pooled):

  | Build | Mean per turn | Median | p99 | Turns at 90k+ | Out-of-fuel turns |
  |---|---|---|---|---|---|
  | baseline | 38,963 | 38,178 | 85,902 | 45,195 | 18,129 |
  | SCOUT_MASK=2 | 38,827 (-0.3%) | 38,056 | 85,735 | 45,489 | 18,296 (+0.9%) |

  - bench.py summary (stats JSON) gives out-of-fuel 20,906 vs 20,740 (+0.8%).
  - Level: the saved refusals (an engine call plus a vision-cache drop each, about 53 a game) are small next to a
    38k turn. The variant also plays 1.5% more champion turns, so it has 1.5% more chances to run out.

## Ward metrics (wp_analyze, pooled over 1,260 games)

| Metric | Baseline | SCOUT_MASK=2 |
|---|---|---|
| Scout refusals | 107,560 (85.4 a game) | **41,228 (32.7 a game), -62%** |
| Share from a tile refused 5+ times (top repeat) | 72% (208) | 16% (17) |
| Distinct (game, champion, tile) refusals | 22,954 | 25,222 |
| Throws in scout_ward | ~2.6 a game (step 1) | 1.16 a game, all "cannot act" / ally ward |
| scout-bush placements | 126,879 | 137,041 (+8%) |
| scout-bush reveals at once | 10,917 (8.6%) | 12,272 (9.0%) |
| scout-catch placements | 204,476 | 205,706 |
| Charge ready, share of alive turns | 0.51 | 0.50 |
| Ghost episodes blocked at start | 92.6% (27,987/30,211) | 92.6% (29,667/32,041) |
| Ghost placements | 3,795 | 3,829 |
| Hidden-attacker turns with no charge | 2,913 | 2,912 |
| Hidden placements | 864 | 668 |
| Champion hits / ward-needed | 256,794 / 16,764 (6.5%) | 264,156 / 17,685 (6.7%) |
| Ward-needed hits after a scout-bush ward | 11,499 | 12,622 (+10%) |

- **Refusals fell 62%, not the 70%+ the plan hoped for.** The repeats are gone (5+ share 72% to 16%). What's left
  is mostly first refusals on new tiles (distinct tiles even rose, since champions now move on to other bushes).
  Those can't be predicted: an ally's bush ward is invisible to us until we bump into it.
- The win comes through scout-bush wards. The retry and the mask place a ward in the next tile of the same bush (a
  bush seen from inside is seen whole), so 8% more bush wards land, 12% more reveal an enemy at once, and 10% more
  hits follow a bush ward.
- The plan's charge risk didn't show up. Charge-ready share (0.51 to 0.50) and the ghost-blocked share (92.6%) are
  unchanged. A spent charge was already the normal state.

## Seed-0 vs v210 pilot (old build: retry could fall back to a non-bush tile)

The first m2 build retried onto *any* next-best tile. That gave 1,096 "scout-other" placements (gain 8, 2.5%
reveal) per 140 games. The retry is now limited to hidden-bush or catch-up tiles; every number above is from the
fixed build.

| Build (140 games vs v210, seed 0) | Record | Win % | Banked | Refusals |
|---|---|---|---|---|
| baseline | 71-68-1 | 50.7% | +3.7% | 12,114 (flag-off probe run) |
| SCOUT_MASK=1 | 66-74-0 | 47.1% | +0.4% | 4,348 |
| SCOUT_MASK=2 (old retry) | 76-64-0 | 54.3% | +12.1% | 4,371 |

M1_RESULTS_PLACEHOLDER

## Base-rule check

- **Normal fuel, 40 games vs v210** (20 maps, both sides, seed 0), flag off against the probe baseline:
  - 39 of 40 identical in winner and gems; 40 of 40 had the same winner.
  - The one difference (00lanes-a, the map with the most out-of-fuel turns) came from fuel timing. Per-turn fuel was
    identical in play but about 9 higher on draft turns, from initialising the mask board as a Bot member.
  - The board was then moved to a file static (constant-initialised pointer), so the flag-off build does no extra
    work. That last build was not re-benched.
- **An earlier flag-off build differed in 27 of 140 games.** Its only differences from the probe were extra
  log-only lines (inside `LOG`). So the content of a `debug_block` is not play-neutral at normal fuel: it shifts
  heap state and fuel timing. A base check has to use log lines identical to the baseline's.
- **Unlimited fuel** (`--fuel-quantum 100000000`): skipped at the user's request.

## Stacking

- W1 (v214-ward-catchup, SCOUT_CATCH): its README recommends **drop**, so nothing was stacked on it.
- W2 (v214-ward-hold): STACK_STATUS_PLACEHOLDER

## Tried and dropped / open

- **Camp-spawn mask (the plan's design):** not built. Step 1 showed 0.4% of refused tiles are camp spawn tiles, all
  of them with an ally ward on them.
- **SCOUT_MASK=2 with the retry onto any tile:** fixed (see the pilot).
- **Masking visible champions and camps:** not needed (2-3 throws per 1,260 games).
- **Open:**
  - `SCOUT_MASK_ROUNDS` sweep (20 not tuned).
  - Spreading allies over different bush tiles, so they stop picking the same tile. This would need knowing allies'
    picks, which the shared bytes can't carry.

## Files

- `mask_analyze.py`: refusal and throw classification from the probe lines `scout refused X Y k=v...` and
  `throw scout X Y k=v... why=...`.
- `gold_noprism.py`: banked by r300 / r600 on non-Prism games.
- `same_games.py`: game-for-game winner and gems comparison of two bench dirs.
