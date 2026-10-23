# v122

`v120` + `BANK_STEPS`: "an altar is near" (`bank.near`, `spawn.bank.near`, `band.bank`) is measured in walking steps to
a submit tile (`altar_near`, one field per altar built on first use) instead of straight distance. Three other
anti-shuffle switches are in the code and off. Not submitted. (The earlier v122, EXIT_TO_ALLY, was removed: it lost
every variant vs v120, 221/560.)

## Why champions shuffled (v120 logs, `analyze_shuffle.py replays/b120c/v120`)
4.0% of turns were a step there and a step back (16.7k of 415k); ~400 runs of 5+ turns. The long ones:
1. `farm.walk` <-> `farm.off` (2.7k turns): a group "up" by our timer but not there; the walk goal was its spawn tiles,
   standing on one holds the group down, so "walk to it" and "step off" alternated.
2. `bank.near.walk` <-> `farm.walk` (~1k): near-altar banking by straight distance; behind a wall the walk left the
   range, farming walked back in (great-wall: one run of 77 turns).
3. `farm.camp` <-> `farm.walk` (~1k): 914 of 1,043 flips switched groups as a camp left and re-entered sight.
4. `fight.exit` <-> `fight.hit`: intended (left as is).

## Switches
- `BANK_STEPS` (on): fixes 2; the loop is gone from the logs.
- `SPAWN_RING` (off): walk to the ring around the spawn tiles when all of them are in sight and empty; else onto
  them as before. A first version (ring for every out-of-sight group) lost 36-44, -10.6%: 32.6k idle turns beside
  camps hidden in bushes.
- `FARM_KEEP` (off): keep the group whose camps we last saw for `FARM_KEEP_ROUNDS` (3) after they leave sight.
- `BANK_STICKY` (off): keep the altar bank.near set out for, `BANK_STICKY_ROUNDS` (12).

## Bench (debug with logs, 40 maps, both sides, vs v120; `replays/b122d`, `replays/b122e`)
| variant | run 1 | run 2 (seed) | banked | shuffle turns |
|---|---|---|---|---|
| **BANK_STEPS (= v122)** | 46-34 | 43-37 | +13.4 / +9.8% | 3.7% |
| BANK_STEPS + FARM_KEEP | 49-31 | 40-40 | +15.4 / +3.1% | 3.6-3.8% |
| all three (ring when tiles seen) | 37-43 | 39-41 | +9.4 / +4.2% | 2.6% |
| SPAWN_RING (tiles seen) | 43-37 | | +3.8% | 3.5% |
| SPAWN_RING (everywhere) | 36-44 | | -10.6% | 4.1% |
| FARM_KEEP | 42-38 | | +1.9% | 4.4% |
| BANK_STICKY | 43-37 | | +0.4% | 4.0% |
| v120 (reference) | | | | 4.0% |

All three fixes remove their loops (the full set cuts shuffling by a third) but win less than BANK_STEPS alone:
fewer back-and-forth turns does not by itself make faster farming. With the ring, a champion waits beside a group
that has not come back instead of going elsewhere; re-timing that group (so the route picks another) is the next test.
`analyze_shuffle.py DIR` counts the back-and-forth turns by branch.