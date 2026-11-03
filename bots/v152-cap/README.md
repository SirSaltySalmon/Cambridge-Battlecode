# v152-cap

v152 + Salmon's `v149-cap` emblem term (route.cpp score_mode: `min(3 x ours, 20) + 10 x ln((ours + 1) / (theirs + 1))`;
EMBLEM_RATIO, EMBLEM_RATIO_ADD on; WEIGHT 10, SMOOTH 1, CAP 20 in `src/evo_constants.h`), merged with v148-normal as the base,
so v152's FIGHT_THRESHOLD -5 stays (v149-cap's 5 was only its v148-normal base).

Shared part (same as `bots/v152-tm`): `v151-tc` shop builds (`src/constants.h` build tables; Vendetta keeps v152's build) and the four
`v151-mapfix` fixes (`SPAWN_PLAY_ON`, `CHASE_LEASH`, `ALTAR_FARM_BANK`, `SEALED_DROP` + `GATE_TEAM`), three-way merged onto v152.
One hand-made merge: `bank_bar()` (`src/bot.h`) returns `ALTAR_FARM_BANK` on altar-farm maps, else v152's small-bank bar.
v152's `HOR_DIVE_FOLLOW` stays on (mapfix left it out after -11% on v151; v152 benched it +12.3% on v149).
