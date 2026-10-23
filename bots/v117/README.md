# v117

`v116` + `TEAM_CHASE` (a team-level chase cut-off); `RECALL_CANCEL` (drop a low-health recall once healed) is in the
code but off. Not submitted.

## Changes
1. `TEAM_CHASE` (`combat.cpp` `update_team_chase`): each champion, once a round, measures the team's gap (fewest tiles
   from any of us, not channelling, to any near enemy). Not improved for `TEAM_CHASE_ROUNDS` (5) rounds while fighting,
   fight becomes farm for `TEAM_CHASE_GIVEUP` (15) rounds: in the leader's shared mode (`sync_decision`) and in each
   champion's own rescore (`play`), so the team breaks off together. It never fires, and a drop ends, while engaged: an
   enemy within a step of one of us reaching it, an enemy channelling, or an enemy within `TEAM_CHASE_GUARD` (6) of an
   ally channelling. Gaps, not spawns: maps with the spawns side by side behave the same, and an enemy coming at us
   shrinks the gap. Debug: 60 drops on lanes, 38 on swirl, 11 on starfish.
2. `RECALL_CANCEL` (off; `turn.cpp`): a recall started at `hurt.recall` is cancelled at `RECALL_CANCEL_HEALTH` unless
   the purse reaches `RECALL_GOLD`. Only Willow heals and she skips recallers, so it needs `WILLOW_HEAL_SKIP_RECALL` off.

## Bench (release, 40 maps, both sides, vs v116; `replays/b117a`, `b117b`)
| variant | run 1 | run 2 (seed 1) | run 3 (seed 2) | total | banked (run 1) |
|---|---|---|---|---|---|
| **TEAM_CHASE 5 / 15 (= v117)** | 43-37 | 42-38 | 44-36 | **129-111** | +0.7% |
| TEAM_CHASE 8 / 15 | 43-37 | | | | -1.9% |
| RECALL_CANCEL 0.6 + Willow heals recallers | 42-38 | 39-41 | 38-42 | 119-121 | +5.8% |
| both | 36-44 | | | | -8.8% |
| v116 seeded vs v116 (noise) | 35-45 | | | | -10.8% |

One run swings +-5 wins and +-10% banked by chance (the noise row). The team cut-off won all three runs but is about
one standard deviation over even, with banked flat and K/D a little worse; the recall cancel is even.

# v116

`v115` with two chase cut-offs added behind switches, both off: no tested rule beat v115. Not submitted.

## How much we chase (debug, v115 vs v114, 8 maps; `analyze_chase.py REPLAY_DIR a`)
"Closing on an enemy" (`fight.hit ... closing`) is 8.2% of champion turns (lanes 21%). Chases of 6+ rounds: 182, 1,757
turns, 38% end with the target in reach. Not gaining on it after 3-6 rounds barely predicts a miss (33% still reach
it vs 28% of all chases): fleeing enemies stop or turn.

## Switches and bench (release, 40 maps, both sides, vs v115; `replays/b116a`, `b116b`)
| variant | record | banked | kills | deaths |
|---|---|---|---|---|
| CHASE_PATIENCE (drop after 4 rounds not gaining, 20 rounds) | 38-42 | -11.9% | -18% | +20% |
| CHASE_LINE_ALL (only what dies before home, anywhere) | 36-44 | -5.2% | -4% | +6% |
| SPAWN_CHASE_RANGE 11 | 38-42 | +3.1% | +1% | -1% |
| SPAWN_CHASE_RANGE 14 | 41-39 | +5.3% | +15% | -12% |
| SPAWN_CHASE_RANGE 18 | 42-38 | -3.7% | -2% | +2% |
| SPAWN_CHASE_RANGE 22 | 39-41 | -1.8% | +6% | -5% |
| PATIENCE + ring 14 | 38-42 | -3.4% | -6% | +6% |

Per-champion cut-offs cost deaths: one champion quitting is turned on, or leaves the others short. The ring is shared
by the team, but no ring size shows a trend, so 8 stays.

# v115

`v114` + `VESPER_KEEP_DASH`: with an enemy champion in sight, Vesper's walks no longer spend her dash, so it is there
for the kite and `escape()`. Not submitted. Prompted by the Live Love Larp ladder game (broken-curve, 449-1013) where
Vesper dashed into fights she could not survive.

## What the logs showed (debug, v114 vs v109 on fragment, Vesper's worst game: 16 deaths)
1. The fight score's emblem term counts only our emblems: a lone Vesper with 10 emblems scored "fight 12.0 (bodies -5
   1v2, health -5, power -5, emblems +30)" and stayed in.
2. Health and power are +-5 whatever the gap.
3. `engage_vesper` dashes to two tiles off the weakest enemy wherever it stands ("dash into range"), and
   `move_towards_closest` dashes whenever the dash is ready, so she reached fights with no escape and died on
   "hurt.dodge ... | move" turns.

