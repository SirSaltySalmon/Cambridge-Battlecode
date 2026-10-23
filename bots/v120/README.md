# v120

`v119` (with the user's Vesper build: Home shard in `VESPER_STRENGTH`, fallback strength, strength, health) +
`AOE_CLEAN` (area hits never cover another group's camps) and `STRAY_EXIT` (farming, step out of another group's awake
camps), plus a fuel pass. Not submitted. Every bench ran in debug mode with logs (`--log-limit 40000000`).

## What was wrong
1. A hit on any camp wakes its whole group, which then hits the nearest champion in reach. Ryth's area stun aimed at
   the centre covering the most camps of *any* group (`spell_places` listed every camp in sight), so it woke the
   group next door on purpose. In fights it ignored camps beside the enemy entirely.
2. Varg's splash picked the camp covering the most of its own group, never checking other groups beside it; in fights
   it splashed camps next to the enemy.
3. Heart of Ruin's dive skipped clusters (strength 0) when checking for sleeping camps, though a cluster in a wolf or
   elk group wakes them all.
4. Farming, nothing noticed another group's awake camps hitting us: `farm()` only kept melees out of awake monoliths.

## Changes
1. `AOE_CLEAN` (`kits.cpp` `aoe_scan`, `clear_of`; `actions.cpp` `aoe_aim(..., clear)`): one pass over the camps
   lists what an area hit should cover (the target's group, or the enemy champions) and what it must keep clear of
   (every camp of another group with a camp that hits back). Ryth's stun, Stellar's area, Varg's camp pick
   (`varg_splash_target`: clean camp in reach, else walk to the nearest clean one, else the old pick) and Varg in fights
   (`varg_fight_target`) use it; HoR's dive now sees clusters of hitting groups.
2. `STRAY_EXIT` (`combat.cpp` `stray_heat`, `stray_exit`, from `farm()`): awake camps of other groups near us (raw state
   read, cached per vision and slot). Hit by one (nearest in its reach), one step to a tile still in reach of our group
   and out of theirs; else one step out; else toward the edge. The farm walk then avoids their reach. The group's
   camps are read again after the step (old handles threw 'not in vision': 236 errors in the first bench).
3. Fuel (no behaviour change): `aoe_centers` walks the 3x3 square instead of three new boards, `positions()` and a
   `std::set`; `aoe_scan` replaces `spell_places`' `std::set` (ryth_stun 5.9k -> 2.3k a call); `gap()` is a flat
   vector instead of a `std::map` keyed by pairs; `monoliths.count()` in per-slot loops is a flag vector
   (`is_monolith`); `drop_hard` counts team emblems once instead of once per slot; `travel()` copy-assigns into a reused
   table and returns a reference; `visible_loot()` fills a pooled list. Release profile per call: pick_target 8.0k ->
   7.8k, travel 8.3k -> 7.9k, share_loot 2.9k -> 2.6k. The new code costs ~430 fuel a turn (stray_heat + exit).

## Bench (debug, 40 maps, both sides, vs v119)
| run | record | banked | camp deaths (us / them) | camp damage taken | champion deaths (us / them) |
|---|---|---|---|---|---|
| `b120a` (old v119 Vesper build, before the vision fix) | 43-37 | +11.0% | 169 / 206 | -2.0% | 1329 / 1483 |
| `b120b` (user's Vesper build) | 41-39 | +6.4% | 170 / 193 | -5.7% | 1293 / 1444 |
| `b120c` v120 (with the fuel pass) | 45-35 | +15.9% | 158 / 204 | -7.2% | 1243 / 1470 |
| `b120c` seed 1 (fuel burn) | 37-43 | -2.5% | 165 / 198 | -5.0% | 1379 / 1267 |
| `b120c` seed 2 (fuel burn) | 41-39 | +5.4% | 158 / 199 | -6.8% | 1384 / 1339 |

With the user's build: 164/320 (51%), banked +6.3% on average. Camp deaths fall 13-22% and camp damage 5-7% in every
run; that part is the mechanism, the record is within noise. Stray exits fired ~1,070 times in 80 games (691 were
"toward the edge": deep inside an elk's or monolith's reach). Fuel: release profile per turn equal to v119 (29.1k);
the debug bench mean is ~3% higher because v120 farms more (pick_target/travel turns), out-of-fuel rate equal
(0.179% vs 0.181%). Seed rows' fuel includes the burn.