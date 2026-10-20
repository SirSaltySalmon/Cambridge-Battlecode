# v100

`v98` made cheaper: the same decisions for less fuel. Over 252 games each (six fuel-burn seeds, 21 maps, both sides)
against v98: **out-of-fuel turns -84% (1,918 -> 298), turns over 90k -80%, mean fuel a turn -13%**, and 131/252 wins
against v98's own 123/252. Under the heavier seeds (more fuel pressure) v98 fell to 17-20 wins of 42 while v99 held
23-24.

## What fuel costs (measured with `bots/fuelbench`, release -O3)

| | fuel |
| --- | --- |
| a host call (`is_set`, `field.get`, `round_number`) over loop overhead | ~25-35 |
| a champion method read (`position`, `health`, `is_recalling`) | ~75-100 |
| `base_stats()` | ~220 |
| a new bitboard (any temporary, copy, `~a`, `a & b`) | ~210-310 |
| the same op written into an existing board | ~100 |
| **a new `distance_field`** | **~1,550** |
| first `map::camp_groups()` | ~37,000 |
| a printed line | ~1,000-1,500 |

Guest code is dear (~10 fuel a simple statement): reading a board or field straight from its chunked storage cost
more (~50 and ~95) than the host call, so the SDK reads stay. Allocation and long guest loops are what to cut.

## Changes (each keeps the same result; the last two change timing on purpose)

- **Reused storage** (`actions.cpp` `MoveScratch`, `board.h` `fast::*_into`, `blocked_tiles_into`): movement and hop
  routing reuse two distance fields and a set of boards; `blocked_tiles` reads the vision boards into one scratch
  board (`vision::champion_tiles()` with no team was three boards). `compute_to` overwrites every entry (checked:
  unreached tiles come back as `unreachable` whatever the field held), so reuse reads what a fresh one would.
- **Vision lists pooled** (`seen.cpp`): lists retired at turn end are emptied and handed out again;
  `visible_camps` reads into a reused board.
- **No champion tile board for ordering** (`seen.cpp` `visible_living`, `fast::rank`): the engine's tile order is
  8x8 chunks, `chunk_stride` per chunk row, row-major inside a chunk (checked against `positions()` on 50, 60 and 100
  wide maps), so a sort of the few found champions gives the same list.
- **`travel()`** caches its raw steps per round, origin, skip and built-field count (the leader asks several times
  a turn), and reads built goal fields even before all are built (walking is symmetric).
- **`best_goal`** in two passes with no lists; `route_rate_steps(s, steps, now)` reads the round once per loop;
  `pick_target` filters loot before `best_goal`, so a follower with no loot in the table skips it.
- **`under_threat`** checks the camp's spawn tile against `MONOLITH_REACH` before four engine reads (camps never
  move; none reaches further).
- **`observe`** into a reused board and tile buffer; **`cached_ident`** reads each champion's type once;
  **`choose_phase`** reads whether Cassi & Issac were drafted once; movement caches the champion's move speed.
- **Setup** (~220k on islands, over the first turn's 200k): one pass per group for value, clear time and load
  (`draft::group_stats`, bit-identical arithmetic), spawn lists viewed in place (`SpawnLists`), the zone block read
  straight from draft memory (`draft::zones_block`), the altar-gate check without a board per altar. Islands: route
  loop 89k -> 58k, zones 37k -> 11k.
- **`thin_holes`** from whole-board shifts (~8k) instead of a loop over every hole (~400k on islands, which took the
  dashers' first move over four rounds); identical boards on islands, fracture, concentrated-wealth and hooks.
- **Timing, on purpose:** the first turn ends after setup unless `SETUP_PLAY_FUEL` (60k) is left (v98 spilled setup
  into round 2 on big maps anyway), and dashers no longer lose rounds to `thin_holes`. Games therefore diverge from
  v98 from round 2, so per-map results move (fracture 7/12 -> 0/12, broken-curve 6/12 -> 12/12) with the same code
  path in every seed; read the totals, not single maps.

## Tools

- `fuel_bench.py` / `fuel_compare.py`: fuel per turn of the bot under test from replays alone (exactly 100,000 is
  out of fuel; draft and first turns, with bigger budgets, are left out).
- `bots/fuelbench`: a one-file bot for timing SDK calls and C++ patterns.
- Profilers (not for submission): `replays/prof/patch2.py` (any function list), `patch3.py` (top-level phases, low
  overhead), `patch4.py` (inside the shared decision), `prof2.py` aggregates. Many nested scopes inflate the inner
  numbers (~100-150 fuel a scope), so trust the low-overhead lists.

## Left on the table

- `farm` (~10k a call: movement plus the kits) and `remember_allies`, `share_loot`, `observe` (~1.7-2k a turn each)
  are the largest remaining items. Lanes and big-world hold most remaining out-of-fuel turns.

## Next: v100, the aggressive mode (notes from the v98 talk)

The idea: farming the most valuable pockets is right, but on symmetric maps without a big centre camp the aggressive
route has no clear way to engage and dominate. Agreed so far:

1. **Open the enemy's side when aggressive, priced.** Rate every group by gems a round (travel, clear, capture), and
   value an enemy-side group at its own worth plus part of what taking it denies them, minus a risk cost from how many
   enemies were seen near it lately. **Only invade groups worth at least as much as our best one.**
2. **Revert when it goes badly.** Keep the team's kills, deaths and gold in shared memory; if we fall behind on gold or
   kill/death trades since invading began, go back to farming the valuable groups on our side.
3. **Plan around respawn timers** instead of waiting in the centre pocket (40-55% of turns after the aggressive route
   switched on, on ambush and corridors).
4. **Arrive as enemy camps respawn** when ahead in a fight: take them and catch their farmers.
5. **Trigger:** the existing aggressive route trigger (Vesper's core done, team emblems, all alive). Measure gold, K/D
   and gem pace against v99, then on the ladder.