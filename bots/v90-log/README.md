# v90-log

`v90` + a rebuilt decision log: every branch says why it was taken, briefly, with no health, gold, item or champion
stats. All of it is built inside `debug_block`, so it costs no fuel. The decisions themselves are unchanged.

## How it reads

One line per champion turn, then indented lines only when they add a reason:

```
r300 c3 (11,0) fight.hit.step: weakest enemy, closing at (18,1); spell ready, nobody in cast range: step toward (18,1) outside enemy reach | move
lead s0 (18,10) -> tribute, farm: no enemy in sight; an escort turns farm into tribute | aggressive, escort c8, centre, zone 1
r153 c3 (10,22) farm.camp.hold: s6 camp (13,26): weakest in reach; safe zone with the team or healthy: hit it like a ranged; waits 4 off the camp until a melee is closer and tanks | move
- c8 claims s0's reward (nearest, poorest)
```

- Turn line: round, champion, tile (plus `ON-ENEMY-SPAWN` / `IN-CHASE-RING`), the branch name, its reason chain, then what the
  turn did (`move,attack`) or why it did nothing (`idle: already on goal`).
- Kit choices extend the branch (`SUB`): `.cast`, `.standoff`, `.hold`, `.follow`, and reasons such as "stun the weakest
  enemy in reach", "area spell: 3 in it", "reset for a second hit", or the v86 mage rule's verdict.
- `lead ...`: the shared decision (target, mode, score terms, hysteresis, escort, ping, phase reason, then tags). It is
  printed only when it changes from the last time this champion led (score numbers ignored), not every round.
- `own sight disagrees with ...`: a follower whose own score picks another mode, printed once per disagreement.
- `- ...`: side decisions (claims, escorts, phase changes with their reason, split pair moves, pings, wards, hidden attackers).
- `setup: ...`: once per champion per game (zones, altars, gem-starved).
- The fight score lists each term's points and the head count (`bodies +10 5v2, health +5, power +5, emblems +27,
  distance -4.0`), not the health or power totals.
- Removed: the per-turn `step-room` / `dA dE` lines, the spawn gap, follower copies of the shared line, champion names in
  reasons, and the `round N: ...` team-event lines (now notes on the turn that made them).
- Size: about 60% fewer lines (contest-a: 21,483 vs 51,811 for v90).

## How it is built (`bot.h`, `board.h`)

- `LOG(stmts)`: runs `stmts` inside `debug_block` and a `try`, so building log text never costs fuel and a failed read
  while building it can never end a turn (the v90 vision error was exactly that).
- `WHY(name, why)`, `NOTE(text)`, `SUB(suffix, why)` wrap `branch()`, `note()` and branch extensions in `LOG`.
- Text that used to be built on every turn outside `debug_block` now lives in `LOG`: the fight score's reason
  (`score_mode`), the channel threat reason (`channel_threat`), chase-ring lines (`chase_line`), phase / raid / split /
  finish / band reasons, every branch and note string, and the failed-step reasons in `move_towards_closest`
  (`goal_cover`, `no_step_reason` read boards). The one side effect there that decisions use, `seen::reset()` after an
  engine-rejected step, now runs outside the log.
- Reasons passed between functions are literals (`const char*`) where they were built strings: `release_claim`,
  `end_split`, and `mage_cheese(..., &why)`.
- A `--release` build compiles and runs. Locally it prints nothing, as the SDK's `--release` note says.
- `analyze_errors.py` reads both log formats.

## Results (34 games each, normal fuel, debug build, `analyze_errors.py`)

| | record | gems | mean fuel a turn | p99 | turns >= 90k | out-of-fuel turns (`.fuel` log) | `fuelExhausted` events | games with any |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| v90-log vs v90 | 20-14 | 28,815 vs 25,635 | 28.3k vs 31.7k | 66.5k vs 79.1k | 0.13% vs 0.39% | 166 vs 284 | 68 vs 206 | 30 vs 33 |
| v90-log vs v82 | 20-14 | 27,986 vs 22,976 | 27.9k vs 36.8k | 66.8k vs 85.7k | 0.15% vs 0.68% | 169 vs 455 | 71 vs 291 | 29 vs 34 |
| (earlier) v90 vs v82 | 18-16 | 26,674 vs 24,338 | 32.2k vs 36.9k | 79.2k vs 85.2k | 0.37% vs 0.64% | 258 vs 405 | 180 vs 241 | 33 vs 34 |

- Fuel: about 11% less per turn than v90 and 24% less than v82. The heaviest turns drop most: p99 -16% vs v90, turns over
  90k -65%, out-of-fuel turns -42% (`.fuel` log) and `fuelExhausted` events -67%. This is the log text that v90 still
  built outside `debug_block`; nothing else changed.
- Results: the decisions are identical, so the 20-14 and +12% gems vs v90 can only come from fewer turns cut short by
  fuel. That is at the edge of noise (+-2 games, ~18% gems per 34 games), so read it as "no worse, maybe better".
- Out of fuel is now rare and spread out (worst game 12 turns: contest-a); v90's worst were lanes (38, 32).
- Result files: `results-v90-log-vs-v82.txt`, `results-v90-log-vs-v90.txt` (repo root).