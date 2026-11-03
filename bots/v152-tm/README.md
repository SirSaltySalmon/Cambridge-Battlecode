# v152-tm

v152 + build + map fixes only: the control for the three merges (v152-jfe, v152-jf, v152-cap).

Shared part (same as `bots/v152-tm`): `v151-tc` shop builds (`src/constants.h` build tables; Vendetta keeps v152's build) and the four
`v151-mapfix` fixes (`SPAWN_PLAY_ON`, `CHASE_LEASH`, `ALTAR_FARM_BANK`, `SEALED_DROP` + `GATE_TEAM`), three-way merged onto v152.
One hand-made merge: `bank_bar()` (`src/bot.h`) returns `ALTAR_FARM_BANK` on altar-farm maps, else v152's small-bank bar.
v152's `HOR_DIVE_FOLLOW` stays on (mapfix left it out after -11% on v151; v152 benched it +12.3% on v149).
