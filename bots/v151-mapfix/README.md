# v151-mapfix

v151 (the teammate's v149-cap + evotune, platform v151) with the four map fixes from 2026-10-29, each a switch in
`src/constants.h` (comments there give the cause and numbers). Benches: `results/results-fixes-vs-v151.txt`.

| Switch | Fix | Alone vs v151 (140 games) |
|---|---|---|
| `SPAWN_PLAY_ON` (turn.cpp) | a healed champion whose spawn.leave finds no step plays the turn from the spawn (00breadcrumbs freeze, ladder 0-13) | 67-69-4, -1.4% |
| `CHASE_LEASH` (combat.cpp join_first) | teams of 4 or fewer: no closing on an enemy unless an ally keeps up (HoR ~10 deaths a game on anvil/hooks) | 3 seeds 202-202-16, +1.9% |
| `ALTAR_FARM_BANK` (route.cpp, turn.cpp) | one-altar maps with 75% of camp gems within 10 of it (anvil, breadcrumbs, hooks): bank at 20, quick 10-20 gem tribute in fights | 69-67-4, +1.4% |
| `SEALED_DROP`, `GATE_TEAM` (route.cpp) | 01boss-battle: skip groups behind a live monolith; the gate monolith is the team goal even with a mage; no win_tribute at a shut altar | 69-66-5, +2.3%, gold +10.7% |

Left out: `HOR_DIVE_FOLLOW` (v151-f2a, -11.0%).
