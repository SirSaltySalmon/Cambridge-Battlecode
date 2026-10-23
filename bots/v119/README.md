# v119

`v117` + `JOIN_FIRST` (don't close on an enemy alone) and `CATCH_MODEL` (a dash- and stun-aware catch test); four
other switches tested and left off. Not submitted. Every bench ran in debug mode with logs (`--log-limit 40000000`).

## What the logs showed (debug v117 vs v116, 40 maps; `analyze_spread.py replays/d119base --both`)
1. 46% of our deaths to champions had no ally within 4 tiles the round before (v116: 54%).
2. Fights begun with no ally within 4 died 10% of the time, against 6% begun together; Vesper (7.5% vs 2.8%),
   Marcella (11.7 vs 6.4) and Heart of Ruin (20.3 vs 12.2) worst, Willow unaffected (she keeps back).
3. Lone deaths end in `hurt.dodge` (median 2 rounds, started under the 35% recall bar): already lost by then, so
   the fight start is the lever, not the escape.
4. Nothing waited for allies: the leader's shared fight mode sends each champion at its own nearest enemy, and
   pings only go out after a lone champion has lost 60 health.
5. `chase_line` assumed the enemy stands still. Everyone walks one tile a turn, so only a ready dash or stun closes.

## Changes
1. `JOIN_FIRST` (`combat.cpp` `join_first`, from `fight()`): closing on an enemy out of reach, outside every enemy's
   reach, with no ally in sight within `JOIN_RANGE` (4), a champion walks to the nearest ally (or holds with none in
   sight) for at most `JOIN_WAIT` (3) rounds of the contact. A squishy also waits for a melee ally to be nearer the
   target. Willow is exempt.
2. `CATCH_MODEL` (`catch_short`, `update_team_chase`, `chase_line`): the gap our ready dash and stun leave, plus the
   enemy's ready dash. In the chase ring a gap we cannot close is a drop; the team chase also drops after
   `CATCH_DROP_ROUNDS` (2) rounds where nobody in sight can close and the team gap did not shrink.
3. Off: `JOIN_KEEP_DASH`, `JOIN_ALONE` 1/2, `PING_GATHER` (ping on first sight), `RING_SCALE` (ring = spawn gap / 4).

## Fight-score sweep on cambc 3.0.5 with the strength-only Vesper fallback (`replays/b119d`-`b119k16`)
The fallback `FALLBACK_VESPER = {power_shard_of_strength}` (was strength, strength, health) is what costs: v119 with it
vs v117 went 32-48, 30-50, 31-49 (93/240, banked -17 to -22%); the old fallback reproduces 42-38, 42-38, 40-40 exactly
(3.0.5 changed nothing). Vesper ends with ~24 strength emblems and no health emblems (old: ~20 + 5.5 health), dies
4 times a game instead of 2 and collects ~10% fewer gems.

Sweeping the score against v119 itself misled: HEALTH_EDGE 15 + EMBLEM_BONUS 5 beat v119 47-33, 49-31, 44-36 (seeds
0-2) but went 41-39 / 38-42 on seeds 3-4, and against v117 lost 74/240 (-34%; with the old fallback 96/240). Tuning
against a pool (v117 and v116, 80 games each) instead, no single step beats the current numbers:

| change (current) | vs v117 | vs v116 | banked |
|---|---|---|---|
| **v119 as is** | 32-48 | 32-48 | -14.3% |
| POWER_EDGE 10 (5) | 34-46 | 30-50 | -18.4% |
| EMBLEM_BONUS 5 / 2 (3) | 30-50 / 28-52 | 31-49 / 32-48 | -20.6 / -18.2% |
| FIGHT_THRESHOLD 10 / 0 (5) | 34-46 / 24-56 | 27-53 / 32-48 | -23.4 / -20.9% |
| FIGHT_HYSTERESIS 20 / 5 (10) | 26-54 / 23-57 | 35-45 / 29-51 | -18.9 / -27.6% |
| OVEREXTEND_PENALTY 5 / 20 (10) | 27-53 / 24-56 | 34-46 / 31-49 | -13.9 / -23.6% |
| MISSING_BODY_PENALTY 3 / 8 (5) | 33-47 / 29-51 | 27-53 / 29-51 | -14.9 / -20.6% |
| OUTNUMBER_BONUS 5 / 15 (10) | 30-50 / 22-58 | 30-50 / 28-52 | -20.6 / -23.3% |
| DISTANCE_PENALTY 0.25 / 1.0 (0.5) | 27-53 / 22-58 | 32-48 / 23-57 | -21.6 / -33.2% |
| HEALTH_EDGE 10 / 2 (5) | 27-53 / 24-56 | 30-50 / 31-49 | -20.4 / -25.6% |
| POWER_EDGE 2 (5) | 25-55 | 33-47 | -23.5% |

The fight-score numbers stay as they are; the loss is the Vesper fallback, which no score setting makes up.

## Bench (debug, 40 maps, both sides, vs v117; `replays/b119a`-`b119c`)
| variant | run 1 | run 2 (seed 1) | run 3 (seed 2) | banked | kills | deaths |
|---|---|---|---|---|---|---|
| **JOIN_FIRST + CATCH_MODEL (= v119)** | 42-38 | 42-38 | 40-40 | +5.5 / +6.5 / -1.5% | +12 / +18 / +7% | -9 / -14 / -6% |
| JOIN_FIRST (wait 3) | 42-38 | 40-40 | 40-40 | +3.4 / +2.5 / +5.4% | +15 / +16 / +21% | -11 / -13 / -16% |
| JOIN_FIRST wait 2 | 37-43 | | | -6.8% | -13% | +13% |
| JOIN_FIRST wait 5 | 34-46 | | | -10.8% | -10% | +9% |
| JOIN_FIRST + JOIN_KEEP_DASH | 41-39 | | | -0.5% | +1% | -3% |
| JOIN_FIRST, JOIN_ALONE 1 (close anyway) | 41-39 | | | +0.4% | +6% | -5% |
| JOIN_FIRST, JOIN_ALONE 2 (farm instead) | 40-40 | | | -4.4% | -4% | +5% |
| JOIN_FIRST + PING_GATHER | 41-39 | | | -0.9% | -9% | +10% |
| CATCH_MODEL alone | 39-41 | | | -1.1% | +2% | -1% |
| TEAM_CHASE_ROUNDS 3 | 41-39 | | | -2.3% | -3% | +2% |
| RING_SCALE (/4, 6-14) | 40-40 | | | -2.7% | -6% | +6% |

The record is about one standard deviation over even (124/240); the K/D gain repeats in all six JOIN_FIRST runs at
wait 3. With CATCH_MODEL, closing turns fall from ~9% to 7.5% of turns (more farming). Lone-death share stays ~52%:
fewer deaths overall, but the remaining ones are champions jumped while farming alone, which no closing rule reaches.