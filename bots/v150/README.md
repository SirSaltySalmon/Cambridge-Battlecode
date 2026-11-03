# v150 (small-map switches)

v150 = v149 + the small-map switches at the top of `src/constants.h` (block "v150 switches"). With every switch off it
plays as v149 (20/20 identical games, manifest `b150id.txt`; +3 fuel a turn). Built from the user's ladder analysis:
small close-spawn maps, 3-4 champions, one altar. bots/v152 ships three of them on (the benched `v150-c1`).

Not to be confused with bots/v150-jack: Jack's v149 + Optuna fight trial 43, which was bots/v150 on jack-claude and was
relabelled when the branches were merged into final-stretches.

## Switches (all off here)
| Switch | Rule |
|---|---|
| `LOCK_BUY` (1, 2), `LOCK_PLACE` | Buy an altar lock when shopping: 1 = one-altar maps first, before the build; 2 = also on other maps from gold left under one shard. A lock we carry goes on an unlocked altar within 2 tiles. |
| `SMALL_BANK`, `SMALL_HUNTED`, `SMALL_BANK_THREE` | One-altar maps (and every three-champion map with `_THREE`) bank a bag of SMALL_BANK gems, SMALL_HUNTED with an enemy in sight. |
| `SMALL_FUNNEL_TEAM`, `SMALL_FUNNEL_EMBLEMS`, `SMALL_CLAIM_NEUTRAL` | Smaller teams funnel loot to Vesper only up to SMALL_FUNNEL_EMBLEMS emblems; with `_NEUTRAL` nobody else collects first either. |
| `VENDETTA_3` | Three-champion maps draft Heart of Ruin, Vendetta, Vesper (Vendetta in Marcella's place). |
| `VENDETTA_KILL_RESET` | After a kill with the turn's cast unused, Vendetta resets and hits the next target in reach (a kill refreshes cooldowns, not the turn's one cast: probe 82/82). |
| `HOR_DIVE_FOLLOW` (1, 2) | v146 port: a Heart of Ruin dive landing counts only with 2 visible allies within 3 tiles; 1 = every map, 2 = teams of four or fewer. |
| `HOR_DIVE_HEALTH`, `HOR_HOLD_SMALL`, `HOR_SMALL_FOUR` | Teams of four or fewer: no dive under this health share / no staying in a fight under the recall bar; `_FOUR` limits both to teams of exactly four. |
| `SPLIT_SMALL`, `SPLIT_ZONE_SMALL` and the `SPLIT_*` numbers | The v86 split for small teams once everyone owns SPLIT_MIN_EMBLEMS emblems; `SPLIT_ZONE_SMALL` 1 lets it use the middle zone. |

## Tools in this folder
- `analyze_maps.py DIR[,DIR...]`: per map set (all, bad = the 14 ladder maps we lose most, best = our 4 best, three /
  four / five champions, one altar): record, banked %, K/D, camp deaths, out-of-fuel turns, tributes, locks and more.
- `compare_seeds.py ROOT CONTROL VARIANT...`: each variant against the seed control, seed by seed.
- Map facts (champions, altars, spawn-to-spawn steps) come from bots/zz-facts.

## Sweep (variant folders removed in final-stretches)
Each variant was a copy of this folder with only the listed constants changed, plus fuel-burn seed copies `-s1` / `-s2`
(the same bot with `{ volatile int sink = 0; for (int i = 0; i < 300; ++i) sink += i; }` before `cambc::end_turn()` in
`src/main.cpp`, which reshuffles fuel timing). Bench: vs v149, all 70 maps x 2 sides x 3 seeds (420 games), release.
Numbers are banked % (ours / theirs - 1) minus the seed control's on the same seed (control = `v150-s1` / `-s2`: v149's
code with the burn loop, -8.2% alone), averaged over the 3 seeds; then wins of 420. Manifests `b150.txt` (single
switches), `b150c.txt`, `b150d.txt`, `b150e.txt` (combinations), `b150u.txt` (split with the middle zone); replays
`replays/b150*/<variant>[-s1|-s2]/`, recomputed for this README with `compare_seeds.py`.

