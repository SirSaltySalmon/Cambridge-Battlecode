# v135

v134 plus the movement and camp fixes from the v135a audit (plan and evidence: `bots/v135a/PLAN.md`), and the v135a
log-only audit lines (`@s`, `@land`; inside `LOG`, so release builds skip them). Each fix is a switch at the top of
`src/constants.h`. On here: NO_ESCORT, SAFE_LAND + SOLO_LAND, PRESENT_GATE, TANK_SWAP + SWAP_HIT, and the fuel guard
V135_SPARE_FUEL, and the capture rules CAPTURE_MOVE_ON (with CAPTURE_STAGE as its gate), CLAIM_HANDOFF, PASS_REACH,
with V86_CAPTURE_NEAR off. Off (lost or neutral in benches): the staging ring and edge wait, TEAM_ABANDON, BANK_KEEP. Single-switch variants for benches: `python bots/v133t/bench.py variant v135 NAME
NO_ESCORT=false CAPTURE_STAGE=false SAFE_LAND=false PRESENT_GATE=false TANK_SWAP=false TEAM_ABANDON=false
BANK_KEEP=false <the one switch>=true` (TANK_SWAP needs NO_ESCORT: it writes the freed carrier bits).

| Switch | What it does | Code |
|---|---|---|
| NO_ESCORT | No escorted tribute; heavy bags bank by the usual rules. The mode byte's carrier bits become TANK_ON_DUTY (`shared_tank` / `write_tank`). | state.h, tribute.cpp, route.cpp |
| CAPTURE_STAGE | Nobody but the collector targets a group under capture: an empty table near the reward is re-picked from the whole table, and the others wait off its tiles or at its edge toward the next group (`stage_capture`). | phase.cpp, combat.cpp |
| SAFE_LAND (+SOLO_LAND) | One rule for every area hit, `area_ok`: a group may be in HoR's landing area or an area spell only if it is clusters only, dead, the farm target / a group already hit (gate and monolith rules), or one we beat alone (`solo_win`, never while awake camps already hit us). Static spawn tiles, so hidden camps count; no legal landing means no dash (`build_land_bar`, `land_bar`). | combat.cpp, actions.cpp, hor.cpp, kits.cpp |
| PRESENT_GATE | The emblem gate counts the champions present (free part and emblems); before the first hit on an elk/golem group a champion waits outside its reach until the gate passes and STAGE_MIN allies are there (at most STAGE_ROUNDS) (`present_near`, `team_present`, `stage_group`). | route.cpp, combat.cpp |
| TANK_SWAP (+SWAP_HIT) | Sturdy = max health 1000+ and over the recall bar. A hurt champion with a sturdy ally at the group steps behind it and keeps hitting, only on a tile it hits a camp from (`swap_hurt`); a sturdy ally steps in front of a camp hitting a hurt or fragile ally (`swap_tank_step`); ranged do not hold back while a sturdy ally is at the camp. | swap.cpp, turn.cpp, kits.cpp |
| TEAM_ABANDON | The leader drops the farm target while its awake camps hit a hurt ally with no sturdy ally near; it is back once seen asleep (`update_abandon`, `abandon_until`). | swap.cpp, route.cpp |
| CAPTURE_MOVE_ON | The collector captures alone; everyone else treats the group as done, never steps on its tiles and walks to the next group (the old ring/edge wait deadlocked: bodies on the collector's path, 02fields r725-792). V86_CAPTURE_NEAR (team kept within 6 of a reward outside the safe zone) is off with it. | combat.cpp, phase.cpp |
| CLAIM_HANDOFF | A collector refused a step 2 turns running, with a reward tile free of champions and of the walk guard and its walk unspent, hands the claim to a visible ally nearer the reward on foot. | route.cpp |
| PASS_REACH | The free hit on a passing cluster skips one whose reward we cannot reach (02the-pond island cluster). | combat.cpp |
| DASH_REACH | "Can it get to the reward" counts a dash: walkable, or for a dasher reachable by hopping thin holes (hop_steps). Every claim (leader, funnel, self), the collector's give-back, PASS_REACH, staging. | route.cpp, combat.cpp |
| COLLECT_FIGHT | A fight comes before a reward: a collector gives the claim back when an enemy stands on the reward, or when the team fights with an enemy near before it stands on a reward tile; no new claim while the team fights. Collection used to run before the fight step and only an enemy in reach of the collector stopped it. | turn.cpp, route.cpp |
| HOP_WHEN_CUT | go() and the hop planner plan the hop route whenever walking cannot reach the goal at all (they only did with a thin hole within HOP_NEAR: 01shortcut, HoR idle on a reward his dash reaches). | actions.cpp |
| BANK_KEEP | An idle-bag bank walk keeps going (it flipped back to farm.wait as the respawn wait on arrival reached zero). | turn.cpp |
| V135_SPARE_FUEL | Under 30k fuel left in a turn, skip the sturdy step and aoe_scan's per-group checks (v134's clear-all rule). | swap.cpp, kits.cpp |

