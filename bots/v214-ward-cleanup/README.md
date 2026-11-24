# v214-ward-cleanup (v213 minus the dead fixed-post ward code)

WARD_PLAN.md W4. Since v72 (`FIX_SCOUT_WARD = true`, `SCOUT_KEEP_POSTS = false`) the old ward posts and the key-bush
ward could never run, but their setup still did: `build_bush_boards` at a turn end (up to 15k fuel per champion) and
`compute_ward_spots` in the draft, written into the draft setup block. The ward-post mask also shared byte 7 with
the ping (`PING_BYTE`), a trap for anyone flipping `SCOUT_KEEP_POSTS`. No flag: code removal only.

Removed (v213 line numbers):
- warding.cpp 12-296: `compute_ward_spots`, `open_ward_spots`, `place_ward`, `ward_detour`, `build_bush_boards`,
  `bush_ward`, `follow_ward`; and the `SCOUT_KEEP_POSTS` early return in `scout_ward` (302). Only `scout_ward` is left.
- turn.cpp 150-159: the `scouting` flag and the `place_ward` / `bush_ward` branch (now
  `if (!(GHOST_WARD && ghost_ward(me)) && FIX_SCOUT_WARD) scout_ward(me);`); turn.cpp 592: the `ward_detour` call
  (its condition `!(FIX_SCOUT_WARD && !SCOUT_KEEP_POSTS)` was constant false).
- combat.cpp 1953-1957 (`ghost_ward`) and 2087-2093 (`answer_hidden`): the `ward_slot` / `WARD_MASK_BYTE` clean-up and
  the `bush_warded` / `ward_for_slot` writes. `ward_slot` was only ever set by `place_ward`; `bush_warded` and
  `ward_for_slot` were only read by the removed functions and by `scout_ward` behind `SCOUT_KEEP_POSTS`.
- route.cpp 161-167: `ward_spots` from the draft facts (or `compute_ward_spots` without them); altar kinds still come
  from the block. route.cpp 1277-1281: the key-bush board build (`BUSH_BOARD_FUEL` gate) at the head of
  `build_fields`, so goal fields start one step sooner.
- bot.h 360-374 (members `ward_spots`, `ward_slot`, `ward_zones`, `ward_union`, `key_bushes`, `key_bush_reach`,
  `bush_warded`, `ward_for_slot`), 813-820 (declarations), 994-995 (`compute_ward_spots`).
- Draft setup block (draft.cpp 166-181 encode, 645-656 decode, 658-665 `setup_end`; draft.h 87 `ward_spots`): the
  layout is now `SETUP_MAGIC`, altar count, one solo flag per altar (11 bytes shorter on every map). `SETUP_MAGIC` went
  from 0xA7 to 0xA8 so an old-layout block is rejected; the length checks and route.cpp's altar-count check are
  unchanged. v213's draft memory peaks at 619 of 1,024 bytes (00road-of-riches), so the aggressive route block was
  never trimmed and the shorter block changes nothing after it.
- constants.h: `SCOUT_KEEP_POSTS` (1075), the ping `static_assert` (1366), the post / bush / detour constants
  (1685-1706: `ANTI_WARD_RADIUS` ... `AGGRESSIVE_WARD_DETOUR_RANGE`, `BUSH_BOARD_FUEL`), `WARD_MASK_BYTE` (2004-2005).

Kept: `FIX_SCOUT_WARD` (true). Turning it off now means no scout ward at all (ghost and hidden-attacker wards still
run), not the old posts; the comment says so. `scout_ward`, `ghost_ward`, `answer_hidden` are otherwise untouched.

Diff: 8 files, 34 lines added, 400 removed. Debug and release builds compile with no warnings.

## Identity test (unlimited fuel)

v214-ward-cleanup and v213 vs v210, all 70 maps both sides, `--fuel-quantum 100000000` (100M a turn; 0
fuelExhausted events in all 280 games): **140 of 140 games identical**, comparing winner, submitted gems per team,
rounds, final input count and the whole stats event list (every kill, death, ward placement, purchase, capture).
Replays: `replays/bward-cleanup-id/`.

