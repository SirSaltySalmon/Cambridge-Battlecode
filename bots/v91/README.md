# v91

`v90-log` + mages never tank a camp, and a hurt tank no longer counts as one.

## The problem (ambush, v90-log as team A, rounds 127-142)

Issac tanked the camp at (30,44) while Cassi, Stellar and Willow hit it from 3 tiles (inside its reach of 3). At r133 Issac
fell under the recall bar, stepped out and recalled. Cassi and Stellar kept basic attacking from inside the reach for
about ten rounds, taking the camp's hits, until Ryth arrived; Willow hit on for a round too. Two rules caused it:

- `V86_MAGE_RULE` sends a mage in the safe zone that is healthy or has an ally near down the ordinary ranged path ("and
  tank them when nobody else does"). That path holds back only while a melee is in sight, so once the tank was gone the
  mages opened up from inside the reach instead of standing off (`mage_camp`).
- `camp_tanked()` and `coming_to_camp()` counted a tank under the recall bar that had not yet stepped away.

## Fix

- `V91_MAGE_NO_TANK` (`mage_cheese()`): the v86 rule applies only while a sturdy ally tanks the camp (`camp_tanked()`).
  With nobody tanking, the mage stands off: it leaves the reach, casts from 4 and waits out cooldowns there.
  `mage_camp` still basic attacks when the finish is cheap (at most `MAGE_FINISH_SHARE` = 10% of its health), so a
  little tanking is still accepted.
- `V91_TANK_HURT`: `camp_tanked()` and `coming_to_camp()` skip an ally under `RECALL_HEALTH`. This also makes Willow and
  other ranged hold back the turn the tank starts leaving.
- Same scene in v91: at r133 Cassi and Stellar step out to the cast ring and cast from 4 (`farm.camp.leave`, then
  `.standoff` / `.cooldown`); Willow holds; all go back to hitting once a tank arrives.

## Results (34 games vs v90-log, normal fuel; `analyze_squishy.py`, `analyze_errors.py`)

| | record | gems | caster camp damage taken / game | Cassi | Stellar | Willow | caster deaths | caster camp damage done / game | mean fuel |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| v91 | 17-17 | 25,624 | 1,091 | 307 | 338 | 583 | 239 | 32,224 | 28.6k |
| v90-log | 17-17 | 25,968 | 1,479 | 533 | 462 | 632 | 243 | 31,536 | 28.3k |

- Casters take 26% less camp damage (Cassi -42%, Stellar -27%, Willow -8%) and deal 2% more camp damage; the team's
  total camp damage taken is 3% lower.
- Record, gems (-1.3%) and deaths are within noise (+-2 games, ~18% gems per 34 games). Most caster deaths come from
  fights, which this does not touch.
- Fuel: +1% a turn (one more `camp_tanked()` read per mage camp turn); out-of-fuel turns unchanged (175 vs 173).
- Result file: `results-v91-vs-v90-log.txt` (repo root).

# Base: v90-log

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

# Base: v90

The starting copy (v82 + the v86 zone, phase, split, band, finish and mage-cheese code) + the fix for "bot error: Unit
is not in vision.", and an analysis script, `analyze_errors.py`.

## The vision error

Every one of these errors (both bots, 34 games) came from two log notes in the v81 mage code (`kits.cpp`). Each one
read the unit right after the spell, so a spell that killed it threw. That ended the rest of the turn and dropped the
turn's log:

| branch (where the error came from) | v90 before | v82 |
| --- | --- | --- |
| `fight.hit.cast`: `mage_fight()` note read the enemy's position after the cast | 123 | 114 |
| `farm/retreat/mono.camp.standoff`: `mage_camp()` note read the camp's health after the cast | 77 | 108 |
| total | 200 (5.9 a game) | 222 (6.5 a game) |

Fixes:
- Both notes now sit in `debug_block` and read the unit only after `alive()` says it is still there. In release builds
  (the platform) they are skipped entirely, so the string building costs no fuel either.
- `MAGE_KILL_FOLLOWUP`: when a mage's fight spell kills its target and the mage stands outside enemy reach, the unspent
  basic attack goes to the next living enemy in reach (before, a kill ended the turn). It fired only 3 times in 34
  games.

## Results (34 games vs v82, normal fuel, debug build, `analyze_errors.py v90 v82 DIR`)

| | record | gems | vision errors | mean fuel a turn | p99 | turns >= 90k | out-of-fuel turns (`fuelExhausted` events) | games with any |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| v90 | 18-16 | 26,674 vs 24,338 | 0 | 32.2k | 79.2k | 0.37% | 180 (258 turns at the limit in the `.fuel` log) | 33 |
| v90 before the fix | 18-16 | 26,614 vs 24,262 | 200 | 32.2k | 79.7k | 0.38% | 181 (259) | 33 |
| v82 (in the v90 games) | 16-18 | | 216 | 36.9k | 85.2k | 0.64% | 241 (405) | 34 |

- The fix removes every vision error and changes nothing else measurable: record, gems and fuel match the unfixed copy.
  The error only struck after the turn's spell, so little was lost each time.
- Fuel vs v82: about 13% less a turn, and out-of-fuel turns about 25% fewer (events) or 36% fewer (turns at the
  limit). This comes from the starting copy (the unfixed copy has the same numbers), not from the fix. Both bots still
  run out somewhere in nearly every game, mostly on contest, lanes and swirl (v90: contest-a 27, lanes-a 21, swirl-b 15).
- `analyze_errors.py` reads per-turn fuel from each replay's `team-<a|b>.fuel` (13-byte header, eight u64 limits, the
  draft turns, the 200k first turns, then one u64 per champion turn; 100,000 means the turn ran out). The `.fuel` log
  counts more turns at the limit than the stats JSON has `fuelExhausted` events; both counts are listed.
- Result files: `results-v90-vs-v82.txt`, `results-v90-pre-vs-v82.txt` (repo root).

# Base: v82

`v81` + recall caution (`RECALL_SPELL_CAUTION`, `RECALL_REACH_MODE`) and the champion id on each log turn line.

## Recall caution (68 games each: 34 normal + 34 unlimited fuel, `analyze_recalls.py`)

| | record vs v80-5 | gems | recalls broken | died mid-recall | turns held by an enemy's reach | cancelled by us |
| --- | --- | --- | --- | --- | --- | --- |
| caution off | 35-33 | +3.4% | 87 / 1,374 (6.3%) | 32 | 0 | 123 |
| mode 0: casters by name | 39-29 | +6.9% | 16 / 1,395 (1.1%) | 14 | 2,368 | 234 |
| mode 1: kit, all ready | 40-28 | +9.7% | 17 / 1,464 (1.2%) | 15 | 2,540 | 278 |
| mode 2: kit, live cooldowns (v82) | 37-31 | +5.8% | 14 / 1,478 (0.9%) | 14 | 1,645 | 299 |

- Caster caution cuts broken recalls ~83% and deaths mid-recall ~55%.
- Dashes barely matter: with casters covered almost nothing breaks, and planning for dashes (mode 1) or not (mode 0,
  which still cancels once an enemy is close) gives the same rate.
- Live cooldowns hold recalls back 30% less often than mode 0 at the same safety, so v82 uses mode 2. Records and gems
  are within noise (about +-2 games, +-9% gems per 34 games).
