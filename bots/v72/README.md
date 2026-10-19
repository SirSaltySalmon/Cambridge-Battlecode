# v72

`v71` + four fixes (each has a switch in `constants.h`). Fix 1: banking,  `FIX_NEAR_BANK` in `constants.h`: with an altar within 6 tiles and no threat (not in a
fight or retreat), any bag of 10+ gems is banked. Built Ren too, so he no longer walks around with his 150-gem
cap; his rush and cap rules are otherwise unchanged.

Why: on the ladder, opponents bank first at ~round 133 with ~36-gem bags; v71 banked first at ~round 272 with
~66-gem bags and lost ~250 carried gems per lost game to enemy kills. Ren banked at round ~164 with ~160-gem bags.

Results (34 games each, all 17 maps from both sides):

| Opponent | v72 | v71 |
| --- | --- | --- |
| v71 | 19-15 (gems +17%, gems lost -21%, deaths +7%) | - |
| v70-stellar-ren | 18-16 | 20-14 |
| v64-bushward | 21-13 | 20-14 |
| v64 | 24-10 | 23-11 |

Ren vs v71: first tribute round 164 -> 68, median bag 160 -> 44, tributes per game 1.3 -> 2.6.

Tried and dropped: Ren banks before every rush recall (16-18; delays his items, 13-21 combined with this fix);
GEM_BANK 25 (16-18); Ren cap 100 (18-16, no gem gain); near-bank range 4/8/10 (18-16, 18-16, 19-15) and bag 20/30
(19-15, 16-18).

## Fix 2: `FIX_SCOUT_WARD` - the ward follows its owner

Ladder teams move their ward 150-390 times a game; v71 put it on a fixed post once (central / enemy-spawn spots,
or one key bush). Vision is not shared, so a far post only helps its owner. Now (`scout_ward()` in
`warding.cpp`) the ward goes into a reachable bush we cannot see into, or catches up (up to 2 tiles ahead
toward the target) once it is more than `SCOUT_REFRESH` = 8 tiles behind. A ward already in a bush beside us
stays. The old posts and the walk to them (`ward_detour`) are off unless `SCOUT_KEEP_POSTS`. ~230 moves a game.

| Opponent | v71 | v72 banking only | v72 banking + ward |
| --- | --- | --- | --- |
| v71 | - | 19-15 | 19-15 |
| v70-stellar-ren | 20-14 | 18-16 | 24-10 (gems +55%) |
| v64-bushward | 20-14 | 21-13 | 25-9 |
| v64 | 23-11 | 24-10 | 25-9 |
| Total | 63-39 | 82-54 | 93-43 |

Head to head, banking + ward vs banking only: 16-18 (gems -1%). Deaths vs v71 +15%.

Tried and dropped: moving the ward every round (~2,760 moves a game, out-of-fuel turns x2.6, 14-20 vs banking
only); refresh 3 / 5 (17-17 / 16-18 vs banking only, 88-48 over the four opponents for 5); bushes only (15-19);
keeping the old posts (14-20).

## Fix 3: `FIX_CARRIER` - protect carriers

On the ladder our carriers died with 30-99 gem bags most often (Ren, Stellar, Cassi & Issac), e.g. Ren leading
alone, 1v3 under 35% health with 80 gems. A champion carrying `CARRY_GEMS` = 20+ now:
- recalls (or steps out of reach) below 50% health instead of 35% (`CARRY_RECALL_HEALTH`);
- stays behind the front below 50% health in a fight or retreat, like v71's 150-gem rule (`CARRY_FRONT_HEALTH`);
- banks instead of farming on while an enemy champion is in sight (not in a fight).

| Opponent | v72 before fix 3 | v72 with fix 3 |
| --- | --- | --- |
| v72 before fix 3 | - | 21-13 (gems +13%, gems lost -12%, deaths -17%) |
| v71 | 19-15 | 18-16 (gems lost -20%) |
| v70-stellar-ren | 24-10 | 22-12 |
| v64-bushward | 25-9 | 25-9 |
| v64 | 25-9 | 26-8 |
| Total vs those four | 93-43 | 91-45 |

Gems lost on death drop 12-20% against every opponent. Each rule alone vs v72 before fix 3: recall 15-19,
front 18-16, hunted 18-16; front + hunted 20-14 (87-49 vs the four); all three at 30 / 40 gems 18-16 / 20-14.

## Fix 4: `FIX_GEM_FIRST` - gems before gold clusters (hard maps)

