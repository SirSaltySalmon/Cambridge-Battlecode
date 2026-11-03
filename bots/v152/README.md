# v152

v152 = v150 with three of its small-map switches on (the benched `v150-c1`, sweep table in bots/v150/README.md); everything else as v149.

- `HOR_DIVE_FOLLOW = 1`: Heart of Ruin dives only onto a landing with 2 visible allies within 3 tiles (v146 port), every map.
- `LOCK_BUY = 2`, `LOCK_PLACE = true`: buy an altar lock first on one-altar maps, elsewhere from leftover gold under one
  shard; place it on any altar within 2 tiles that has no lock.
- `VENDETTA_3 = true`, `VENDETTA_KILL_RESET = true`: 3-champion draft is Heart of Ruin, Vendetta, Vesper; after a kill
  with the turn's cast unused, Vendetta resets and hits the next target in reach.

Self-play vs v149 (70 maps x both sides x 3 fuel-burn seeds, banked % minus the seed control): +8.7% overall
(HoR rule alone +12.3%, locks alone +7.6%, Vendetta alone +3.3%). Mean fuel per turn +2.6%, out-of-fuel turns
2644 vs 2076. Shipped to the platform to test locks and Vendetta against teams that are not v149.

Switches, code and the full result table: `bots/v150/src/constants.h` (top block) and bots/v150/README.md.
