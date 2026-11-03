# v157-monolith (v155 + monolith deadlock fixes and session play)

Jack's bot (jack-claude, 2026-10-31), benched locally as v161. The same switch names appear in bots/v182's lineage
(v156-v161 blocks of its `src/constants.h`).

Deadlocks (fixed on 00contest and 02guardians, which ended 0-0):
- SHOP_NO_TRIBUTE / SHOP_EMPTY_BAG: shop without a tribute where none can come.
- GATE_ALL_GROUPS / GATE_GROUP_LOOSE: every group blocking an altar is a gate.
- MONOLITH_GATHER: the only monolith that scores is the goal before the team is grouped.

Sessions:
- MONO_MEMORY / MONO_RESUME / MONO_OWN_ONLY: remember monolith damage (it never heals) and come back to monoliths we
  damaged.
- MONO_SESSIONS / MONO_EARLY: a damage-before-the-tanks-run-out estimate replaces the emblem gate; MONO_SIDE keeps the
  emblem gate for contested monoliths and teams of three or fewer.
- MONO_LEAVE: whoever the monolith hits leaves on its own walk-out bar, then rests until healed.
- MONO_ENEMY_END / MONO_FIGHT_OUT: an enemy ends the session, and fights stay out of the monolith's reach.
- MONO_COVER is in the code but off (it idled fragile champions).

Bench (local, as v161):
- vs v156, 70 maps x 2 sides: 78-62, banked +10.5%.
- vs v155 + v152, 280 games: 156 wins (v156 also 156), banked +14.7% (v156 +12.0%), real monoliths 62-3 (v156 24-9),
  fewer deaths.
- Fuel mean 34.3k vs v156 33.7k.
