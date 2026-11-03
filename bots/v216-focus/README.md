# v216-focus: fight plan F4 (focus kept steady, focus by value, peel)

Base: bots/v213. One source tree; the variant folders differ only in three flags in `src/constants.h`
(made by `tuner/work/focus/mkvar.sh NAME KEEP VALUE PEEL`).

| Folder | FOCUS_KEEP | FOCUS_VALUE | PEEL |
|---|---|---|---|
| v216-focus | on | off | off |
| v216-focus-v (+ -s1, -s2 fuel seeds: 300 / 600 sink like v213-s1/-s2) | on | on | off |
| v216-peel | off | off | on |
| v216-focus-off | off | off | off (= v213) |

## What changed

- **FOCUS_KEEP.** In v213 any champion whose near list lacks the focus overwrites it with its own pick on every
  `fight()` turn, so two separate fights overwrite each other. Now a champion that does not see a fresh, living focus
  picks its own target locally and does not write.
  - Shared bits: the focus index moves to bits 4-6 of byte 4 (enemy roster <= 6, so index + 1 <= 6). Bit 7 is a
    refresh bit: every champion that sees the focus sets it to `round & 1`. Each champion watches the nibble every turn
    (`watch_focus`); unchanged for FOCUS_STALE 3 rounds means nobody saw it, so it is stale and may be overwritten.
    Seen every round, the nibble changes at least every 2 rounds whatever the turn order. No other byte is touched
    (Chat C uses byte 1 bits 6-7 and byte 7).
  - Not done: "clear on contract end" (would need Chat C's contract).
- **FOCUS_VALUE.** The focus written is the near enemy with the best
  `(FOCUS_KILL_VALUE 40 + its damage per round, Willow's heal counted) x (damage of us + visible allies within reach + 2 of it) / (health + HoR shield)`
  (`focus_by_value`). A champion that sees the focus moves it only to an enemy scoring 1.3x better, and never away from
  a focus that none of its visible hitters reaches (that scored 0 from its view and caused thrash in the first smoke).
- **PEEL.** Ryth (area stun), Prism (stun) within range 4, or Marcella (enhanced reset, a walk and a hit away) turn the
  ready stun on an enemy melee within 2 of our Vesper, Prism or Willow; skips JOIN_FIRST for that turn. A channelling
  enemy in reach still comes first. Debug logs record peels (and chances with PEEL off) and the carry's health a round
  later (`peel_report`).

Files touched (this folder only): constants.h, state.cpp/h (`read_focus`, `write_focus`, `focus_nibble`), combat.cpp
(`fight()` focus block, `focus_by_value`, `peel_target`, `peel_report`, `watch_focus`, `set_focus`), turn.cpp (hooks),
bot.h.

## Bench (release, --jobs 1; logs tuner/replays/e-*.log)

Exactness: v216-focus-off vs v210 at `--fuel-quantum 100000000` = 86-86, 117979 vs 120569, K/D 1589/1709, fuel 36656:
identical to v213. Under real fuel 171/172 games identical (03the-front-a flips on suspension timing).

Screens vs v213, seed 0, 172 games (noise reference v213-s1 vs v213: 86-86, -0.7%, K/D 1579/1671):

| Build | Record | Banked | K/D | Fuel mean | Out-of-fuel |
|---|---|---|---|---|---|
| v216-focus (KEEP) | 87-85 | -6.3% | 1552/1777 | 36960 | 0.36% |
| v216-peel | 84-88 | -4.5% | 1434/1682 | 36907 | 0.35% |
| v216-focus-v (KEEP + VALUE) | 96-76 | +12.3% | 1641/1647 | 37341 | 0.39% |

v216-focus-v vs the pool (v210, v206, v208, v185), 688 games per seed, against Chat A's v213 baseline:

| Seed | focus-v record | focus-v banked ours vs theirs | K/D | v213 record | v213 banked |
|---|---|---|---|---|---|
| 0 | 365-323 | 503847 vs 461494 | 7553/6803 | 362-326 | 493606 vs 457269 |
| 1 | 336-352 | 481187 vs 475431 | 7120/7244 | 358-330 | 491720 vs 459269 |
| 0+1 | 701-675 | 985034 vs 936925 | | 720-656 | 985326 vs 916538 |
| 2 (cut by HEAD after v210 only) | v210: 87-85 | 125437 vs 119860 | 1677/1787 | v210: 83-89 | 117936 vs 120074 |

Fuel (seed 0): 37272 per turn vs v213 36994 (+0.8%), out-of-fuel turns 0.37% vs 0.32%.

## Verdict

- **FOCUS_VALUE + KEEP: neutral.** Pooled seeds 0+1: 19 fewer wins, our banked 0.0%, theirs +2.2%. The +12.3% screen
  vs v213 did not hold against the pool. Not a keep on its own; it can ride in a stack only if the stack wins.
- **FOCUS_KEEP alone and PEEL: not kept.** Both screens lean negative (-6.3%, -4.5%, deaths up ~140 beyond the noise
  run) and neither had a mechanism showing a gain. PEEL fires ~2 times a game, so its screen is mostly chaos; the data
  can't say whether peeling helps.
- Not measured (time): debug fight metrics (focus writes per contact, kills per contact, Vesper deaths) on the bench
  games. Smoke games only: focus writes ~5-9 per 100 fight turns; same-area target splits stay 32-45% because v213's
  "weakest in reach before the focus" rule is unchanged.

Open questions from the plan: whether Ryth's stun usually reaches a diver on Vesper from where Ryth stands, and
whether HoR should dive the diver: unknown (too few peels to tell).
