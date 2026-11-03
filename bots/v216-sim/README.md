# v216-sim: fight simulation (fight_coord_plan.md F3, Chat D)

Base: bots/v213. Flag `FIGHT_SIM` in `src/constants.h` (block after `FIGHT_BAR`).

| FIGHT_SIM | Behaviour | Folder |
|---|---|---|
| 0 | v213 | - |
| 1 | v213 plays; sim + Chat B's g4 model run inside LOG only (debug logs a `sim:` note per score_mode call) | bots/v216-sim |
| 2 | sim decides: SIM_SCALE x (fight EV - flee EV) - distance + ping vs SIM_BAR (0; -17 in -on17) | bots/v216-sim-on, -on17 |
| 3 | g4 decides: 100 x (p_won - p_lost) - distance + ping vs G4_BAR (-16; -38 in -g4m) | bots/v216-sim-g4, -g4m |

What changed: new `src/fightsim.cpp` (sim, g4 features, `sim_mode`, the `score_mode` wrapper); v213's body renamed
`score_mode_old` (route.cpp, also records the score and bar inside LOG); `stays_out` moved out of route.cpp's anonymous
namespace; FightEval / SimSeen in bot.h. No shared-memory bytes.

Sim: units = us + free allies within 12 of us or the front enemy, near enemies, living unseen enemies seen in the last
8 rounds (0.5 weight). Arrival = gap beyond reach (ready dash -2). 8 rounds: basic hits, each slot cast once its live
cooldown is up (enemy cooldowns read live), stuns silence the top-damage foe, Willow heal, HoR/Varg/item multipliers.
Death value = 2 x bag + 5 + min(60, left) x farm rate (Chat B: 0.12/0.14/0.16). Flee: in-reach enemies 1 hit,
dash/stun enemies 2 more, plus 4 farm rounds per body. Camps not modelled.

## Results (2026-11-03)

Enemy ability readiness: readable (776 enemy cooldown reads, 0 throws, 54% non-zero).

Offline (88 debug games vs v210 + v208, 22 maps; `python3 bots/v216-sim/simeval.py bots/v216-sim/bench/off1`),
AUC no death of ours / won exchange: v213 margin 0.676 / 0.832, sim 0.707 / 0.840, sim death edge 0.729 / 0.880,
g4 0.741 / 0.904. Sim at bar 0 fights 59% of contact rounds vs v213's 86%.

Release vs v213, 172 games, seed 0 (unlimited fuel in brackets):

| bot | record | banked | out-of-fuel turns |
|---|---|---|---|
| v216-sim (logged only) vs v210 | 87-85 (86-86, identical to v213) | 119272 vs 119410 | 0.31% (v213 0.32%) |
| -on | 47-125 | -37.3% | 1.58% |
| -on17 | 60-112 (72-100) | -25.2% (-14.9%) | 1.44% |
| -g4 | 65-107 | -21.3% | 1.63% |
| -g4m | 69-103 (73-99) | -22.6% (-6.7%) | 1.42% |

Verdict: replacing the fight score loses, from both fuel (sim reads repeated after every action, on fight turns) and
decisions (still -7% to -15% with free fuel at v213's fight share). A better offline predictor did not make better
decisions. Not for the merge. Untested follow-up: g4 as an extra brake only, cached once a round.
