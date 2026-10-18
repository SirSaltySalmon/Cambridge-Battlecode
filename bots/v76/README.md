# v76

`v75` (its `FIX_FIGHT_BODIES` stays off) + fixes for the stuck states of the rushbait_v2 ladder loss to LigaSiur.
Not submitted.

## What happened (ladder, v74, rushbait_v2 vs LigaSiur, 88 - 1,026 gems)
The spawns are ~5 tiles apart. LigaSiur kept champions on its own spawn, hitting ours: 124,270 champion damage and
0 kills either way in 1,083 rounds (both spawns heal), while its other champions farmed the far half. Our team
saw enemies "near" all game, stayed in fight mode at home and captured 6 camps (none after round 139):
- C&I, Ren, Stellar: ~1,300 turns of `fight.hit` (close in from outside the chase ring) / `fight.leave` (inside,
  nothing catchable: step out) at the ring's edge;
- Willow: ~430 rounds 4 tiles from a camp waiting for a melee to tank it ("melee is not closer"); none came;
- one champion: ~320 rounds of `spawn.heal` waiting for full health while enemies hit it between turns.
Against Dreamar on the same map (a normal game, 1,026 - 594 win) none of this happened. Our log stopped at round
564 of 1,083: the ~1 MB log cap.

## The fixes (switches in constants.h)
- `FIX_HOME_CAMPERS` (the root fix): an enemy within `HOME_CAMP_STEPS` = 1 step of its own spawn is dropped from
  `near_enemies()`, so it neither keeps the team in fight mode nor becomes a target (the spawn heals it every
  turn). Dodging enemies in reach still uses the full vision list.
- `FIX_STUCK_RING`: right after leaving the chase ring, enemies home in it that cannot be caught are not targets.
- `FIX_STUCK_HOLD`: a ranged champion holds back from one camp for at most `HOLD_LIMIT` = 30 rounds.
- `FIX_STUCK_HEAL`: after `SPAWN_WAIT_LIMIT` = 15 heal waits in a row on spawn, play on from the spawn.
First versions of the last three that acted in every situation lost 29-39 to v74 (out-of-fuel turns +53%).

## Results
Spawn-camper sparring bot (v74 whose ranged champions sit on their spawn and shoot; Ren and Issac farm), rushbait_v2:

| Bot | Side A | Side B | fight.leave turns (A / B) |
| --- | --- | --- | --- |
| v74 | lost 242 - 1,030 | lost 922 - 1,006 | 948 / 175 |
| v76 without FIX_HOME_CAMPERS | lost 568 - 1,006 | won 1,020 - 932 | 191 / 251 |
| v76 | won 1,080 - 154 | won 1,004 - 804 | 6 / 12 |

Normal games, 68 each (34 normal + 34 unlimited fuel): vs v74 35-33 (gems +0.8%); vs v76 without the home fix
34-34 (gems -0.7%). With unlimited fuel 25 of 34 games are identical with and without the home fix, and the only
changed result is rushbait_v2. Out-of-fuel turns +14% vs v74, 0 crashes.