Ladder stats, camp captures per game (us / them): gold clusters (35 gold, no gems) on collision 26.7 / 3.8, swirl
11.3 / 2.9, monolith 8.9 / 3.2, corners 3.1 / 0.6, while ladder teams took more gem camps (monolith 32.5 vs 23.0).
The cause: the draft pocket on these maps holds a line of gold clusters, and `best_goal()` picked any pocket group
by rate, so whenever the pocket's wolves were respawning a gold cluster became the whole team's target (~15% of all
turns on monolith). Now:
- `best_goal()` aims only at the pocket's gem groups when it has any (gold clusters stay fillers for respawn waits);
- a gem-less group's value in the draft plan and route is scaled by `GEMLESS_VALUE` = 0.25;
- an on-the-way gold cluster is taken only within `GOLD_ONLY_NEAR` = 2 steps.
Pockets with no gem group (stampede's gold fields; its only gems are the 10-elk herd) are unchanged.

| Opponent | v72 before fix 4 | v72 with fix 4 |
| --- | --- | --- |
| v72 before fix 4 | - | 22-12 (gems +19%, gems lost -13%, deaths -19%) |
| v71 | 18-16 | 17-17 |
| v70-stellar-ren | 22-12 | 25-9 |
| v64-bushward | 25-9 | 25-9 |
| v64 | 26-8 | 24-10 |
| Total vs those four | 91-45 | 91-45 |

Vs v72 before fix 4, monolith gold-cluster captures 20 -> 3 and gem captures 54 -> 68; swirl gem captures 16 -> 26.
Lanes lost both sides (lanes has no gold-cluster problem on the ladder; its problem is gems lost on death).
Goal filter alone: 22-12 vs v72 before, 87-49 vs the four. Tried first and dropped: lower FARM_GOLD_WEIGHT (1.0 /
0.75 / 0.4: 14-20 / 14-20 / 18-16, no change in what we capture); skipping on-the-way gold clusters alone.
The monolith camp itself is almost never taken on the ladder (2 captures in 21 games), so its gate is unchanged.

## Ladder check (submitted as version 72, 2026-10-18)

Same 30 test matches as v71 (6 teams ranked 145-155, same maps, we are side A): v72 8-22, v71 13-17. Against the
25 matches whose opponent did not change its bot in between: v72 7-18, v71 9-16 (4 new wins, 6 new losses: noise
level, but no gain). The fixes work as designed (per lost game: banked by round 150 55 -> 92, gems lost to kills
284 -> 206, ~180 ward moves), yet losses are still an economy race: opponents' gold by round 300 ~1,940 vs ours
~1,490 in both versions.

Over half of our deaths with gems (53 of 89 in v71, 60 of 91 in v72) happen on `hurt.dodge` turns. `FIX_DODGE`
(dash out, Cassi stun, walk toward an ally, melee hits back) cut deaths 12% locally but lost 16-18 to v72 without
it and 86-50 vs the four opponents (v72: 91-45), so it is off.

## v71 (base)

`v70` (the ladder upload) + seven fixes for capturing and farming. Each fix has its own switch at the top of
`constants.h` (`FIX_...`), so it can be turned off or tested alone; the `v71-<name>` folders are v71 with
only that one switch on.

## Results (34 games per opponent, all 17 maps from both sides)

| Opponent | v71 normal fuel | v70 normal fuel | v71 unlimited | v70 unlimited |
| --- | --- | --- | --- | --- |
| v70 | 20-14 | 17-17 | 18-16 | 17-17 |
| v69 | 21-13 | 16-18 | 18-16 | 17-17 |
| v67 | 20-14 | 19-15 | 23-11 | 15-19 |
| v64 | 23-11 | 23-11 | 25-9 | 19-15 |
| **Total** | **84-52** | 75-61 | **84-52** | 68-68 |

Over all 272 games: gems 214,966 vs v70's 196,665 (+9%), deaths 4,304 vs 4,706 (-9%), captures 8,950 vs
7,891 (+13%). Per-game details are in `results/results-v71-vs-*.txt` (tested under the name `v74w`).

**Captures** (v71 vs v70 and v70 vs itself, unlimited fuel; a capture takes at least ~6 rounds after the clear):

| Rounds from clear to capture | v71 | v70 |
| --- | --- | --- |
| up to 10 | 873 of 1,045 (84%) | 576 of 932 (62%) |
| over 20 | 28 | 114 |
| taken by the enemy | 29 | 50 |
| never captured | 10 | 29 |

**Fuel** (unlimited fuel, all 136 games): 47.2k per turn on average (v70 46.4k); turns over 100k 3,744
(v70 3,072). The new code itself costs ~2k per turn (profiled); most of the rise is more fight turns, above
all the leader walking out of the enemy's chase ring (`fight.leave`, 23% of those turns over 100k) because
v71 wins more fights and chases enemies home. That branch's cost is v70 code: ~16k for the per-enemy chase
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
   while Ren was capturing the group next door, 2 tiles from the camp). This is not the rejected `v69-hold`
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

## Single-fix results (each vs v70, v69, v67, v64; normal + unlimited fuel, 272 games)

| Bot | W-L | Gems |
| --- | --- | --- |
| v70 (baseline) | 143-129 | 196,665 |
| v71-camp | 157-115 | 206,250 |
| v71-claim | 149-123 | 203,658 |
| v71-hidden | 144-128 | 199,016 |
| v71-absorb | 148-124 | 200,378 |
| v71-taken | 153-119 | 205,063 |
| v71-lead | 152-120 | 203,944 |
| v71-self | 158-114 | 207,157 |
| **v71 (all seven)** | **168-104** | **214,966** |

Every fix alone is at or above the baseline, and all seven together are clearly best.

## Tried and dropped
- `FIX_TAKEN` first picked the next route target with `pick_target()`: ~20k fuel per call, turns over 100k
  708 -> 911. Replaced by the nearest visible camp.
- `FIX_HIDDEN` first counted only visible camps: camps inside bushes were taken for hidden enemies.

## Still open
- Nearly half of all deaths happen on `hurt.dodge` turns (under 35% health, stepping out of threat).
- About 21% of champion turns are spent dead, 18% walking.
- The decision log is still the biggest fuel item (see the fuel note above).
