# v216-micro: per-tile fight movement for Vesper (fight_coord_plan.md F5)

Base: bots/v213. Flags in `src/constants.h`: `MICRO_VESPER`, `MICRO_APPROACH`, `MICRO_DMG_BAR` 0.2, `MICRO_COVER` 0.5,
`MICRO_DASH_KEEP` 0.5, `MICRO_FUEL` 15000. Code: `src/micro.cpp`, hooked at the top of `engage_vesper` (kits.cpp),
champion targets only. Variants: `-off` (both flags off), `-na` (`MICRO_APPROACH` off). `mkvar.sh` builds variants.

## Why

`deathctx.py` on Chat A's debug baseline (v213 vs v210, seed 0, 172 games; `deathctx-base.txt`):

- Vesper died 219 times to champions.
- 73% of those deaths end in `hurt.dodge`.
- 58% start from >=60% health 5 rounds before.
- 78% have no ally within 3 tiles.
- The replays show her standing in `fight.hit` beside an enemy melee (often stunned) until she drops under the recall
  bar.

Turn order, checked step by step in a replay: champion turns interleave by slot (A1 B1 A2 B2, starting team flips each
round). So each enemy acts about once between two Vesper turns.

## What it does

Each visible enemy gets a threat estimate:

- **Basic hits:** melee reach 2, ranged reach 3. A ready dash adds its range. A ready attack reset doubles the damage.
- **Spells:** the best ready spell, at its range plus radius.
- **Stuns:** a ready stun adds another round of basic hits.
- **Cover:** an enemy with a melee ally of ours strictly closer to it counts half.

Each tile within 2 gets a danger score from those threats. If standing still costs no more than 20% of her current
health, she plays as v213. Otherwise she tries, in order:

1. a ready dash to a safe tile that still has a shot (dash, then fire);
2. a safe step (no shot this turn);
3. a dash to a shooting tile with at most half the current danger;
4. v213's play.

`MICRO_APPROACH` keeps unsafe tiles out of her walk and dash while she closes on a target.

## Bench (release, --jobs 1, seed 0, 172 games each)

| build | opp | record | banked | K/D | fuel mean | out-of-fuel |
|---|---|---|---|---|---|---|
| v213 (Chat A) | v210 | 86-86 | 118010 vs 119279 (-1.1%) | 1521/1631 | 36994 | 0.32% |
| v216-micro | v210 | 85-87 | 119251 vs 119238 (+0.0%) | 1573/1703 | 37265 | 0.38% |
| v216-micro | v213 | 83-89 | 116568 vs 122690 (-5.0%) | 1508/1718 | 37220 | 0.38% |
| v216-micro-na | v213 | 86-86 | 116843 vs 123319 (-5.3%) | 1494/1723 | 37170 | 0.36% |
| v216-micro-off (fuel 1e8, 20 maps) | v213 | 20-20 | 28530 vs 28530 | 360/387 | - | - |

The off build is exact: perfectly symmetric against v213 with timing removed.

Vesper, from the stats JSON (`deaths.py`):

| run | games | deaths to champions | kills | damage dealt | damage taken |
|---|---|---|---|---|---|
| v213 vs v210 (Chat A release) | 154 | 207 | 822 | 2.12M | 1.10M |
| micro vs v210 | 154 | 205 | 839 | 2.01M | 0.99M |
| micro vs v213: ours | 154 | 227 | 772 | 1.86M | 0.97M |
| micro vs v213: v213's Vesper, same games | 154 | 225 | 807 | 2.01M | 0.99M |

## Verdict

**Drop: not for the merge.** Vesper's deaths don't move (227 vs 225 in the same games; 205 vs 207 vs v210). She deals
5-7.5% less champion damage. Banked is -5% against v213 with or without the approach rule, and flat against v210. Fuel
is about +0.25k a turn, and out-of-fuel turns go from 0.32% to 0.38%.

The micro only takes over when standing is already expensive. One smoke game had about 9 such turns in 456 fight turns.
By then a dash or step rarely escapes a melee plus a stun, and moving costs shots.

Not tested (deadline): other bars (0.1 / 0.35), `MICRO_RANGED`, seeds 1-2, the pool.

A different direction, unverified: deaths start from high health within ~5 rounds, so the lever is probably earlier.
Either don't enter a melee's reach without an ally covering her, or leave when the bar would be crossed in two rounds
rather than at 35%.

## Tools

- `deathctx.py DIR [--champ NAME]`: branch, health and geometry before each death (debug replays).
- `deaths.py DIR`: per-champion deaths, kills and damage from any --keep run (release is fine).
- `run_locked.sh QUEUE`: lock-aware bench queue.
