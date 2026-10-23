# v113w (was v113)

`v111` with Varg in Marcella's pick on five-player maps packed with wolves and gem clusters. Not submitted.

## Changes
1. Draft (`draft.cpp` `varg_map`): on five, Varg takes Marcella's slot when the map has at least `VARG_WOLVES` (18)
   wolves and `VARG_GEM_CLUSTERS` (12) gem cluster camps. Three and four are unchanged. It switches 7 of the 35 maps:
   lanes, monolith, big-world, corridors, islands, starfish, yellow-brick-road.
2. `VARG_CLAIM` (`route.cpp` `marcella_collects`): Varg gets Marcella's funnel: he collects second, after a funnelled
   Vesper. Only one of them is ever drafted.
3. Varg keeps his own build (Treasure, Jungle, Bruiser Strength). `VARG_MARCELLA_BUILD` (off) gives him Marcella's.

## Bench (release, both sides, vs v109; `replays/b113a`, `b113b`, `b113c`)
First try (`bots/v113a`, `bots/v113b`): Varg for Marcella on every three/five map with 45+ gem and gold cluster camps
(13 maps). One seed: v111 14-12, Varg's build 14-12, Marcella's build 12-14. With 2 fuel-burn seeds more, v111 44/78
vs Varg (own build) 36/78. Seeds barely change games, so the per-map effect is real (wins of 6 per map):
| better with Varg | worse with Varg | same |
|---|---|---|
| monolith 0 -> 6, islands 3 -> 6, lanes +1, big-world +1 | boss-battle -5, the-farm (three) -5, elk-arena -3, pilgrimage -3, expanse -3 | quadrants, split, fracture, yellow-brick-road |

The losers are gold-cluster maps with few gem clusters or wolves, and the only three-player map; the winners all have
18+ wolves and 12+ gem clusters. v113 is that rule (fitted on these games), checked on 3 seeds:
| map (both sides, 6 games) | v111 | v113 |
|---|---|---|
| monolith | 0/6 | 6/6 |
| islands | 3/6 | 6/6 |
| lanes | 3/6 | 4/6 |
| big-world | 3/6 | 4/6 |
| yellow-brick-road | 3/6 | 3/6 (margin +165) |
| corridors (new) | 3/6 | 3/6 (margin -120) |
| starfish (new) | 3/6 | 3/6 (margin -34) |
| **total** | **18/42, -3175** | **29/42, +7598** |

The two maps outside the fitting set are neutral, so the rule is not shown to generalise; the gain is from monolith
and islands. Unswitched maps play identically to v111 (pilgrimage A checked: same 907 margin).

# v111

`v110` with the draft reordered: three is Heart of Ruin, Marcella, Vesper; four is Heart of Ruin, Vesper, Willow, Ryth;
five is Ryth, Heart of Ruin, Willow, Vesper, Marcella. Adds `MARCELLA_FUNNEL_EMBLEMS` (0 = no cap, kept): Marcella
collects second only while she owns fewer emblems than this. Not submitted.

## Cap sweep (release, the 28 three- and five-player maps, both sides, one seed, vs v109; `replays/b111a`)
| cap | record | banked | gold | kills | deaths | Marcella emblems (mean) | her gem share | flips vs no cap |
|---|---|---|---|---|---|---|---|---|
| **none (= v111)** | **33-23** | +17.6% | +21.8% | 1375-1053 | 1200-1519 | 19.4 | 35% | - |
| 6 | 31-25 | +22.6% | +19.0% | 1294-998 | 1143-1429 | 12.8 | 23% | 2 |
| 12 | 33-23 | +19.3% | +21.0% | 1345-1034 | 1176-1487 | 14.7 | 28% | 2 |
| 15 | 32-24 | +17.3% | +18.6% | 1339-1086 | 1236-1487 | 15.9 | 30% | 3 |

The caps bind (she has 12+ emblems in 42 of 56 games uncapped) and move her share of the gems, but only 2-3 games
change each, all inside noise; 01boss-battle as A is lost by every cap. Vesper's emblems are unchanged (28-30).

# v110

`v109` (= v106) for the Focus-nerf patch (Ren's stun 1.25 -> 0.75 x Focus, Stellar and Heart of Ruin cut, Vesper,
Vendetta and Tavi more health): Marcella takes Varg's pick and Ryth takes Ren's. Not submitted.

## Changes
1. Draft (`draft.cpp`): five is Marcella, Ryth, Heart of Ruin, Willow, Vesper; three and four are Heart of Ruin, Ryth,
   Vesper (+ Willow). With `HOR_SMALL_SWAP 0` Varg was only on five, so Marcella only plays five-player maps (22 of 35).
2. Marcella kit (`Bot::marcella_cast`, `kits.cpp`): her 2 x Focus spell (range 4, excludes walking) goes off beside
   the target (a camp, or a champion once her reset is on cooldown) or in a fight when the target is more than a step
   away; beside an enemy champion she attacks, casts the enhanced reset (`MARCELLA_STUN_RESET`: the second hit stuns)
   and hits again. Build: Treasure Strength, Jungle Health, Durability Health, then Power Strength. Priority 3.
