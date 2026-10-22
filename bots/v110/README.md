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
