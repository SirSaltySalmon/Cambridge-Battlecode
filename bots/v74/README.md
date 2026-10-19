# v74

`v73` + `FIX_SPAWN_BANK` (switch in `constants.h`). Not submitted.

## The bug
A champion reaching spawn alive with a bag (a low-health recall keeps it; Ren's rush recall keeps it on purpose)
looped for up to ~440 rounds: on spawn, `spawn.leave` (it never checked the bag) stepped off toward the farm; off
spawn, a bank rule walked toward an altar whose path crosses the spawn, stepping back on. `update_rejoining()` set
`rejoining` again on every spawn touch, so the champion never led or called an escort to break the tie. Found by a
teammate. In the ladder test logs it hit 6 of 30 games for both v71 and v72 (7 of 9 corners games, plus ambush,
packs), and 5 of 34 local v73 games (corners, ambush, packs, rows, interlock).

## The fix
1. `bank_from_spawn()` (turn.cpp): before `spawn.leave`, the off-spawn bank rules are checked in the same order
   (Ren's 150 cap, near-bank, hunted carrier, step 7 incl. idle and solo-altar heavy bags); if one would bank, the
   champion starts the bank walk from spawn, so stepping back on spawn keeps walking to the altar.
2. `update_rejoining()` (route.cpp): only an arrival (respawn, recall, match start) sets `rejoining`; walking back
   over a spawn tile does not.

## Results (34 games, all 17 maps from both sides)
v74 vs v73: 18-16, gems +7.6%, gems lost -27%, deaths -2%, out-of-fuel turns +2%, 0 crashes.
Spawn loop pairs: v73 360, v74 0. A first version that skipped the near-bank rule for built Ren and the hunted rule
left 12 pairs (packs, interlock).