3. Ryth acts first (priority 7, Ren's old slot) so his area stun lands before the team's damage. Ren's special rules
   (rush, carry bag, fight-alone check) no longer apply.
4. `MARCELLA_CLAIM` (`route.cpp`): Marcella collects second. A funnelled Vesper within `VESPER_CLAIM_RANGE` still
   collects first; otherwise a Marcella within that range is handed the claim, claims nearby loot herself
   (`vesper_claim`, now for both), is yielded to by `self_claim`, and joins a reward an ally already stands on.

## Bench (release, 35 maps, both sides, one seed, vs v109; `replays/b110a`)
| team size (maps) | record | banked | kills | deaths |
|---|---|---|---|---|
| all (35) | 36-34 | +13.9% | +38.9% | -24.5% |
| five (22, Marcella + Ryth) | 24-20 | +21.5% | 1177 vs 736 | 890 vs 1313 |
| four (7, Ryth) | 7-7 | -2.0% | 181 vs 185 | 217 vs 212 |
| three (6, Ryth) | 5-7 | +7.5% | 243 vs 232 | 239 vs 258 |

Gold +25.5%, banked r300 / r600 / r900 +4.6% / +18.1% / +13.7%. The record is inside noise (+-2 games); most maps
still split by side. Both-sides losses: 00elkring, 00split, 01gold-rush, 01the-farm.

## Previous: v106

`v105` + the monolith gate by team size, a longer loot funnel to Vesper, and Vesper shopping at every emblem after
her core. Not submitted.

## Changes
1. `MONOLITH_BY_SIZE` (`Bot::monolith_emblems`, `constants.h`): nobody hits a monolith until the team owns 15 emblems on
   five, 18 on four, 20 on three (v105: 12 on every size). It gates the route (`allowed`, `drop_monoliths`); the mage
   monolith job is already off (`monolith_delegated` is false since v94).
2. `VESPER_FUNNEL_EMBLEMS = 12`: Vesper collects first (claims, `vesper_claim`, `self_claim` yields) until she owns 12
   emblems (v105: her 4-emblem core). Away from her (beyond `VESPER_CLAIM_RANGE`) anyone collects. The aggressive gate
   and `vesper_done` still use the 4-emblem core.
3. `VESPER_EMBLEM_RECALL`: after her core Vesper recalls at `RECALL_GOLD` (150, one emblem) with everyone's banking
   rules, not at `VESPER_RECALL_GOLD` (600).

## Bench (release, 35 maps, both sides, one seed, vs v105; `replays/b106a`, `b106b`)
| variant | record | banked | gold | kills | deaths | banked r300 / r600 / r900 |
|---|---|---|---|---|---|---|
| monolith gate only | 35-35 | +1.2% | +0.7% | +0.8% | -1.2% | +0.0% / +0.0% / +1.1% |
| gate + funnel 8 | 41-29 | +18.4% | +13.6% | +19.8% | -15.1% | -0.5% / -2.0% / +6.0% |
| **gate + funnel 12 (= v106)** | **42-28** | **+16.7%** | **+18.3%** | **+29.2%** | **-19.7%** | +0.5% / -2.1% / +8.9% |
| gate + no funnel cap | 37-33 | +1.3% | +4.3% | -3.0% | +4.9% | +0.4% / -3.1% / -0.2% |
| funnel 8, no gate | 40-30 | +19.0% | +15.7% | +19.5% | -14.5% | -0.5% / -2.0% / +5.3% |
| funnel 12, no gate | 42-28 | +14.8% | +18.2% | +27.8% | -18.9% | +0.5% / -2.1% / +8.9% |

Funnel variants all recall at every emblem after the core. The gate is neutral: with or without it the funnel games
are nearly identical, and boss-battle is the only map it changed (funnel 12 lost boss-battle 0-0 and 0-585 without
it, only the B-side 0-0 with it). It is kept because it never cost a game. Fuel is flat (mean ~26.5k a turn, out of
fuel 470 turns vs v105's ~456).

## Gem-starved discount (not kept; `replays/b106c`)
v106 with the monolith bar 2 lower on gem-starved maps (13 / 16 / 18): 42-28 vs v105, the same record. Only one of
the 70 games changed: boss-battle as team A, still won but 500-449 instead of 849-42. Contest was identical down to
the gold (600-0 both sides), so the lower bar never changed when the team went for its monolith there.

Only contest is gem-starved (setup log over the 35 maps; boss-battle is not, so its b106c change was release noise).
A 3 lower bar (12 / 15 / 17) on contest (`replays/b106d`, `b106e`): unseeded, A side 800-0 vs v106's 600-0, B side
identical; with 3 fuel-burn seeds each, v106 and the -3 build played identical games on every seed and side (wins
600-200, 600-0, 600-0 / 600-0, 600-0, 200-0). The bar is not what holds the team back on contest.
