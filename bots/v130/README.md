# v130

`v129` (our ladder bot, downloaded from the website) + the retreat and fight-score plan: a dash escape, a safer team
retreat, escape tools for champions without a dash, leaving allies out of the fight score, and a strict stay-in bar when
behind on both health and power. Built step by step as v130-dash, v131-retreat, v132-escape, v133-fleeing and
v134-emblem (step 6, v135-stayout, was dropped). Not submitted.

## Why
v129 vs itself (40 platform maps, both sides, debug logs): low-health retreats ended in death 26% of the time (61-69% when
an enemy was within 2 tiles), Vesper used her dash in 2% of her retreat deaths, 32% of champions died within 20 rounds
after a team retreat that started inside enemy reach, and the fight score's emblem term (+3 per emblem of ours, ~+41 in an
average fight decision) carried fights we were losing on both health and power (lost 47%, won 3%).

## Switches on (all in `constants.h`)
1. `ESCAPE_DASH` (step 1, `dash_escape` in `combat.cpp`): in the low-health dodge, with the dash ready and an enemy within
   THREAT_RANGE + 2, dash to the tile farthest from the enemies (out of reach first, then nearest our spawn) if it gains
   distance, then walk. Heart of Ruin raises his shield first and dashes only while it is not ready.
   (`ESCAPE_DASH_HOR` on, `ESCAPE_MIN_GAIN` 1.)
2. `RETREAT_SAFE` (step 2, `retreat()`): a team retreat steps out of each enemy's dash and spell reach (`recall_zone`)
   instead of the flat THREAT_RANGE, toward the nearest ally already outside it; out of reach, it farms around that zone.
3. `ESCAPE_VARG_SHIELD`, `ESCAPE_STUN` + `ESCAPE_STUN_BACKED` (step 3, `escape_tools`): Varg raises his shield while fleeing;
   Ryth / Prism stun the chaser instead of walking when walking cannot get out of its reach, only with a healthy ally
   within 4 tiles or a second enemy beside the chaser.
4. `FIGHT_SKIP_LEAVERS` (step 4, `score_mode` in `route.cpp`): allies leaving the fight (`stays_out`: under the recall bar,
   or a carrier sent back) do not count in the fight score.
5. `EMB_STAY_STRICT` (step 5): behind on both health and power, staying in a fight needs the normal FIGHT_THRESHOLD (no
   hysteresis slack).

## Switches off (kept in the code)
`RETREAT_COOL` (+ `RETREAT_COOL_NEAR`), `ESCAPE_MARCELLA_STUN`, `LEAVERS_KEEP_HOR`, `EMB_DOUBLE_DEFICIT`, `EMB_CAP_VALUE` (0),
`EMBLEM_RELATIVE` (Salmon's v115 switch). Step 6 (`HURT_STAY_OUT`) is not in this bot.

## Bench (release, 40 platform maps, both sides, normal + unlimited fuel = 160 games a row)
| step | change | result | notes |
|---|---|---|---|
| 1 | dash escape | 79-81 vs v129, -0.1%; 90-70 vs v122, +17.0% | Vesper retreats with a dash died 11% (32% without); Vesper-only / min gain 2 / both: 78-82, 81-79, 80-80 |
| 2 | safer team retreat | 81-79 vs step 1, +4.3%; 88-72 vs v129, +11.7% | kills +13% / deaths -12% (normal); cool-off lost every way (76-84, 79-81, 80-80: less farming) |
| 3 | Varg shield + backed stun | 84-76 vs step 2, +7.8% | Varg shield alone 82-78; unbacked stun 75-85 (stunned in lost spots); Marcella stun 72-88 |
| 4 | skip leavers | 78-82 vs step 3, +1.2% | applies on 5.3% of leader decisions; masked by the emblem term until step 5 |
| 5 | strict stay-in | 84-76 vs step 3, +9.3%; 81-79 vs step 2, +2.6%; 85-75 vs v129, +0.8% | without skip leavers 83-77; double deficit 81-79; cap 15 81-79 (+6.1%); cap 10 76-84; cap 20 80-80; ours - theirs 70-90 |
| 6 | stay out at low health (dropped) | 79-81 / 81-79 / 79-81 / 81-79 vs step 5 | fewer champions fighting, re-deaths after rejoining not reduced |

Step 5 debug (vs step 3, same games): stay-in decisions lost 11% / won 25% (19% / 15%); staying in while an ally fled lost
17% (36%); farming 54.7% of turns (54.1%), camp clears 50.5 a game (49.6). Fuel per turn across all steps: about the same
as v129 (step 1 -2.4%, step 2 +1.0%, step 3 +1.0%, step 4 -0.4%, step 5 -0.4%).

## Before the ladder
Step 5's gain shows in normal-fuel games (45-35, 43-37, 44-36 vs step 3, step 2, v129); with unlimited fuel it is even or
slightly behind, and it collected 4-5% fewer gems there vs step 2 and v129. On the ladder, compare gems collected per game
with v129's. If farming drops, the safe option is `EMB_STAY_STRICT` off and `EMB_CAP_VALUE` 15 (81-79, +6.1%, more gems
collected in both fuel modes).