## Debug audit (65-map mirror, v135 vs the v135a mirror = v134 decisions)
| Measure | v135a | v135 |
|---|---|---|
| Non-collector turns targeting the group under capture | 23,831 | 1,899 (1,220 of them waiting) |
| Hurt/exit moves out of a farm | 2,065 | 909 |
| ...with a sturdy healthy ally in the camp's reach | 1,128 | 312 |
| HoR landings beside a live camp off his target / died within 4 turns | 246 / 17 | 117 / 7 |
| Camp deaths | 201 | 105 |
| Step-there-and-back turns | 3.95% | 3.75% |

Left: the team walking in a column blocks its own steps for a turn or two (00ambush r178-183), present in v134 too. This audit ran before SOLO_LAND, SWAP_HIT and the fuel guard.

## Release bench (vs v134, 65 maps both sides, fuel-burn seeds 0-2; replays/b135, b135b, b135c, b135d, b135g)
| Variant | Record | Banked | Notes |
|---|---|---|---|
| PRESENT_GATE alone | 205-169-16 | +6.9% | |
| TEAM_ABANDON alone | 197-175-18 | +5.0% | with the others: 180-192 / 180-197 (+0.8 / +0.9%): off |
| SAFE_LAND alone (v135 rule / strict solo / farm target exempt) | 196-178 / 193-177 / 190-185 | +4.3 / +1.9 / +2.7% | |
| NO_ESCORT alone | 189-193-8 | -0.3% | kept: the user removed escorts; frees TANK_ON_DUTY bits |
| BANK_KEEP alone | 183-195-12 | -0.5% | off |
| TANK_SWAP alone (first / SWAP_HIT) | 175-200 / 212-167 | -3.8 / +8.3% | idle waits behind the tank lost; hitting from behind wins |
| CAPTURE_STAGE (re-pick on / off) | 175-202 / 166-207 | -7.2 / -8.6% | off; why it loses is not found |
| v135d: escort, land, gate, swap | 204-175-11 | +9.0% | out of fuel 2,104 vs 870 (seed 0) |
| v135g: v135d + V135_SPARE_FUEL + fuel trims | **194-184-12** | **+5.0%** | camp deaths 373 vs 647; mean fuel +4.2%, out of fuel 1,127 vs 958 |
| CLAIM_HANDOFF + PASS_REACH on v135g (control) | 198-180-12 | +8.6% | seeds +8.1 / +8.4 / +9.2; replays/b135m |
| + CAPTURE_MOVE_ON, V86_CAPTURE_NEAR kept | 202-175-13 | +7.9% | |
| + CAPTURE_MOVE_ON, V86_CAPTURE_NEAR off | 201-178-11 | +10.6% | seeds +10.1 / +10.4 / +11.3; mean fuel +7.2%, out of fuel 1,237 vs 698 (seed 0) |

Capture check (debug, one seed, replays/b135md): non-collector turns aimed at the captured group 25,561 -> 1,640, captures
5,386 -> 5,591, capture gems +6%, collectors killed while collecting 78 -> 46, stalls of 10+ turns 10 -> 4 (all Vesper
boxed in by terrain on 01boss-battle / 02the-pond). Capture times unchanged, Capture emblem cuts included.
| + DASH_REACH + COLLECT_FIGHT | 235-141-14 | +29.7% | seeds +27.9 / +36.0 / +25.3; replays/b135r |
| **v135: + HOP_WHEN_CUT** | **235-141-14** | **+30.8%** | seeds +27.9 / +36.7 / +28.1; K/D 7,063/5,163 vs 4,734/7,720; mean fuel +9.9%, out of fuel 1,200 vs 843 (seed 0); replays/b135h |
| v135 with COLLECT_FIGHT off | 200-179-11 | +9.7% | the fight-first rule is nearly all of the last gain |

Stalls of 10+ turns (debug, one seed): 4 -> 1 with DASH_REACH + COLLECT_FIGHT (the last, 01shortcut, fixed by
HOP_WHEN_CUT). Waiting on awake camps' reach before a reward (02stalactites) is intended and kept. A debug-only
`collect path:` line tells why a collector cannot step (terrain, walk guard, units).
