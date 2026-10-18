# v74

`v70` (the ladder upload) + seven fixes for capturing and farming. Each fix has its own switch at the top of
`constants.h` (`FIX_...`), so it can be turned off or tested alone; the `v74-<name>` folders are v74 with
only that one switch on.

## Results (34 games per opponent, all 17 maps from both sides)

| Opponent | v74 normal fuel | v70 normal fuel | v74 unlimited | v70 unlimited |
| --- | --- | --- | --- | --- |
| v70 | 20-14 | 17-17 | 18-16 | 17-17 |
| v71 | 21-13 | 16-18 | 18-16 | 17-17 |
| v67 | 20-14 | 19-15 | 23-11 | 15-19 |
| v64 | 23-11 | 23-11 | 25-9 | 19-15 |
| **Total** | **84-52** | 75-61 | **84-52** | 68-68 |

Over all 272 games: gems 214,966 vs v70's 196,665 (+9%), deaths 4,304 vs 4,706 (-9%), captures 8,950 vs
7,891 (+13%). Per-game details are in `results/results-v74-vs-*.txt` (tested under the name `v74w`).

**Captures** (v74 vs v70 and v70 vs itself, unlimited fuel; a capture takes at least ~6 rounds after the clear):

| Rounds from clear to capture | v74 | v70 |
| --- | --- | --- |
| up to 10 | 873 of 1,045 (84%) | 576 of 932 (62%) |
| over 20 | 28 | 114 |
| taken by the enemy | 29 | 50 |
| never captured | 10 | 29 |

**Fuel** (unlimited fuel, all 136 games): 47.2k per turn on average (v70 46.4k); turns over 100k 3,744
(v70 3,072). The new code itself costs ~2k per turn (profiled); most of the rise is more fight turns, above
all the leader walking out of the enemy's chase ring (`fight.leave`, 23% of those turns over 100k) because
v74 wins more fights and chases enemies home. That branch's cost is v70 code: ~16k for the per-enemy chase
text in the decision log and ~8k for the walk out.

## What was wrong, and the fixes

Found by reading the decision logs of a debug copy that also printed each champion's id, health and threat,
plus the `campClear` / `gemCollection` events of the match statistics.

1. **`FIX_CAMP_COLLECT` - camp hits no longer stop a capture.** v70's collector skipped its reward whenever a
   damaged camp could reach it. On clusters of groups 2 tiles apart (lanes centre) the reward tile is in reach
   of the next group's camps, so the collector walked off, came back, walked off (Willow did this ~20 times in
   one lanes game). A camp hit does not reset a capture (only an empty or contested region does), so now:
   only an enemy champion in reach sends the collector away; the collector stands on a reward tile out of
   awake camp reach when the group has one; with none it holds the reward under fire, or gives the claim back
   if under 50% health. A collector whose reward already has an ally on it gives the claim back (allies
   split the reward, so a second body adds nothing).
2. **`FIX_CLAIM` - the claim goes to someone who will actually collect.** v70 gave the claim to the poorest
   visible ally, with distance only as a tie-break, so Willow was sent across the map from spawn and the claim
   timed out first. Every 16 rounds the leader re-claimed the same reward for the same busy champion (lanes:
   slot 10 waited ~290 rounds). Now the nearest ally wins (150 gold of items counts as one tile), never one
   about to recall or the escorted carrier; Ren takes it only within 8 tiles; a dead collector's claim is
   dropped at once; a collector driven off by an enemy gives the claim back.
3. **`FIX_HIDDEN` - an enemy hitting from a bush.** Health lost with no enemy in sight, more than the camps in
   reach can deal (including camps inside bushes, which we cannot see but which hit us), means an enemy in an
   unseen bush within 4 tiles (spell reach). The champion puts its ward into the nearest such bush to reveal
   the attacker (then the normal fight logic answers); with no ward ready it steps out of reach of every
   unseen bush nearby, a collector gives up its claim, and a recall or tribute that would not survive the
   hits is cancelled. Against our own bots it fires 1-11 times a game (about a third reveal an enemy), so it
   is roughly neutral there; it is aimed at bush-camping ladder bots.
4. **`FIX_ABSORB` - ranged stop waiting when the camp already has a tank.** Camps hit the nearest champion.
   If a sturdy ally (melee, or over 900 max health) already stands inside the camp's reach and closer to it
   than we would stand to shoot, a ranged champion opens the camp instead of holding at 4 tiles (v70 held
   while Ren was capturing the group next door, 2 tiles from the camp). This is not the rejected `v71-hold`
   rule: the tank must already be in the camp's reach.
5. **`FIX_TAKEN` - nobody waits beside a reward someone is already capturing.** v70 champions stood idle next
   to a target whose only tile an ally was capturing (~1,600 idle turns per 34 games, "no free goal; 1 goals
   otherwise blocked"). They now hit the nearest visible camp they may take. Only visible camps are checked:
   a fresh route pick here cost ~20k fuel.
6. **`FIX_LEAD` - a champion off on its own does not lead.** Ren acts first every round, so he almost always
   led, even while chasing enemies alone across the map. The team then got fight mode (no loot-spotting, no
   bush wards), a target picked from Ren's position, and captures only from what Ren could see. Now one over
   8 tiles from the target with no ally within 8 leaves the decision to the next champion while the last one
   is fresh.
7. **`FIX_SELF_CLAIM` - claim what you see.** Vision is not shared and only the leader claimed rewards. With no
   claim open, a champion that sees a reward within 6 tiles, no enemy near and no visible ally nearer to it,
   claims it itself.

## Single-fix results (each vs v70, v71, v67, v64; normal + unlimited fuel, 272 games)

| Bot | W-L | Gems |
| --- | --- | --- |
| v70 (baseline) | 143-129 | 196,665 |
| v74-camp | 157-115 | 206,250 |
| v74-claim | 149-123 | 203,658 |
| v74-hidden | 144-128 | 199,016 |
| v74-absorb | 148-124 | 200,378 |
| v74-taken | 153-119 | 205,063 |
| v74-lead | 152-120 | 203,944 |
| v74-self | 158-114 | 207,157 |
| **v74 (all seven)** | **168-104** | **214,966** |

Every fix alone is at or above the baseline, and all seven together are clearly best.

## Tried and dropped
- `FIX_TAKEN` first picked the next route target with `pick_target()`: ~20k fuel per call, turns over 100k
  708 -> 911. Replaced by the nearest visible camp.
- `FIX_HIDDEN` first counted only visible camps: camps inside bushes were taken for hidden enemies.

## Still open
- Nearly half of all deaths happen on `hurt.dodge` turns (under 35% health, stepping out of threat).
- About 21% of champion turns are spent dead, 18% walking.
- The decision log is still the biggest fuel item (see the fuel note above).
