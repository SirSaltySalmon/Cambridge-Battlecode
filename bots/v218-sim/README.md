# v218-sim: g4 as a narrow brake on v213's fight score (fight_coord_plan.md F3 follow-up)

Base: bots/v216-sim (= v213 with FIGHT_SIM 0). Flag `FIGHT_SIM` in `src/constants.h` (block after `FIGHT_BAR`,
the v218 part after `SIM_ENEMY_READY`).

| FIGHT_SIM | Behaviour |
|---|---|
| 0 | v213 |
| 1 | v213 plays; sim + g4 logged in debug only (`sim:` note, now also the lean g4 and the veto block bits) |
| 2, 3 | v216-sim: the sim / g4 decides (both lost, see bots/v216-sim/README.md) |
| **4** | **v213 decides; g4 may only turn its FIGHT into RETREAT** (this folder) |

Rule (`veto_mode`, `src/fightsim.cpp`): `score_mode_old` says FIGHT by less than VETO_MARGIN 30 over its bar ->
`g4_eval()`; veto when p_lost - p_won > VETO_BAR 0.5 (VETO_STAY 0.6 when the current mode is already FIGHT). Never
when `veto_block()` finds: win_tribute, us channelling, an ally tributing within SIM_ALLY_RANGE 12, or a channelling (recall or tribute) enemy
within VETO_PRIZE_RANGE 6. No veto memory: recomputed each call, so it holds only while the edge does. RETREAT is
never turned into FIGHT. Debug notes: `g4v:` per contact call (model numbers, veto, block bits); a veto prefixes
`score_why` ("retreat: g4 veto, ...").

Fuel: `g4_eval` is fight_sim's g4 part alone (same units and weights, no `abilities()` / cooldown reads, no sim loop),
cached once a round per near-enemy count, run only when v213 said FIGHT under the margin. Reads per unit: health, focus,
strength, attack range (type and emblems cached; enemy tiles read once); allies come from `score_mode_old`'s
`fight_allies` (same filter as fight_sim's, static_assert on the flags). The block bits are read only once the bar is
crossed. FIGHT_SIM 0 and 1 run none of it in release; the one line added to `score_mode_old` (route.cpp, the margin)
is `if constexpr (FIGHT_SIM == 4)`.

## Choosing the bar (4 debug games, FIGHT_SIM 1, vs v210)

`sim:` notes, one per score_mode call with near enemies: 2,485 calls, 2,004 contact rounds, 1,958 v213 FIGHT calls.
Outcome: the noting champion's death within 10 rounds; enemy death within 10 rounds and 10 tiles. (Fight_sim's g4 and
the lean `g4_eval` agree on 2,482 of 2,485 calls.)

| g4 edge (p_lost - p_won), FIGHT calls | n | we die | enemy dies near |
|---|---|---|---|
| < 0 | 529 | 2.8% | 23.8% |
| 0 - 0.2 | 864 | 6.8% | 10.4% |
| 0.2 - 0.4 | 370 | 17% | 4.6% |
| 0.4 - 0.5 | 90 | 30% | 0% |
| 0.5 - 0.6 | 50 | 20% | 0% |
| > 0.6 | 55 | 42% | 1.8% |

| candidate (enter / stay) | % of contact rounds | we die | enemy dies near |
|---|---|---|---|
| edge 0.3 / 0.4 | 11.2% | 26% | 2.0% |
| edge 0.4 / 0.5 | 6.0% | 28% | 0.8% |
| **edge 0.5 / 0.6** | **3.5%** | **34%** | **1.4%** |
| edge 0.6 / 0.7 | 2.2% | 44% | 2.2% |
| g4 p_die > 0.6 | 1.7% | 46% | 2.9% |
| sim od >= td + 1 | 27% | 20% | 3.6% |
| sim od >= td + 1 and p_die > 0.5 | 2.5% | 50% | 1.9% |

Chosen: g4 edge 0.5 / 0.6. Above 0.4 the enemy almost never dies, so a veto gives up little; the sim's death edge
fires far more for less signal (and needs the ability reads). Blocks among FIGHT calls: win_tribute 43, channelling
enemy 25, ally tributing 8 (none over the bar). VETO_MARGIN: 53% of FIGHT calls have a margin of 30+, holding 2 of the
73 would-be vetoes.

## Smoke (4 debug games, FIGHT_SIM 4, same maps and sides)

Built before the margin gate and the read trims (both keep the rule; the trimmed build compiles in debug and release
for FIGHT_SIM 0/1/4 but has not been played).

| game (our side) | v213 (replays/inv216/v213) | FIGHT_SIM 1 (logged) | FIGHT_SIM 4 | vetoes / contact rounds |
|---|---|---|---|---|
| 01shortcut a | W 987-923, deaths 17-17 | W 987-923 | W 1015-137, deaths 2-17 | 4 / 382 |
| 00corners b | W 1021-340, 7-10 | L 311-1018 | W 1060-705, 8-9 | 0 / 445 |
| 01starvation a | L 124-1029, 26-4 | L 124-1029 | W 1002-200, 15-28 | 4 / 953 |
| 00the-line b | L 161-1042, 16-4 | L 161-1042 | L 161-1052, 16-2 | 13 / 545 |

- Veto rate 21 of 2,325 contact rounds (0.9%; 23 of 3,093 calls). The champion died within 10 rounds after 11 of the
  23 vetoes: most fired on turns it was already in hurt.dodge or outclass.leave, i.e. the veto mostly extends those
  per-champion exits to the team. 6 veto/fight flips, 0 veto-fight-veto flaps within 3 rounds, 0 bot faults.
- Record 3-1 vs v213's 2-2 on these games, but play diverges early (fuel timing), so 4 games say nothing on strength.
- Fuel (debug fuel = release fuel; LOG is paused): per-turn extra on a g4 turn, paired with the logged game before play
  diverged: median ~10k on 00the-line (p90 14k, max 15k), up to 17k on 01starvation. Mean fuel a turn 38.3k vs 34.4k
  logged / 35.3k v213, out-of-fuel turns 62 of 23,588 (0.26%) vs 25 (0.10%) / 31 (0.12%); the means also carry the
  different games. Hence the margin gate (half the evaluations) and the read trims (allies' filter reads, type, enemy
  tiles, unseen alive reads): expected well under half the measured cost, unmeasured.

Untested follow-ups: a bench vs v213 (3 seeds) with the trimmed build, first confirming out-of-fuel turns are back near
v213's; the veto only on the leader's shared decision (it acts on the local re-score and hor_holds too).
