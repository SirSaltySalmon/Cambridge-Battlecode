# v141

v141 = v140 + home-aware spawn rules (HOME_STALEMATE, REL_RING, SPAWN_RETREAT; RING_FARM built but off), in the v141
block of `src/constants.h` after HOME_CAMP_STEPS. Benches: all 70 maps in `maps/`, both sides, release, seed 0 plus
fuel-burn copies s1 and s2 of both bots (`bots/v133t/bench.py`), vs v140.

## The problem (the user's)
Our rules for keeping away from the enemy spawn ignore that our own spawn heals us. On 02treasure-box the spawns are 4
apart and the chase ring 8, so our whole spawn and the only bottom exit sat in their ring. In a v140 mirror the team
spent ~450 of its 500 champion turns of rounds 0-99 at home: an enemy two steps out of its spawn put us in fight
mode, the in-ring check dropped it ("home in 2"), `fight.leave` walked us out of the ring away from the exit, farming
walked us back. Team A reached the box (16 gem clusters and an altar) at round 188, team B at round 344; A won 1091-319
while dying ~29 times to B's 8.

## What it does
- **HOME_STALEMATE** (`near_enemies`): while we stand within 3 walking steps of our spawn, an enemy within 3 of its own
  is not near (the home-camper rule's 1 step, widened). Neither of us can die there, so it is no fight.
- **REL_RING** (`setup`): a ring tile we can reach is dropped unless it is at least 1 step nearer their spawn than
  ours. Tiles we cannot reach, and tiles far from both spawns (behind walls beside theirs), stay. Two bounded searches
  (`compute` with a max distance) mark the tiles sure to stay; full fields are built and read tile by tile only for
  the rest (none on far-apart spawns). The first version read every ring tile and spilled setup into an out-of-fuel
  turn on ~0.6 champions a game.
- **REL_RING_HOLES**: those sides are measured with holes crossable (walls only), since holes stop no attack, spell,
  sight or dash. With walking sides, 01gold-rush gave us the band just across the holes from their spawn, and we died
  there 217 times to their 85; with it, 130 vs 131.
- **SPAWN_RETREAT** (`retreat`): inside enemy reach within 2 walking steps of our spawn, retreat onto a free spawn tile
  (heals to full at turn end) or hold on it, hitting the weakest enemy in reach. Walking steps, not Chebyshev, so a
  spawn behind a wall never pulls us on a long walk. `steps_to_ally_spawn` is a lazily built field like
  `steps_to_enemy_spawn`.
- **RING_FARM** (off): in their ring with nobody to chase, farm instead of walking out.

## Results (vs v140)
| Bot | Games | Record | Banked | Per seed | K/D ours vs theirs |
|---|---|---|---|---|---|
| **v141 (exact ring, holes crossable)** | 840 | **398-415-27** | **-1.0%** | -5.8 / -1.6 / -2.8 / -1.0 / -5.0 / +11.1 | 13480/13864 vs 13386/13992 |
| exact ring, walking sides | 840 | 409-404-27 | +1.6% | +1.0 / +1.0 / +4.2 / -1.8 / -7.0 / +13.4 | 13404/14038 vs 13551/13900 |
| ring off (stalemate + spawn retreat only) | 420 | 209-197-14 | +3.3% | +5.3 / +6.3 / -1.5 | 6888/7011 vs 6770/7154 |
| ring bounded at 16 walking steps of theirs | 420 | 197-209-14 | +1.0% | +5.8 / -0.8 / -1.7 | 6692/6887 vs 6636/6956 |
| 1+2+3+4 (4 steps, RING_FARM on) | 420 | 198-208-14 | -3.9% | +4.7 / -7.8 / -8.1 | 6309/7479 vs 7200/6553 |
| 1+2 | 420 | 204-202-14 | +0.4% | +9.4 / -1.7 / -5.7 | 6630/6957 vs 6718/6863 |
| 1+2+4 at 2 steps (old ring code) | 420 | 209-197-14 | +2.1% | +9.6 / +1.3 / -4.0 | 6804/7022 vs 6778/7041 |
| 1+2+7 (REL_POCKET) | 420 | 201-205-14 | -0.6% | +5.8 / -2.3 / -4.9 | 6619/7050 vs 6808/6851 |

### Maps it seems to hurt
A debug pass on all 70 maps (`replays/b141dbg`, both sides) logged where any v141 rule fires: the stalemate on 6 maps,
the spawn retreat on 19, ring tiles dropped on 17; 40 maps never fire anything, so there v141 plays v140's rules and
only its setup fuel differs. Those 40 still showed 6 maps won <=1/6 and 4 won >=5/6 (the 30 active ones: 4 and 2):
fuel timing alone reshuffles whole maps, so a lopsided map is evidence only with a mechanism. The ones checked:
- reverse-spiral, starvation, broken-curve (bounded ring): the 16-step bound dropped ~50 tiles behind walls beside
  their spawn; fixed by the exact rule.
- 01gold-rush: walking sides gave us the band across the holes from their spawn; fixed by REL_RING_HOLES.
- eight, yellow-brick-road, ambush, stampede, quarry, famine: no rule fires (or one tile / one retreat): reshuffles.
- rushbait: flips with every build (0/6 to 3/6), turning off any one switch flips a different seat.

Seed 0 ablations of 1+2+3+4 (+4.7%): stalemate off +5.5, REL_RING off -0.5, RING_FARM off +9.0, SPAWN_RETREAT off
+8.0. SPAWN_RETREAT at 2 steps changed ~17 games a seed vs 1+2: 11 turned to wins, 6 to losses.

Target maps (seeds 0-2): 02treasure-box 5/6 with the bounded ring, 3/6 (+400 gems) with the exact one; 00gem-fields
2/6-3/6; 00rushbait-v2 0/6-3/6 depending on the build. The home bounce itself is gone (rounds 0-99 at home: ~450 of
500 champion turns in v140, ~210 in v141), but the map's result swings with every build.

Fuel: per turn -0.2 / +1.1 / +0.2%; out-of-fuel turns in rounds 0-19 437 vs 391 (old ring code 514); all but
01boss-battle 1464 vs 1437. 01boss-battle is a 0-0, 2,000-round game where whoever sits as team B runs out of fuel
500-1,500 times (v140 too); it dominates out-of-fuel totals and is not from these switches.

Idea 7 (REL_POCKET, built as v142 and removed): the draft's aggressive pocket left out a group within 12 walking steps
of their spawn only when it was also nearer theirs than ours. It changed 4-5 games a seed (swirl, elk-arena,
yellow-brick-road, reverse-spiral) and lost 4 of the 5 it flipped.

Replays: `replays/b141s*` (round one), `replays/b141bs*` (round two), `replays/b141fs*` (final), `replays/b141x`
(rushbait ablation).
