# v155 (was v152-jf)

v155 = v152 + three merges, three-way onto v152:
- the `v151-tc` shop builds (`src/constants.h` build tables; Vendetta keeps v152's build), see below;
- the four `v151-mapfix` fixes (`SPAWN_PLAY_ON`, `CHASE_LEASH`, `ALTAR_FARM_BANK`, `SEALED_DROP` + `GATE_TEAM`), see
  bots/v151-mapfix. One hand-made merge: `bank_bar()` (`src/bot.h`) returns `ALTAR_FARM_BANK` on altar-farm maps, else
  v152's small-bank bar. v152's `HOR_DIVE_FOLLOW` stays on (mapfix left it out after -11% on v151; v152 benched it
  +12.3% on v149);
- Jack's fight-only tuned bot bots/v150-jack (bots/v150 on jack-claude; Optuna fight trial 43, `src/evo_constants.h`:
  FIGHT_THRESHOLD -14, FIGHT_HYSTERESIS 15, MIN_HOLD 1, OUTNUMBER_BONUS 0, MISSING_BODY_PENALTY 7, HEALTH_EDGE 7,
  POWER_EDGE 2, DISTANCE_PENALTY 0.1, OVEREXTEND_PENALTY 20). No economy change (GEM_BANK 40, IDLE_BANK 10).

## Results (vs v152, all 70 maps x 2 sides, release; `results/results-v152-merges-vs-v152.txt`)
| Bot | Seed 0 | 3 seeds | Banked | Collected | Gold | Fuel / turn (seed 0) |
|---|---|---|---|---|---|---|
| **v152-jf = v155** | 74-64-2, +8.1% | **218-194-8** | **+9.6%** | +8.8% | +12.3% | 33.9k vs 32.9k (+2.9%) |
| v152-tm (builds + mapfix only) | 73-63-4, +9.5% | 217-193-10 | +6.4% | +6.6% | +8.5% | 34.1k vs 33.0k (+3.3%) |
| v152-evof | | 210-203-7 | +4.7% | +6.2% | +10.8% | 34.3k vs 33.2k (+3.4%) |
| v152-evo | | 199-210-11 | +0.9% | +5.3% | +11.0% | 34.4k vs 33.1k (+4.0%) |
| v152-jfe | 75-63-2, +7.5% | | | | | 33.8k vs 33.0k (+2.6%) |
| v152-cap | 70-67-3, +3.6% | | | | | 34.2k vs 33.4k (+2.4%) |

v152-jf was ahead on all three 3-seed totals and became v155 (2026-10-31). Evotune's numbers gained nothing over
v152's own on this base, and with the emblem-ratio term lost 5.5 points.

## Test folders removed in final-stretches

These folders were local test variants that never shipped. Their READMEs are kept below; where they say `bots/<folder>/README.md` for a removed folder, the text is in this section. The code is in git history (vargvale-strikes-back, commit a31dc30 and before).

### bots/v151-tc (shop builds on v151)

v151-tc = v151 (the ladder bot: v149-cap code + evotune numbers) with only these build tables changed (src/constants.h);
Varg, Cassi/Issac, Ren, Stellar, Vendetta unchanged. Notation: family:stat, o = the offensive stat (Strength for
Vesper/Marcella/Ryth, Focus for Prism/HoR/Willow), h = Health. Theorycraft from v151 per-game data: Monster early for
everyone, Hunter/Skirmish only on Vesper, Treasure only on the gold earners, Collectors at 7 families, Jungle only on
the camp tanks, Marcella on Strength, no Greed.

- VESPER: treasure:o monster:o hunter:o skirmish:o home:o durability:h collectors:o bruiser:h jungle:o tribute:h capture:o | then repeat: power:h power:o power:o
- MARCELLA: treasure:o monster:o durability:h tribute:o jungle:h skirmish:o collectors:o prospector:h | then repeat: power:o power:h
- PRISM: treasure:o monster:o durability:h prospector:o home:h tribute:o collectors:o jungle:h | then repeat: power:o power:h
- HOR: capture:focus jungle:focus monster:h durability:h | then repeat: bruiser:focus
- RYTH: jungle:h monster:h durability:h | then repeat: bruiser:h
- WILLOW: monster:focus durability:focus | then repeat: power:health power:focus

Result vs v151 (70 maps x 2 sides, release): 3 seeds 208-195-17, banked +4.6%, collected +3.1%, gold +8.7%; every seed
positive (+7.7 / +3.1 / +3.0%). Fuel on seed 0 34.2k vs 33.9k a turn (+1.2%). Details: results/results-builds-vs-v151.txt.

### bots/v152-tm (control: builds + mapfix only)

v152 + build + map fixes only: the control for the three merges (v152-jfe, v152-jf, v152-cap).

### bots/v152-jfe (v152-jf + Jack's economy trial)

v152 + Jack's tuned bot `v151-jack` (origin/jack-claude): Optuna fight trial 43 (`src/evo_constants.h`:
FIGHT_THRESHOLD -14, FIGHT_HYSTERESIS 15, MIN_HOLD 1, OUTNUMBER_BONUS 0, MISSING_BODY_PENALTY 7, HEALTH_EDGE 7,
POWER_EDGE 2, DISTANCE_PENALTY 0.1, OVEREXTEND_PENALTY 20) + economy trial 77 (`src/constants.h`: GEM_BANK 50, IDLE_BANK 15).
Jack's older Vendetta build is not taken.

### bots/v152-cap (Salmon's emblem-ratio term)

v152 + Salmon's `v149-cap` emblem term (route.cpp score_mode: `min(3 x ours, 20) + 10 x ln((ours + 1) / (theirs + 1))`;
EMBLEM_RATIO, EMBLEM_RATIO_ADD on; WEIGHT 10, SMOOTH 1, CAP 20 in `src/evo_constants.h`), merged with v148-normal as the base,
so v152's FIGHT_THRESHOLD -5 stays (v149-cap's 5 was only its v148-normal base).

The shared part is v155's builds + mapfix merge (top of this README).

### bots/v152-evo

v152-cap (v152 + tc builds + mapfix + v149-cap emblem ratio code) with Salmon's v151 evotune numbers (`src/evo_constants.h` = bots/v151's):
the fight and emblem-ratio numbers as tuned together. The shared part is v155's builds + mapfix merge.

### bots/v152-evof

v152-tm (v152 + tc builds + mapfix) with only v151 evotune's fight numbers: FIGHT_THRESHOLD 2, FIGHT_HYSTERESIS 13,
MIN_HOLD 1, MISSING_BODY_PENALTY 3, DISTANCE_PENALTY 0.454557, OVEREXTEND_PENALTY 9. No emblem ratio code, EMBLEM_BONUS stays 3.

### Seed copies

`v152-tm-s1/-s2` and `v152-jf-s1/-s2` (never committed) were fuel-burn seed copies: the same code with `{ volatile int sink = 0; for (int i = 0; i < 300; ++i) sink += i; }` before `cambc::end_turn()` in `src/main.cpp`, so fuel timing (and every timing-dependent outcome) reshuffles. Benches pool seed 0 (this folder) with the two copies; recreate them by adding that line.
