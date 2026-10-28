# v142

v142 = v141 + the user's golem gate (GOLEM_GATE), GOLEM_LAST off, and NOTHING_ROAM, in the v142 lines of
`src/constants.h` (after HARD_DECAY_ROUNDS; GOLEM_LAST's comment). Benches: all 70 maps in `maps/`, both sides,
release, seed 0 plus fuel-burn copies s1 and s2 of both bots (`bots/v133t/bench.py`), vs v141.

## The problem (the user's)
v137's GOLEM_LAST pulled golem groups out of every target table while an easier group was up, because the team took
golems and died to them. The deaths came from the gate, not from the priority: v141 asked 3 emblems per golem less one
per champion present, so a lone champion with 2 emblems (or two with 1) could open a golem.

## What it does
- **GOLEM_LAST off**: golems are targets again with wolves and elks around.
- **GOLEM_GATE**: per golem in the group, the champions present (PRESENT_GATE's count) need, among them:
  | present | 1 (melee) | 1 (ranged) | 2 | 3 | 4+ |
  |---|---|---|---|---|---|
  | emblems per golem | 6 | 10 | 6 | 5 | 4 |

  No free part for golems; elks keep 1 each less one per champion present, added on top. The staging check
  (`stage_group`) uses the same rule with who is within 8 of the group.
- **hard_floor**: the fallback that lets the easiest group in when nothing passes never lets a golem group in
  (monoliths were already outside it: they keep their own gate).
- **NOTHING_ROAM**: with nothing in the target table, `pick_target` heads for the group nearest the map centre (one we
  may farm and is up first) instead of keeping the gated target: gold there, enemies to meet. Fights are still taken
  only on the fight score.

## Model behind the numbers
`golem_calc.py` (session scratchpad, not kept): a golem (5,000 health, 120 a hit) against v141's builds in shop order,
basic attacks + resets + spells per 8 rounds, stuns skipping golem hits, shields, Willow's heal, Jungle / Durability /
Monster, tank swaps spreading the hits, finishing over FINISH_HEALTH. Emblems needed, Vesper funded first:
- solo: HoR 5, Varg 5, Ryth 6, Marcella 8, Vesper 10, Prism 16, Willow never
- pairs: 1-6 (melee pairs and Willow pairs 1-3, Vesper pairs 5-6)
- three or more: 0-1
- two golems: pairs 5-15, three 2-9, four or five 0-1; four golems: four 14-15, five 7-9

It is optimistic (no walking, no enemies, perfect swaps), so the user's numbers sit above it.

Monoliths (unchanged): start with every living ally within 6 tiles at 80%+ health, no enemy in sight, and the team
owning 15 / 18 / 20 emblems (five / four / three); keep going with 3 within 6.

## Golem groups (engine dump of all 70 maps)
1 golem + clusters on crater, dunes, lost-world, oasis (2), quadrants (2), reverse-spiral (2, with 2 wolves),
road-of-riches (2), split (2), crossing (2), cuts (2), famine (2), geodes (2), harvest (4), uncharted-lands (2);
golem + elk on treasure-box; 2 golems on fields (2) and harvest's centre; 4 golems on great-wall and tangle.

## Results (vs v141)
| Bot | Games | Record | Banked | Per seed | K/D ours vs theirs |
|---|---|---|---|---|---|
| **v142** | 420 | **216-189-15** | **+5.0%** | +4.0 / +3.5 / +7.5 | 6789/6766 vs 6502/7049 |

- Golem maps (108 games): 59-49, +6.9%; golem clears 194 vs 39; camp deaths near golems 19 vs 11 (10% of clears
  vs 28%). Other maps (312): 166-146, +4.2% (only NOTHING_ROAM differs there; its ablation was cut short by a full
  disk after 22 games).
- 00road-of-riches 0-6 (v141 won 4 of 6 there): its golems sit in the far corners and v142 walks out to them from
  ~r680. A pricing problem (walk vs 71 gems), not the gate.
- Fuel (all but 01boss-battle): +1.0% per turn, out-of-fuel turns 7616 vs 7232. 01boss-battle: v142 as team B runs
  out 1559 times a game (v141 1062, against any opponent): the known boxed-in-by-the-monolith state (two champions
  repeat failed path searches every turn) starts at ~r430 instead of ~r850. No golems there; roam never fires.
