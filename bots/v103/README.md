# v103

`v102` + dashers leave hole pockets, and the hop route spends dashes better (switches in `constants.h`). Not submitted.

## Why
On 01islands (v102 mirror, debug) the dashers (Vesper, Ren, Heart of Ruin) spent ~1,500 turns a side in hole pockets
(tiles our spawn cannot walk to), two thirds of them waiting on pocket respawns, while Varg and Willow could not come:
1. A dasher hops into a pocket (collect, bank at a pocket altar, chase a camp). FIX_WALK_TARGET then saw no walking
   path to the team's target and gave it the pocket's own group, so it waited out every 50-round respawn there.
2. A dasher in a pocket could lead: its `travel()` only reaches pocket groups, so it made a pocket group the team's
   target; the walkers detoured and Willow sat on spawn "boxed in".
3. The hop route spent a ready dash on a plain 2-step gain just before a hole, and took 2-3 hops where 1 would do; each
   extra hop waited ~7 turns at the next edge (226 edge-wait turns a game).
4. Standing on a group's spawn tile holds its respawn: two dashers stood on a pocket pair's tiles for 296 rounds.

## Changes
- `FIX_POCKET`: in a pocket (off `home_island()`), `walk_target` only swaps to a group that is up now; with none it
  keeps the team's target and the hop route dashes out. `pocket_recall`: recalls out instead when the dash route is
  over `POCKET_RECALL_SLACK` (5) steps longer than recall + walking from spawn, or no dash route exists, and nothing
  can break the recall. `pocketed()`: a dasher in a pocket neither leads nor points the team at loot while an ally
  that cannot dash is alive. A due group whose spawn tile we stand on (`farm.off`): step off to the wait ring.
- `FIX_HOP_FEW`: `few_hop_holes()` builds hop layers from the goals (walk-reachable, then +1 hop, ...; `HOP_LAYERS` 4,
  cached for `HOP_CACHE` 6 goal boards, built only with `HOP_LAYER_FUEL` spare) and keeps only holes that step down a
  layer; the hop route uses it unless the any-hop route saves over `HOP_EXTRA` (8) steps. A dash over plain ground
  (two walking steps reach the landing) is skipped while a route hole is within `HOP_KEEP` (8) of the landing.
- Not done (v104): splitting off to farm pockets the rest of the team cannot reach.

## Results
- 01islands, debug, v103 as team A vs v102: dasher turns in pockets 1,510 -> 417, pocket respawn waits ~800 -> ~20,
  hole-edge waits 226 -> 43; won 1,025-449 at r1,040 (v102 mirror: 1,007-598 at r1,202).
- Release bench, 35-map pool, both sides (70 games, `replays/b103`): **44-26** vs v102 (mirror 35-35), 16 results
  gained, 7 lost, summed gem margin +13,001. Lost flips: 00ambush-a, 00monolith-b, 00quadrants-b, 00rushbait-v2-b,
  01boss-battle-a, 01starfish-b, 01starvation-b. One seed only; release games are chaotic (see `analyze_seeds.py`).
- Fuel (`analyze_errors.py`): mean 25.3k vs 23.9k a turn (+6%), out-of-fuel turns 354 vs 340, fuelExhausted events
  122 vs 78 (most on 01big-world and 01yellow-brick-road). No bot errors.
