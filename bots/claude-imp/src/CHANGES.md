# Bot changes (bot.py only — draft.py, main.py, randomness.py unchanged)

Tested with the real engine (`cambc` 2.1.6) on the 9 pool maps + 8 generated maps, every map from both sides.

| vs original bot | Wins–losses | Gem margin |
|---|---|---|
| 9 pool maps (18 games) | **16–2** | — |
| All 34 games | **28–6** | +14,708 |

Remaining losses: expanse (side A), monolith (side A), packs (side B), plus 3 generated maps.

## Bugs fixed
- **Fuel exhaustion.** On big maps 30–45% of turns ran out of Python fuel, so champions froze mid-turn
  (the "Vesper does nothing" / "team has a stroke" games). Now ~1%.
  - Per-turn cache of `vision.champions/camps` (`T.enemies()`, `T.allies()`, `T.camps()`), reset after moves/attacks.
  - Per-group distance fields built once (lazily, only while fuel is left) instead of a full-map BFS + Python
    loop over every camp tile on every `pick_target` call.
  - `observe()` only touches groups whose spawn tiles are in sight; emblems polled every 10 rounds;
    ward-spot scan memoised per turn; terrain bitboards built once.
- **Turn crashes**: `vision.champions()` sometimes raises ("No visible entity…"); wrapped in `safe_champions()`,
  and `engage()` tolerates a target leaving vision.
- **Spawn trap**: when the path to an altar crossed our own spawn, "leave spawn → target" and "walk to altar"
  alternated forever (Vesper paced beside spawn for ~380 rounds on rows). Healed champions on spawn now use
  the normal decision flow.

## Banking
- Vesper no longer refuses to tribute until 6 emblems (on collision she collected 2,728 gems and banked 0).
- Bank bar scales with distance to the nearest altar: `2 gems × steps`, between 30 and 90.
  Small bags (≥10) bank when an altar is ≤6 steps away; bags ≥25 bank when enemies are seen and an altar is ≤12 steps.
- Banks immediately if the bag would reach 1,000.
- Altar choice skips altars with a visible enemy within 5 or an enemy altar lock, without detouring >10 tiles;
  if only contested altars are near, small bags are held.
- Allies within 10 of a team-mate channelling ≥50 gems stay to guard it.

## Fights
- Fight score is damage × health of each side (Focus counts 0.25 as damage), counting only champions within
  9 tiles of the fight; hidden living enemies add 15% each; fighting at a spawn doubles that side.
- Hysteresis: start a fight at ratio ≥ 1.0, keep fighting at ≥ 0.7 (no more fight/retreat flip-flopping).
  A champion already in enemy reach uses the lower bar.
- Each champion judges from its own vision (the leader may not see the fight).
- Focus fire: whole team hits the lowest-health enemy in reach (+2), preferring channellers and Willow.
- Issac/Vendetta/Ren only dash in when an ally is already on the target (no solo dives).
- The capture collector drops the capture when enemies are within 9.
- Retreat heads toward our spawn (walking distance) instead of the nearest corner; dashes away if threatened.
  If chased far from home with no dash, turn and fight back.
- Vesper never dashes/walks into tiles an enemy melee can reach next turn when fighting champions.

## Other
- Altar lock (10g) bought after the first emblem; used to cancel an enemy tribute within 2 tiles.
- Camp value: gold weighted 1.5 until round 150, falling to 0.2 by round 450 (late game chases gems).

## Tried and dropped
- Teleport crystals: work, but on the pool maps routes almost never gain ≥5 steps — wasted gold.
- Lowering the fight bar when enemies carry gems: made results worse (22–12).

## Not done
- Ward placement rework (wards only give vision to their owner, so far-away spots help little).
- The "melee cat scout" code isn't in this zip.
