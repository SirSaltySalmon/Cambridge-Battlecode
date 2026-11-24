# v215-tp (v213 + TP: teleport crystals)

Ladder 02treasure_box vs Noobot (match 854ca201, 58 vs 1,313 gems at r730): the altar sits in a box inside a box. They
bought 24 crystals (Vesper 9, Marcella 6, HoR 5, Ryth 4), hopped the walls to the altar, tributed, recalled, and
bought the next crystal on spawn. We never bought one.

A crystal channels 3 turn ends, then lands within 3 tiles (Chebyshev) through any terrain; stuns and death cancel it,
damage does not, and the channel can't be cancelled. Three channel turns cover what three steps do, so it only pays
where terrain stretches the walk (walls, holes, pockets).

- Draft (crystal.cpp `encode`, first draft turn): three bitboard wavefronts (to the altars' tribute rings, our spawn,
  every camp tile). Layer k: walking reach F_k, crystal reach D_k = dilate3(F_k), one-crystal reach
  O_k = walk(O_{k-1}) | F_k | D_{k-3}. Tiles first reached through D are launch tiles; those saving TP_KEEP_SAVE+ steps
  are thinned to TP_MAX_JUMPS jumps (launch, landing), 3 bytes each, at the tail of the 1,024-byte draft memory with the
  map's value (gem-weighted turns saved per trip, group to altar plus spawn to group). Board ops cost the same on any
  map size; guest loops over tiles cost ~300 fuel a tile a pass (too dear on 100x100). Stops with TP_DRAFT_RESERVE left.
- Shop: one crystal (10 gold) on spawn or while dead, on maps with value >= TP_BUY_VALUE (TP_FIRST: before the build).
- Travel (actions.cpp go -> crystal::travel): holding a crystal, no enemy champion in sight, mode not FIGHT/RETREAT,
  health >= TP_HEALTH, not in awake camps' reach: compare walking with "launch here" and with "walk to a stored jump end,
  launch, walk on" (both directions; one BFS from us, ~3k fuel). If the best beats walking (hops counted for dashers)
  by TP_SAVE turns: launch, or retarget the walk to that launch tile.
- TP_RECALL (off; variant v215-tp-rc): right after a tribute with no crystal, recall when recall + spawn-to-target with
  a crystal beats walking by TP_RECALL_MARGIN.

Draft fuel (first draft turn, 70 maps): max 7.2M of 10M (01big-world), at least 2.8M left; wavefronts finished on
every map. Crystal value per map: `tp_values.txt`.

Bench: see results below. `analyze_tp.py tp_values.txt NAME=DIR,... NAME=DIR,...` summarises bench folders.
