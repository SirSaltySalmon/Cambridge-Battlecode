# v152-jf

v152 + Jack's fight-only tuned bot `v150` (origin/jack-claude, Optuna fight trial 43, `src/evo_constants.h`:
FIGHT_THRESHOLD -14, FIGHT_HYSTERESIS 15, MIN_HOLD 1, OUTNUMBER_BONUS 0, MISSING_BODY_PENALTY 7, HEALTH_EDGE 7,
POWER_EDGE 2, DISTANCE_PENALTY 0.1, OVEREXTEND_PENALTY 20). No economy change (GEM_BANK 40, IDLE_BANK 10).

Shared part (same as `bots/v152-tm`): `v151-tc` shop builds (`src/constants.h` build tables; Vendetta keeps v152's build) and the four
`v151-mapfix` fixes (`SPAWN_PLAY_ON`, `CHASE_LEASH`, `ALTAR_FARM_BANK`, `SEALED_DROP` + `GATE_TEAM`), three-way merged onto v152.
One hand-made merge: `bank_bar()` (`src/bot.h`) returns `ALTAR_FARM_BANK` on altar-farm maps, else v152's small-bank bar.
v152's `HOR_DIVE_FOLLOW` stays on (mapfix left it out after -11% on v151; v152 benched it +12.3% on v149).