| Folder | Settings | All | Three | Four | Five | One altar | Wins |
|---|---|---|---|---|---|---|---|
| v150-hf1 | HOR_DIVE_FOLLOW 1 | **+12.3** | +15.8 | +16.7 | +10.2 | +23.3 | 210 |
| v150-hf2 | HOR_DIVE_FOLLOW 2 | +5.1 | +15.8 | +13.7 | 0.0 | +13.6 | 199 |
| v150-hh | HOR_DIVE_HEALTH 0.5 | +1.3 | -3.9 | +8.8 | 0.0 | -3.9 | 202 |
| v150-hhold | HOR_HOLD_SMALL | +1.4 | -9.5 | +15.0 | -0.3 | -7.6 | 197 |
| v150-lk1 | LOCK_BUY 1, LOCK_PLACE | +0.9 | +9.9 | +2.1 | -1.4 | +4.8 | 195 |
| v150-lk2 | LOCK_BUY 2, LOCK_PLACE | +7.6 | +6.0 | +11.2 | +6.7 | +4.8 | 209 |
| v150-vd | VENDETTA_3 | +3.0 | +22.9 | -1.3 | 0.0 | +2.3 | 201 |
| v150-vdr | VENDETTA_3, VENDETTA_KILL_RESET | +3.3 | +26.7 | -1.3 | 0.0 | +0.6 | 201 |
| v150-bk20 | SMALL_BANK 20 | -1.0 | -3.0 | -5.5 | +1.1 | -5.5 | 192 |
| v150-bk20h | SMALL_BANK 20, SMALL_HUNTED 30 | -0.7 | -2.6 | -5.5 | +1.5 | -5.2 | 192 |
| v150-bk20h3 | bk20h + SMALL_BANK_THREE | -1.5 | -5.1 | -2.4 | -0.5 | -5.2 | 187 |
| v150-bk30h | SMALL_BANK 30, SMALL_HUNTED 30 | -0.4 | -2.4 | -4.6 | +1.5 | -3.4 | 192 |
| v150-fb | SMALL_FUNNEL_TEAM 5, SMALL_FUNNEL_EMBLEMS 0 (no funnel), SMALL_CLAIM_NEUTRAL | -18.5 | -56.1 | -35.6 | +0.6 | -17.1 | 152 |
| v150-fc | SMALL_FUNNEL_TEAM 5 (cap 4), SMALL_CLAIM_NEUTRAL | -2.0 | -20.3 | +3.7 | +0.6 | -7.8 | 186 |
| v150-sp2 | fc + SPLIT_SMALL | -1.8 | -18.6 | +4.7 | +0.2 | -10.6 | 184 |
| v150-sp3 | sp2 + SPLIT_MIN_EMBLEMS 3 | -1.5 | -18.4 | +6.0 | +0.2 | -8.5 | 184 |
| v150-sp2u | sp2 + SPLIT_ZONE_SMALL 1 | -1.8 | -19.4 | +5.2 | +0.2 | -10.7 | 186 |
| v150-sp3u | sp3 + SPLIT_ZONE_SMALL 1 | -1.5 | -18.7 | +6.1 | +0.2 | -8.7 | 186 |
| v150-c1 | hf1 + lk2 + vdr (= bots/v152) | +8.7 | -11.9 | +13.9 | +12.4 | +4.3 | 207 |
| v150-c2 | hf1 + lk2 + vd | +6.7 | -21.8 | +13.9 | +12.3 | -3.8 | 206 |
| v150-c3 | c1 + HOR_HOLD_SMALL, HOR_SMALL_FOUR | +7.2 | -11.9 | +6.5 | +12.4 | +3.1 | 206 |
| v150-c4 | c1 + HOR_DIVE_HEALTH 0.5, HOR_SMALL_FOUR | +8.5 | -11.9 | +12.8 | +12.4 | -0.1 | 210 |
| v150-c5 | hf1 + lk2 | +8.6 | -12.8 | +13.9 | +12.3 | -6.0 | 209 |
| v150-c6 | hf1 + vdr | +11.0 | +10.5 | +13.7 | +10.2 | +10.3 | 209 |
| v150-c7 | lk2 + vdr | +8.7 | +12.4 | +11.2 | +6.8 | -3.9 | 210 |
| v150-c8 | hf1 + lk1 | +7.7 | +1.6 | +12.1 | +7.7 | -6.0 | 202 |

Read-out:
- HOR_DIVE_FOLLOW on every map (hf1) is the robust keeper: positive on every seed and set, fuel +2.0% a turn.
- Locks pay only bought everywhere (lk2, ~7 locks a game, fuel level), not on one-altar maps alone (lk1).
- Vendetta on three-champion maps gains there (+23%, +27% with the kill reset) and nothing elsewhere.
- The small funnel, the small split and SMALL_BANK all lose; the HoR health / hold rules gain on four and lose on three.
- Combinations do not add up: hf1 alone beats every combination. Three-champion results swing +-20% from whole maps
  flipping between seeds and seats, so the per-set interactions there are mostly noise.
- The user shipped c1 (hf1 + lk2 + vdr) as bots/v152, to test locks and Vendetta against teams other than v149.

`bots/zz-lock` (also removed): an early lock probe, v149 + LOCK_BUY 1 + LOCK_PLACE only, played against v149 on 01hooks
and 02the-pack (`zzlock-manifest.txt`) to check locks are bought, placed and block enemy tributes before the sweep.
