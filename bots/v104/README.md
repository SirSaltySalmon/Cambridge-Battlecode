# v104

`v103` + a priced invasion of the enemy's side once the aggressive route is on (switches `INVADE*`, `REVERT_*` in
`constants.h`, code in `phase.cpp`: `update_invasion`, `invade_rate`, `invade_target`). Not submitted.

## Changes
1. `INVADE`: aggressive (Vesper's core, `TEAM_AGGRESSIVE_EMBLEMS`, all alive) with the centre open, the leader prices
   every enemy-side gem group (`enemy_side`: unsafe, or fewer walking steps from their spawn than ours; the draft's
   unsafe label alone is too strict: corridors has none, islands 3 of 46). Rate = `route_rate_steps` (travel, respawn
   wait, clear, capture) x (1 + `INVADE_DENY` 0.5) / (1 + `INVADE_RISK` 0.5 x enemies seen within 10 of it, fading
   over 60 rounds). It invades (RAID phase) only at >= `INVADE_MARGIN` (1.0) x the normal pick's rate; the current
   target gets `INVADE_KEEP` 1.25; a group being cleared is finished.
2. Revert: every champion counts kills and deaths from roster life and both teams' banked gems (all global, so no
   shared memory is needed; enemy gold cannot be read, banked gems stand in). From the round aggressive first comes
   on, deaths - kills >= `REVERT_TRADE` (2) or their banked lead >= `REVERT_GEMS` (150) turns invading off for
   `REVERT_HOLD` (300) rounds, then a new baseline.
3. `INVADE_TIMERS`: aggressive, `best_goal` rates every open group with its respawn wait, not only the centre pocket.
4. `INVADE_AMBUSH`: more of us alive than them, an enemy group back within 5 rounds of our arrival gets +50%.

Debug check, 00ambush v104 (A) vs v103: 20 invasions started, won 1,015-529, 19 kills / 4 deaths.

## Bench (release, 35 maps, both sides, one seed; `replays/b104`, v102 mirror and v103-vs-v102 from `replays/b103`)
| | record | mirror | flips | gem margin | banked | kills | deaths | banked r300 / r600 / r900 |
|---|---|---|---|---|---|---|---|---|
| v104 vs v103 | 35-35 | 35-35 | +6 / -6 | +934 | +2.0% | +10.0% | -7.8% | -2.6% / +8.7% / +2.0% |
| v104 vs v102 | 40-30 | 35-35 | +15 / -10 | +7,974 | +18.2% | +27.0% | -19.1% | -1.4% / +7.0% / +9.2% |
| v103 vs v102 | 44-26 | 35-35 | +16 / -7 | +13,001 | +32.8% | +29.6% | -19.8% | +3.8% / +13.4% / +18.9% |

Fuel (vs v103): mean 25.9k vs 24.4k a turn, out-of-fuel turns 409 vs 353, fuelExhausted 180 vs 121.

Reading: against v103 the invasion is neutral on results (within the +-2 game noise) with a better K/D (+10% kills,
-8% deaths) and mid-game pace (+8.7% banked by r600) but slower early; against v102 it does worse than v103 alone
(40-30 vs 44-26). The four parts have not been benched apart yet.

## Pricing fix (after the 00ambush replay)
The team camped a captured enemy group: rate = gems / (travel + respawn wait + clear + capture) let a 51-gem group
with a 54-round wait (0.67 a round) beat every nearby group, both in the invasion pick and in pick_target's
"goal is down" branch, which rated the down goal against its own fillers. Now an enemy group still down
`INVADE_MAX_WAIT` (8) rounds after we would arrive is not invaded (unless the ambush case), and `FIX_FILL` fills a
down goal with the up group whose gems most exceed what the detour costs the goal (goal rate x rounds of delay,
walking back included), per round of detour, keeping the current filler unless another beats it by SWITCH_MARGIN.
00ambush debug, while invading: respawn waits 1,588 -> 388 turns, fight turns 178 -> 525.

Rebench (`replays/b104b`): vs v103 35-35 (+7/-7 flips, margin +291), banked +0.6%, gold +4.8%, kills +10.1%,
deaths -7.6%, banked r300/r600/r900 +2.6% / +3.3% / -3.9%. vs v102 38-32 (+15/-12, margin +5,970; v103 got 44-26).
Fuel: mean 26.0k a turn, fuelExhausted 187 (v103 121).