## Switches (constants.h) and bench (release, 40 maps, both sides, vs v114; `replays/b115a`, `b115b`)
| variant | record | banked | team deaths | kills | Vesper deaths (ours / v114's) |
|---|---|---|---|---|---|
| EMBLEM_RELATIVE + VESPER_SAFE_DASH (1.5 / 100) + VESPER_KEEP_DASH | 28-52 | -23.6% | +48% | -35% | 396 / 216 |
| the same + GRADED_EDGES (15) | 23-57 | -34.5% | +66% | -43% | 415 / 196 |
| EMBLEM_RELATIVE only | 34-46 | -13.2% | +17% | -17% | 345 / 269 |
| SAFE_DASH (1.5 / 100) + KEEP_DASH | 39-41 | -7.5% | +5% | -4% | 334 / 307 |
| SAFE_DASH (1.5 / 100) only | 39-41 | -3.5% | +10% | -11% | 326 / 268 |
| SAFE_DASH lethal only (1.0 / 0) | 40-40 | +0.7% | +3.5% | -3.6% | 301 / 277 |
| **KEEP_DASH only (= v115)** | **40-40** | **+5.5%** | **-8.0%** | **+10.9%** | **316 / 340** |

A more cautious fight score loses: the team retreats more and is run down while retreating, and Vesper dies more, not
less. The safe dash-in is neutral at best. Keeping the dash is the only variant with better K/D for the team and for
Vesper, but the record is a tie (inside noise). `analyze_champs.py` counts deaths and kills per champion.

# v114

`v113w` (Varg for Marcella on five-player maps with 18+ wolves and 12+ gem clusters) plus `PASS_FIX` and a lenient
emblem gate on big elk packs and golems (`HARD_GATE`, free part = team size). Not submitted.

## Changes
1. `PASS_FIX` (`combat.cpp` `farm`): a cluster in reach on the way is a "free hit" only when its whole group is
   clusters. One inside an elk group wakes the elks (damage wakes the whole group) and earns nothing until they die.
2. `HARD_GATE` (`route.cpp` `hard_left`, `hard_ok`, `drop_hard`): a group is a target (travel table, `allowed`, farm's
   camp picks) only once the team's visible emblems reach `ELK_EMBLEMS` (1) per elk + `GOLEM_EMBLEMS` (3) per golem,
   less a free part: `HARD_FREE` (0) + the team size (`HARD_FREE_TEAM`) + one per `HARD_DECAY_ROUNDS` rounds (off).
   If nothing reachable passes, the groups needing the fewest stay in (`hard_floor`). Monoliths keep their own gate.
   On five only 6+ elk packs and golem groups wait; it fires on great-wall, stampede, starfish and the-farm.

## Bench (release, all 40 pool maps, both sides, one seed, vs v109; `replays/b114a`-`b114c`, merged in `replays/b114all`)
| variant | record | banked | r300 | r600 | r900 | camp deaths | camp damage taken |
|---|---|---|---|---|---|---|---|
| v113w | 47-33 | +16.9% | +3.4% | +12.9% | +20.1% | 191 | 2.59M |
| **PASS_FIX only (= v114)** | **47-33** | **+16.9%** | +11.0% | +28.4% | +17.3% | 174 | 2.49M |
| gate 1 / elk, free 1 | 46-34 | +13.0% | -1.5% | +19.0% | +17.3% | 170 | 2.33M |
| gate 1 / elk, free 2 | 42-38 | +5.7% | +6.0% | +18.9% | +10.4% | 178 | 2.30M |
| gate 1 / elk, free 3 | 42-38 | +7.7% | +14.9% | +19.2% | +16.0% | 173 | 2.40M |
| gate 1.5 / elk, free 1 | 33-47 | -10.7% | -33.4% | -4.1% | -4.2% | 130 | 2.10M |
| gate 1.5 / elk, free 2 | 43-37 | +8.2% | +2.9% | +22.3% | +16.6% | 160 | 2.30M |
| gate 0.5 / elk, free 2 | 43-37 | +10.9% | +4.8% | +19.9% | +10.9% | 178 | 2.50M |
| **gate 1 / elk, free = team size (= v114)** | **46-34** | **+20.3%** | **+15.4%** | **+31.4%** | **+27.4%** | 182 | 2.51M |
| gate 1 / elk, free 2 + 1 per 100 rounds | 40-40 | +2.9% | +0.5% | +19.9% | +8.5% | 187 | 2.33M |

The team-size gate fires on 4 maps (great-wall, stampede, starfish, the-farm): +1 win, +1907 margin there (starfish
+1865, both sides won). The 36 maps it never fires on moved -2 wins, -294 margin: that is release chaos from the code
change alone, so +-2 wins is this bench's noise. The fixed free parts lose because the packs they gate hold 17-100%
of a map's gems (stampede 100%, starvation 66%, pilgrimage 47%, starfish 35%); waiting for them costs more than the
deaths. Lenient runs: `replays/b114d`.

35-map only (before the pool grew), no free part: 1 / elk 27-43, 0.5 / elk 34-36, 1.5 / elk 27-43 (v113w 42-28).

The gate does what it says (camp deaths -15 to -25%; starfish debug game: 17 -> 4 camp deaths) but the team spends
those rounds on weaker groups, and 1-2 elk groups carry most gems on wolf maps (monolith, lanes, big-world, islands,
ambush all flip to losses with the plain gate). It helps hooks, elkring, corridors, starfish. `analyze_camps.py`
(this folder) counts camp deaths (death events with context `camp`) and camp damage per game.

Clusters inside a hard group cannot be picked off for profit: a group's reward needs every camp in it dead
(`camp_group_reward_available`), and on these maps clusters are almost always cardinally joined to the elks.

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