Uncapped fuel turn by turn on those identical games (same decisions, so the only difference is the removed code):

| Turn | v213 mean | v214-ward-cleanup | Change |
|---|---|---|---|
| draft first turn (10M budget) | 417,212 | 377,331 | -39,880 (`compute_ward_spots`) |
| first champion turn | 386,003 | 372,582 | -13,421 (the key-bush boards, built at its end when fuel allows) |
| next 10 turns per champion | 43,317 | 43,323 | +5 |
| every later turn | 36,724 | 36,726 | +2 |

So the dead code cost nothing per turn after setup (all its gates are compile-time constants); the saving is a
one-off ~13k per champion, spent at the first turn end with `FIELD_FUEL_RESERVE + BUSH_BOARD_FUEL` to spare. At
normal fuel that shifts when the goal fields get built, so games reshuffle as with any fuel-timing change.

## Fuel bench (normal fuel)

vs v210, all 70 maps both sides, timing seeds 0-2 (`bench.py seed`), against bots/v213 run the same way
(`bots/v213-s1`, `-s2`). Replays: `replays/bward-cleanup-fuel/`, `replays/bward-cleanup-v213/`,
`replays/bward-cleanup-v213s/`. Fuel from `bots/v114/fuel_bench.py` (turns at or under 100k).

| Seed | Build | Record | Banked | Mean fuel / turn | Out-of-fuel turns | Games identical to v213 |
|---|---|---|---|---|---|---|
| 0 | v213 | 71-69-0 | -0.4% | 36,695 | 1,556 | |
| 0 | v214-ward-cleanup | 68-72-0 | -3.2% | 36,636 | 1,531 | 71 of 140 |
| 1 | v213 | 70-69-1 | +0.2% | 39,527 | 2,000 | |
| 1 | v214-ward-cleanup | 65-74-1 | -6.4% | 39,366 | 1,942 | 62 of 140 |
| 2 | v213 | 67-73-0 | -3.2% | 41,931 | 2,465 | |
| 2 | v214-ward-cleanup | 68-72-0 | -4.3% | 42,213 | 2,811 | 63 of 140 |
| all | v213 | 208-211-1 | -1.1% | 39,403 | 6,021 | |
| all | v214-ward-cleanup | 201-218-1 | -4.6% | 39,423 (+0.05%) | 6,284 (+4.4%) | 196 of 420 |

Mean fuel is level, as the unlimited-fuel run says it must be. Out-of-fuel turns: -25 and -58 on seeds 0 and 1,
+346 on seed 2. About 53% of games at normal fuel differ from v213 (the other 47% match event for event): the goal
fields now start one turn end sooner, and any shift in when a turn runs out of fuel reshuffles the game. Banked -3.5
points over 3 seeds is at the edge of the measured noise (about +-3.5% for three pooled seeds), with no decision
changed at unlimited fuel to explain it.

The zz-wardprobe baseline (`replays/bwardbase-v210/zz-wardprobe*`) is not a fuel twin of v213. Its probe reads the
vision lists inside `LOG` (fuel paused), which fills the per-turn `seen::` cache for free, and it drops `log_turn`.
At seed 0 it scored +3.7% where v213 scored -0.4%, and its mean fuel was 36,228 vs v213's 36,695.

## Verdict

Keep it in the merge: it plays exactly as v213 at unlimited fuel, costs nothing more per turn, saves about 13k fuel on
each champion's first turn (and 40k of draft precompute), and removes the dead ward-post writes to byte 7 that the ping
uses. The normal-fuel bench is level on fuel. Banked is -3.5 points over 3 seeds, at the noise edge, and no decision
changed at unlimited fuel. The merge bench (3 seeds x 3 opponents) will show whether that holds. If the merge wants
zero risk, fold this in after the ward flags are chosen. It changes no play, so it doesn't interact with them.
