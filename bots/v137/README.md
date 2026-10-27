# v137

v137 = our v135 + the fixes from the mike branch's v135 (built there as v134-move, kept here as `bots/v135--mike`) and
`bots/v135-golem`, ported as switches in the "v137 switches" block at the top of `src/constants.h`, plus fuel trims of
v135's own additions. Benches use all 70 maps in `maps/` (the 65 `0*` maps and the 5 `adgato-gemcombat*`), both sides,
release, seed 0 plus fuel-burn copies s1 and s2 of both bots (420 games a line). Bench tools: `bots/v133t/bench.py`.

## What came in, and from where
| Switch | From | What it does | Code |
|---|---|---|---|
| GOLEM_LAST | v135-golem | Golem groups leave every target table and `allowed()` while a wolf / elk / gem-cluster group in reach is up within GOLEM_IDLE (10) rounds of arrival; kept when mid-clear, its reward lies there, or it gates every altar. | route.cpp (`drop_golems`, `golems_held`, `golem_keeps`) |
| VESPER_ESCAPE | v135-golem | Vesper's hurt dodge ranks landings and steps by the enemies' walking steps (capped field) and reach margin, dashes first, fires at the weakest in reach when no step escapes. Skipped under VESPER_ESCAPE_FUEL (20k) left. | combat.cpp (`vesper_escape`), turn.cpp |
| TARGET_HOLD | mike v135 | A new shared target holds 6 rounds (not when it went down, the new one is mid-clear, or a kill raid / split starts). | route.cpp (`sync_decision`) |
| INVADE_OWN_KEEP | mike v135 | Invasions are rated against our current own-side target too. | phase.cpp |
| SHARE_LOOT_TEAM, SHARE_LOOT_COOL | mike v135 | A follower points the team at loot only with an ally within TEAM_RANGE (checked before the pick, so a straggler skips the travel table), and not again within 20 rounds. | route.cpp (`share_loot`) |
| CAPTURE_NEWS | mike v135 | With no claim open, bytes 3 / 6 carry "group s started its respawn timer at round r" (CAPTURED_MARK); readers fix their respawn record. Our tank-on-duty bits are in byte 0: no clash. | route.cpp |
| FINISH_TANK | mike v135 | finish_ok's tank test uses the gap we will have while hitting. | phase.cpp |
| BANK_OPEN | mike v135 | Bank walks skip altars whose submit tiles in sight are all fixed or living camps; camp tiles are no bank goal. Rewritten as board ops (mike's per-tile loop was ~75 host reads per altar). | tribute.cpp (`altar_usable`) |
| HOP_STICKY | mike v135 | A dasher that took a hop route keeps planning hops next turn (beside our HOP_WHEN_CUT). | actions.cpp |
| CAPTURE_STAND | mike v135 (CAPTURE_WAIT, half) | A cleared, unclaimed reward is stood on, not stepped off by FIX_POCKET. The claimed half (a ring wait) is our CAPTURE_STAGE ring, which deadlocked; CAPTURE_MOVE_ON stays. | combat.cpp (`farm`) |

Not taken: mike's HOR_DASH_SAFE and AOE_MAP_CLEAR (our SAFE_LAND / SOLO_LAND rule already covers both, and keeps the
farm-target exemption), EXIT_COMMIT (off there too, lost), VESPER_CARRY_SAFE and VESPER_NO_IDLE (off in v135-golem,
lost), the claimed-reward ring wait, and mike's debug-only log lines.

## Fuel
- `mid_clear` memo per slot and vision generation (13 callers, two engine reads per camp; new callers in GOLEM_LAST and
  TARGET_HOLD). Every action bumps the generation, so a hit of ours this turn is seen.
- `swap_tank_step` reads an ally's health only when it stands in a camp's reach (it read every ally up front).
- BANK_OPEN as board ops, SHARE_LOOT_TEAM before the pick, GOLEM_LAST skipped on maps without golems, VESPER_ESCAPE's
  field capped at 12 steps and skipped when the turn is short, enemy tiles from `seen::positions`.
- Checked dead: CAPTURE_REPICK's second pick_target never runs with V86_CAPTURE_NEAR off.
  v137-off (every v137 switch off = v135 + the fuel trims) in the same games as v135 (seed 0): mean 33.6k vs 33.5k,
  out-of-fuel turns 1,150 vs 1,326. v137 (all on): 33.9k vs 33.6k, out-of-fuel 1,411 vs 1,607 (fewer than v135 in
  its own games). Bot errors (seed 0): 10 "not in vision" in 140 games (v135 14), no others.

## Results (vs v135; replays/b137a*, b137b*, b137g*; manifests b137a.txt, b137b.txt)
| Bot | Record | Banked | Per seed | K/D ours vs theirs | Camp deaths |
|---|---|---|---|---|---|
| **v137 (all on)** | **241-170-9** | **+16.8%** | +19.2 / +19.4 / +11.8 | 6288/5947 vs 5682/6645 | **265** vs 357 |
| v137-off (fuel trims only) | 206-194-20 | +2.3% | +5.7 / +2.2 / -0.9 | 6048/6459 vs 6081/6437 | 378 vs 389 |
| v135-golem switches only | 196-205-19 | +4.2% | +1.5 / +4.9 / +6.2 | 6360/6301 vs 5940/6724 | 361 vs 364 |
| mike v135 switches only | 214-197-9 | +5.6% | +0.1 / +10.5 / +6.6 | 5743/6258 vs 5973/6136 | 285 vs 393 |
| v137 vs v135-golem | 231-176-13 | +17.3% | +8.8 / +20.6 / +22.9 | 7050/5447 vs 5156/7482 | 291 vs 432 |

The parts are worth more together than apart (+4.2 and +5.6 alone, +16.8 together).

### One switch off (v137 minus it, vs v135, 420 games each)
| Off | Record | Banked | Drop vs v137 |
|---|---|---|---|
| CAPTURE_NEWS | 204-207-9 | +1.4% | -15.4 |
| VESPER_ESCAPE | 206-205-9 | +5.3% | -11.5 |
| SHARE_LOOT_TEAM + COOL | 222-191-7 | +8.2% | -8.6 |
| TARGET_HOLD | 233-171-16 | +13.9% | -2.9 |
| BANK_OPEN | 236-177-7 | +13.9% | -2.9 |
| CAPTURE_STAND | 224-186-10 | +14.3% | -2.5 |
| GOLEM_LAST | 237-175-8 | +15.0% | -1.8 |
| HOP_STICKY | 237-174-9 | +16.0% | -0.8 |

Every removal is below v137, so all stay on; GOLEM_LAST and HOP_STICKY are inside the noise. INVADE_OWN_KEEP and
FINISH_TANK were not ablated. Against v135-golem, v137 has more fuelExhausted events (6,719 vs 5,582): the mike-based
bots stay cheaper per turn, and v135's own additions (present gate, area rule, tank swap) are where the next trims are.
