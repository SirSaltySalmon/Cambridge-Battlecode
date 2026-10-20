# v96

`v95` with lower gold weights and the patched cluster health, so the farm route favours gems.

## Changes

- **Cluster health** (`draft.cpp` `camp_stats`): gem and gold clusters have 120 health in the balance patch (was
  500). The route's clear-time estimate (`clear_rounds`) and the draft's pair check read it. Camp rewards did not
  change in the patch.
- **Gold weights** (`draft.h`): `FARM_GOLD_WEIGHT` 1.5 -> 0.6 (before the aggressive route), `GOLD_WEIGHT` 0.4 -> 0.1
  (aggressive route, most of the game). At 1.5 a wolf was worth 64 gems in planning (4 gems + 60), so gold drove the
  route.
- `v95` now builds against SDK 3.0.1 unchanged (the golem and Heart of Ruin enums fall to the `default` cases, and dash
  goes through the generic ability lookup).

## Results (each vs v95, 42 games: 21 maps x both sides; `analyze_gold.py`, `results-v96-vs-v95.txt`)

rNNN = gems banked by round NNN, summed, vs v95 in the same games.

| variant | farm / agg weight | cluster hp | record | banked | r300 | r600 | r900 | gold earned |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| gw1.0 | 1.0 / 0.4 | 500 | 23-19 | +6% | +1% | 0% | +5% | +5% |
| gw0.6 | 0.6 / 0.4 | 500 | 22-20 | +6% | -6% | -5% | -3% | +3% |
| gw0.3 | 0.3 / 0.4 | 500 | 18-24 | 0% | +10% | -1% | +4% | -4% |
| gw0.1 | 0.1 / 0.4 | 500 | 21-21 | 0% | -1% | +3% | +5% | -3% |
| agg0.1 | 1.5 / 0.1 | 500 | 21-21 | +2% | -2% | +1% | -7% | -3% |
| hp | 1.5 / 0.4 | 120 | 22-20 | -1% | -7% | -2% | 0% | -4% |
| m2 | 0.6 / 0.2 | 120 | 22-20 | +7% | +13% | -1% | +3% | -5% |
| **both (= v96)** | **0.6 / 0.1** | **120** | **24-18** | **+17%** | **+12%** | **+4%** | **+13%** | -2% |
| m4 | 0.6 / 0.0 | 120 | 25-17 | +12% | +18% | +7% | +7% | -10% |
| m1 | 0.3 / 0.1 | 120 | 21-21 | +2% | +20% | +17% | +9% | -10% |
| m3 | 0.0 / 0.1 | 120 | 24-18 | +13% | +8% | +7% | +7% | -3% |
| gem | 0.0 / 0.0 | 120 | 21-21 | +3% | +5% | +20% | +15% | -12% |

- Neither change does anything alone: lowering only the farm weight, only the aggressive weight, or only fixing
  cluster health stays in the noise. Together (health fixed and aggressive weight <= 0.1) every variant banks more by
  every checkpoint, 115-95 pooled over the five.
- v96 (0.6 / 0.1) has the best banked total and keeps the most gold (-2%); m4 (0.6 / 0.0) is level with it.
- Records alone are within noise (+-2 games): the same ~15 games are lost by every variant, decided by map side
  (e.g. 01hooks-b 0-1012, 01elk-arena-a 0-1008, 00elkring-a 638-1012, 00fragment-b, 01concentrated-wealth-a, where
  v95 loses to itself from that side).

## Follow-ups

- The side-decided losses above bank 0-60 gems: farming fails structurally on those maps from one side.
- `GEMLESS_VALUE` (0.25) and `GOLD_ONLY_NEAR` (2) were set when clusters took 500 health to clear.