# v123

`v122` + `ESCAPE_DASH`: the low-health dodge dashes away from the enemies when the dash is ready. Three other retreat
switches are in the code and off (`HURT_DANGER`, `PEEL`, `WILLOW_RETREAT_HEAL`). Not submitted.

## Why (v122 vs itself, 40 platform maps x both sides, debug logs; `retreat_deep.py`)
- The low-health retreat (`hurt.*`, under the 35% bar) ran ~23 times per team per game and 26% ended in death; it was
  the last step before 42% of all deaths. Death rate by the nearest enemy at the start: <=1 tile 69%, 2 tiles 62%,
  3-4 27%, 5+ 8%. 61% of these deaths came within 1-2 turns.
- It only walked (`move_towards_closest(~zone)`): every champion walks one tile a turn, so the gap to the chaser never
  grew in 84% of the deaths; Vesper Vale (range 3, dash, reset) made 51% of the kills. A dash was in 2% of Vesper's and
  11% of Heart of Ruin's retreat deaths: the old `escape()` (`FIX_DODGE`, off) dashes only onto a tile out of reach.
- Allies don't help a fleeing teammate: in a team retreat every champion runs `retreat()`, pings are answered only in
  farm mode. Fleeing next to retreating allies died 48% (no ally near: 39%); 63% of allies near made no attack.

## Changes
1. `ESCAPE_DASH` (on, `dash_escape` in `combat.cpp`): in `hurt.dodge`, with the dash ready and an enemy within
   THREAT_RANGE + 2, dash to the tile in dash range farthest from the enemies (out of reach first, then nearest our
   spawn) if it gains distance, even if still in reach; then the usual walk. Heart of Ruin raises his shield first
   (one cast a turn) and dashes only while it is not ready. Willow has no dash.
2. `HURT_DANGER` (off): hurt also under 50% with an enemy within 2 tiles, or under 60% with health below 1.5x the damage
   taken in the last 3 rounds (`recent_damage`, from a 4-round health ring in `remember_health`).
3. `PEEL` (off): a healthy ally (>= 60%, light bag, not Willow) within 6 of a teammate under 50% with a chaser within 2
   hits that chaser from where it stands or after walking (never dashing, at most 2 steps) to a tile no other near
   enemy reaches; only if not outnumbered around the chaser and its health covers 2x their strength + focus.
4. `WILLOW_RETREAT_HEAL` (off): in a retreat, and in a fight while an ally is under 50%, Willow stands out of reach in
   heal range of the lowest ally.

## Bench (release, 40 platform maps, both sides, normal + unlimited fuel = 160 games a row)
| variant | vs v122 | vs v120 | kills / deaths vs v122 (normal) | out-of-fuel turns |
|---|---|---|---|---|
| v122 | - | 84-76, +5.0% | | |
| **ESCAPE_DASH (= v123)** | **86-74, +8.3%** | **92-68, +14.1%** | +4.6% / -3.1% | +1% |
| HURT_DANGER | 83-77, +7.1% | | +27.1% / -19.2% | +21% |
| HURT_DANGER + ESCAPE_DASH | 80-80, +6.2% | 86-74, +7.3% | +21.3% / -16.4% | +26% |
| + WILLOW_RETREAT_HEAL | 84-76, +7.5% | 84-76, +5.8% | +17.9% / -14.6% | +10% |
| WILLOW_RETREAT_HEAL | 80-80, +2.4% | | -4.8% / +3.7% | +22% |
| PEEL + WILLOW_RETREAT_HEAL | 81-79, -3.0% | | -9.3% / +8.7% | +49% |
| all four | 85-75, +9.0% | | +7.1% / -7.7% | +12% |

Debug reruns (low-health retreats that died; v122 26%): ESCAPE_DASH 24% (enemy <=1 tile 66%, 2 tiles 55%; gems carried
into these deaths 34 -> 27 a game); HURT_DANGER 22% (56%, 45%; team retreats followed by a death within 20 rounds
20% -> 13%); both 22%.
