# v98

`v97` + dashing over holes (FIX_HOP) and taking the monolith that shuts the only altar (FIX_ALTAR_GATE). Overall
level with v97 (65/126 against v97's own 64/126 over three seeds); each feature wins the maps it targets.

## Changes

- **FIX_HOP** (`actions.cpp` `thin_holes`, `hop_route`): a champion with a dash (Vesper, Ren, Issac, Vendetta, Heart
  of Ruin; read from its kit, so any dasher) paths through "thin" holes, a hole with standable ground on both opposite
  sides, which one two-tile dash crosses. It takes that route when it saves at least `HOP_SAVE` (10) steps plus the
  dash's remaining cooldown, or when walking cannot reach the goal: walks to the edge, dashes across when ready,
  waits at the edge while it recharges, and falls back to walking when a ready dash finds no legal landing.
  - The hop field is a second distance field, so it is only built with a thin hole within `HOP_NEAR` (12) tiles, a
    walk of at least `HOP_SAVE` + 2 steps and `HOP_FUEL` (30k) fuel left. A first gate that looped over the goal tiles
    ran 10x more turns out of fuel (12-14 wins of 42).
  - Fixes on the way: an edge wait now keeps the turn (returning false let the next branch walk us off and the dash
    hop us back each turn); a dash must gain 2 tiles; a recharge counts against the hop (fracture: 217 edge waits).
- **FIX_ALTAR_GATE** (`route.cpp` `split_altars`, `altar_open`, `all_altars_shut`, `best_goal`, `drop_monoliths`):
  an altar whose every standable neighbour is a camp spawn tile is gated by that group (boss-battle only: the altar
  at (25,20) behind the monolith pair). While every altar is shut, the gate group becomes the goal as soon as the
  monolith rules allow it (`MONOLITH_EMBLEMS`, grouped) instead of only as a last resort. A gate counts as shut while
  we saw its group up within `GATE_MEMORY` (30) rounds. Worked out at setup: a few tile reads per altar, cheaper than
  a draft-memory block.
  - Tried first: also skipping shut altars for banking (bags kept, farm on). It banked 0 in both test games: the
    enemy killed the monolith out of our sight, and our carriers were far from the altar when it opened. Carriers
    still walk to the shut altar, so they are beside it when it opens.

## Results

Release-mode games are chaotic: two builds that differ only on boss-battle had 6 of 42 results differ. So each build
ran with a fuel-burn seed (300 x seed loop iterations a turn, bench copies only) to sample that chaos:
`analyze_seeds.py` (per map, all seeds). Against v97 (each 42 games x 3 seeds; v97 seeds are v97 vs v97):

| map | v97 W/G, mean margin | v98 W/G, mean margin | why |
| --- | --- | --- | --- |
| concentrated-wealth | 3/6, -14 | **6/6, +945** | hops over the hole row: walking 62% -> 27-33% of turns |
| boss-battle | 4/6, +32 | **5/6, +370** | the gate monolith goal (as B alone: 462-135 against hop-only, a mirror loss) |
| monolith | 3/6 | 6/6 | not traced |
| broken-curve | 3/6 | 0/6, -863 | no holes, no gate; debug games are identical to v97: fuel-timing chaos |
| fracture | 6/6 | 3/6 | dashers still wait at a chain of holes (two hops 8 turns apart) |
| all 21 | 64/126 | 65/126 | |

- Fuel: out-of-fuel turns 265-351 per 42 games against v97's 200-300 (~+20%); mean fuel ~+2%.
- The seeds do not shake every map: broken-curve gives the same -863 in all three, and its debug games match v97
  line for line. Read per-map flips only where a mechanism shows in the logs.
- Files: `results-v98-vs-v97.txt`, `replays/bench-noise*/`, `replays/bench-v98/`.

## Follow-ups

- Chains of holes (fracture): count crossings on the hop route (each after the first costs a recharge).
- Fuel: the hop field is still the main extra cost; it could be cached per goal for a few turns.
- v99: the aggressive route (see the talk after v98).