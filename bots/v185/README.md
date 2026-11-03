# v185 (v182 + v170's strengths + focus fire, join fix, draft map block)

Folder named for the next free platform version (v183 and v184 are a teammate's PRISM_SAT versions). Rename it if the
platform number turns out different. Every change is a switch in `src/constants.h` with its bench numbers next to it.

## Why v170 beat v182

Ladder (ranked games in each version's live window, rating fitted against the opponents' current ratings):
v170 ~1838, v180 ~1857, v182 ~1789. Locally, 3 seeds (70 maps x 2 sides x 3 = 420 games), v182 vs v170: 194-226,
banked -8.4%.

1. **KILL_TIME** (v181, kept by v182). Its +-20 clamp outweighed every other fight-score term (health and power edges
   are worth about 7 and 2): 43% of v182's scores sat at the cap, 2,322 retreat calls at -20 in 140 debug games. It
   also lost when it was added (129-151 locally, v181 30-70 on the ladder). Off: v182 vs v170 222-198, banked +2.8%.
2. **PRISM_STRICT** (v180). Prism tributed only a bag that wins outright; v170 also starts on the enemy's pace and the
   team's income. adgato-gemcombat4: v182 banked 0 gems in all 6 games vs v170.
3. **v156-v157 economy** (shop without a tribute or with an empty bag, early bank, bank on the way, gold-only groups up
   to 8 steps away). Back to v170's values on v182 without KILL_TIME: 227-193, banked +8.4%, gold +11.0%.

Kept from v156-v179 because they help: the satellite split (off: -6 points) and ECON_PHASES / CLOSE_OUT (off: -10).
Invasions (v170 had them on): banked the same, gold +6 points, turns at 100k fuel +24%: left off.

## Added on top

- **FOCUS**: the team's focus target, the enemy the first of us picks, kept in the stamp byte's high nibble (the round
  stamp is now round % 15 + 1). Allies hit it when it is in their near list (a melee also one step out), unless a much
  weaker enemy is in reach. v182 split its fighters across enemies in 36 of 110 multi-fighter rounds of a debug game.
- **JOIN_GAP**: `join_first`'s wait restarted after any one-round gap, so a champion could hold for 35 rounds
  ("no ally in sight: hold"); a contact now ends after 6 rounds without a check.
- **DRAFT_MAP**: a draft-memory block after the zone block with the chase ring (as the tiles REL_RING drops), each
  group's camp counts by kind and steps to the nearest altar, and map facts (fight index, dash value, dash-only gems).
  Decisions are identical to v182 at unlimited fuel; champion setup fuel -8.4% on average, lower on 68 of 70 maps
  (01big-world -76k), up to 619 of 1024 bytes used. The facts are logged by the draft (debug) and kept in
  `Bot::map_info` for later work.
- **PRISM_GUARD** stays on (v180's fix: v170's guard() froze the allies, 1,386 idle turns in two gemcombat4 games).

## Tried and dropped (3 seeds vs v182 unless noted)

| change | result |
| --- | --- |
| enemy-steps dodge for every champion (ESCAPE_ALL) | 207-213, -0.7%; only with a dash ready: 205-215, -3.3% |
| walk to a remembered ally when outnumbered (REGROUP) | 191-229, -3.9%; retreat part only: 197-223, -4.2% |
| Vendetta for a non-dash melee on dash maps (DASH_DRAFT) | on the 11 swapped maps 43-35 vs 46-32 without |
| shop / bank trips inside respawn waits (WAIT_TRIP) | +6.4% vs the draft base's +7.4%; trips rarely fit |
| no satellite split on maps with fight index >= 60 | +3.8% vs the draft base's +7.4% |
| Willow shoots from her backline hold | never fired (identical games) |

## Results

v185 vs v182 (3 seeds): 222-198, banked +15.8%, gold +5.0%, fuel the same (mean 36.5k). v185 vs v170: 242-178, banked +20.0%,
gold +13.3%. Full table: `results/results-v185.txt`.
