# v217-tp (v7215-tp + crystal fuel fixes)

v215-tp's travel planning scanned every stored jump end (up to 180) with engine reads on every armed travel turn, and
ran a full walking search from the champion whenever a plan existed: 11-19k fuel a planning call on maps with ~90
jumps (ambush, big-world, tangle, starvation), out-of-fuel turns 335 vs v213's 120 on those maps (team A side).

- Plan cache (TP_REPLAN 4 rounds for "no plan", TP_PAD_KEEP 12 for "walk to a launch tile", keyed by the goal board's
  count and first tile); go() asks the cache before building its field, so a cached walk builds one field, not two.
- Jump ends on one board plus a tile -> jump list (built at setup): the scan is a few board ops at the widest useful
  radius, shrunk only while it holds more than TP_JUMP_READS (12) ends; engine reads only for those.
- Launch-from-here check only within TP_NOW_NEAR (2) of a known jump end or off the walking map, landings 2-3 tiles off
  only, the minimum taken without a sort (fallback landings sorted only after a refusal).
- The walking search from us stops at the kept launch tiles (compute_to); the dasher hop search runs only once a plan
  exists; planning is skipped with under TP_PLAN_FUEL (40k) left in the turn.
- Crystal bought from the gold the build leaves (TP_FIRST false), before the lock.

Planning cost per call (log-only probe, team A vs v213): mean 2.6-6.6k (was 11-19k), p95 7-15k.
Fuel on the four worst maps, team A vs v213 (8 games): turns 35,270, mean 40.8k, out-of-fuel 123; v213 33,419 turns,
40.9k, 120; v215-tp 46.2k, 335. Not benched for wins (user: no bench this round).
