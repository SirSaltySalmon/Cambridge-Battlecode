# v218-micro: Vesper's micro, acting earlier and keeping the shot (fight_coord_plan.md F5, second try)

Base: bots/v216-micro (itself v213 + `src/micro.cpp`). Flags in `src/constants.h`: `MICRO_VESPER`, `MICRO_APPROACH`,
`MICRO_PROJECT` 2 (documents the two-turn projection), `MICRO_DMG_BAR` 0.2, `MICRO_COVER` 0.5, `MICRO_DASH_KEEP` 0.5,
`MICRO_KILL_SHARE` 0.9, `MICRO_REENTER` 2, `MICRO_SAFE_ZERO` true, `MICRO_FUEL` 15000. Hook: top of `engage_vesper`
(kits.cpp), champion targets only. With `MICRO_VESPER` and `MICRO_APPROACH` both off, the hook is skipped: v213.

## Why v216 didn't work

v216 only acted when standing cost more than 20% of her current health: about 2% of fight turns, too late to beat a
melee plus a stun. Each evasive step cost a shot. Her deaths didn't move (227 vs 225), she dealt 5-7.5% less damage,
and banked was -5% vs v213.

## What v218 does

Threat model as in v216: basic reach, a ready dash or reset, the best ready spell, stuns, and half damage when a sturdy
ally is closer to the enemy. Sturdy is now anyone but Vesper, Willow, Prism, Stellar and Cassi. Then:

- **Trigger (a projection).** A tile is unsafe when two enemy turns of standing there would drop her under the recall
  bar (0.35 of max health), or the next enemy turn alone costs more than 20% of current health.
  - The second turn: a melee walks one more tile and swings; a ranged enemy shoots again from where it stands; spells
    and resets are down.
  - A stun counts its extra turn.
  - `MICRO_SAFE_ZERO`: a tile no enemy reaches next turn is never unsafe.
- **Order on an unsafe tile.**
  - (a) Fire here (v213's play, reset included) when the target is in reach and either the hits kill it, or one more
    enemy turn is affordable with the dash ready and no stun reaching her.
  - (b) Dash to a safe tile that still shoots, then fire.
  - (c) Take a safe step: least danger first, then nearest a shot.
  - (c2) Dash to a safe tile.
  - (c3) Dash to a shooting tile with at most half the danger.
  - (d) Play as v213.
- **`MICRO_APPROACH`.** While closing in, her walk and dash stay out of uncovered melee reach: walk 1 + hit 1 + its
  ready dash, unless a sturdy ally is strictly closer to that enemy.
- **No oscillation.** For 2 rounds after the micro moves her, the approach keeps out of tiles the model still calls
  unsafe.
- **Fuel.**
  - One engine record per champion (`sdk_champion`), with the cooldown read standing in for `is_ready()`.
  - Enemies more than 9 tiles away are never read, and the micro returns early when nothing reaches the window.
  - The window shrinks to 3x3 when she is in reach with her dash down.
  - Log text goes on the turn's line through SUB: `.micro.step`, `.micro.dash`, `.mfire`, `.mnone`, `.mguard`,
    `.mguard.re`.

## Smoke (debug, vs v210, the same games as `replays/inv216/v213`; `replays/v218m/`)

6 games, so this is noise-level evidence, not a bench. The first 2 (`calib/`) ran an earlier build: its second turn
also counted a ranged enemy walking and shooting, and its tile loop was unoptimised. The other 4 ran the smoke build
(`MICRO_SAFE_ZERO` off, before the read cuts).

| games | build | record | banked | V deaths | V kills | V dmg done | V dmg taken |
|---|---|---|---|---|---|---|---|
| gemfight-b, starvation-a | v213 | 0-2 | 124 | 11 | 4 | 13.5k | 19.1k |
| same | v218 (calib) | 1-1 | 1571 | 3 | 20 | 34.8k | 24.7k |
| shortcut-a/b, arena-b, the-pond-b | v213 | 1-3 | 2780 | 14 | 28 | 104.4k | 88.8k |
| same | v218 | 0-4 | 1801 | 14 | 24 | 82.7k | 91.6k |
| all 6 | v213 / v218 | 1-5 / 1-5 | 2904 / 3372 | 25 / 17 | 32 / 44 | 117.9k / 117.5k | 108.0k / 116.3k |

- **How often it acts.** It ran on 712 of Vesper's fight.hit turns against champions.
  - It triggered on 82 (11.5%) and moved her on about 40 (5.5%; v216 about 2%). Most moves were steps.
  - The other triggers: 6 fired here (`.mfire`) and 37 found no safe tile (`.mnone`).
  - The approach guard shaped about 18% of those turns (`.mguard`).
- **Oscillation.** Of about 40 micro moves, 2 returned to the tile she left within 2 rounds. Back-to-back micro moves happened
  only twice.
- **Faults.** No `botFault` entries.
- **Fuel** (smoke build, Vesper turns aligned from `team-*.fuel`):
  - fight.hit turns: mean 53.0k vs 41.9k for v213, p99 79k vs 68k, max 92k vs 73k (one turn over 90k).
  - All of Vesper's turns: +1.7k. Team mean: 31.9k vs 32.2k.
  - Out-of-fuel turns: 8 vs 9 for the team, 0 for Vesper.
  - The micro itself cost a median 9.8k a call. The read cuts after the smoke should take a good part of that off.
    They aren't measured.

## Not tested

- The read cuts and `MICRO_SAFE_ZERO`. The cuts should not change decisions; set `MICRO_SAFE_ZERO` false for the
  smoke build's decisions.
- Release fuel, seeds, the pool, `-na` (`MICRO_APPROACH` off), and other bars.

## Tools

`deaths.py`, `deathctx.py`, `run_locked.sh` and `mkvar.sh` come from v216-micro; `mkvar.sh` now writes
`v218-micro-<suffix>`. `deathctx.py` needs `tools/fights/fightstats.py`, which this checkout doesn't have.
