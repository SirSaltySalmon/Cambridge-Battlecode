# v135 plan: farming movement, camp exits, accidental wakes, present-champion gate

`bots/v135a` is v134 plus log-only audit lines (all inside `LOG`, decisions identical to v134):
- `@s hp H/M mode slot cap s/cN awake k(x,y)>who[=ties] tanks cA cB` after every living turn: every awake camp whose
  reach holds us and who it hits (nearest: `me`, an ally `cN`, an enemy `E`; `=` lists allies tied with the nearest),
  plus the sturdy allies (max health 1000+, over the recall bar) inside reach.
- `@land dash|hop|dive (x,y) beside gG(x,y)state` after every dash: camp spawn tiles within 1 of the landing, from the
  static map (state `awake`, `asleep`, `hidden`, `dead` = tile in sight with no camp, `unseen` = tile out of sight).

Evidence runs (debug, logs): `replays/baudit` (v134 mirror, 65 maps), `replays/baudit2` (v135a mirror + both sides vs
v133t, 195 games), `replays/baudit3` (v135a mirror, 65 maps, final audit format). Analysis scripts are listed at the end.

## What the logs show

| Measure (v135a mirror, 65 maps unless noted) | Count |
|---|---|
| Turns that are a step there and straight back (v134 mirror) | 26,060 of 669,143 (3.9%) |
| Non-collector turns whose target is the group being captured | 23,831 (3.5% of living turns) |
| ...of which step-back-step | 5,046 |
| Hurt/exit moves out of a farm | 2,065 |
| ...with another healthy 1000+ max-health ally inside the same camp's reach | 1,128 (55%) |
| Exit runs (any `*.exit`) | 5,381 |
| ...hit again by a camp within 10 rounds | 2,640 (49%), 1,922 of them on a `farm.camp` turn |
| ...dead within 10 rounds | 265 |
| HoR landings beside a live camp off his target | 243: 17 died within 4 turns, 174 went into camp exits |
| Engagements opened by one champion before the rest | median ally joins +3 rounds, 23% join +8 or later; HoR opens most |
| Distance tie with an ally: was I hit next round? | 30% (strictly nearest: 78%; ally strictly nearer: 3%) |

### 1. Shuffling while farming (the biggest movement waste)
- **Capture loop (largest).** After a group is cleared the shared target often stays on it while one collector
  captures. The reward state reads as `ready_at = 0`, so everyone else logs "s# is up but out of sight: walk to it",
  walks onto the spawn tiles, then `farm.off` steps off (standing there holds the respawn), and repeats for the whole
  capture. Cause in code: `choose_phase` sets `capture_near` (team stays within 6 of a reward outside the safe zone);
  when no other group is that close, `travel()` is empty and `pick_target` returns `s` = the capture group itself.
  `FIX_TAKEN` only helps when every spawn tile already has an ally on it. Example: 00ambush r416-422, r520-525.
- **Hurt-melee tie flip.** A hurt melee alternates `hurt.dodge` / `farm.camp` every round (00ambush Marcella
  r589-602, Ryth r307-313). `finish_ok` says "an ally tanks" from one tile back; stepping in to swing makes it level
  with the tank (`gap < my_gap` fails on a tie), so next turn it dodges out again. `camp_tanked` has the same strict
  test; for an adjacent melee `my_gap = min(d, 1) = 1`, so no ally can ever count as tanking for it.
- **Ranged hold dance.** When the tank steps out, ranged champions run `farm.camp.hold` ("waits off the camp until a
  melee is closer and tanks"), then walk back when it returns; Willow also loses sight of the camp from the hold ring
  and logs `farm.walk` back in (00ambush r307-316). `farm.camp.hold` is 2,682 of the leave lines in team-wide leaves.
- **Staggered arrival.** Dashers (HoR, Vesper) reach and open camps first; walkers arrive 3-8+ rounds later
  (00ambush r42-52: HoR opens s5 alone at r42, Ryth and Marcella hit at r52).
- Smaller: bank.near.walk <-> farm.walk flips (~1k), Prism "closing" while stepping away (mage stand-off), guard.close
  <-> farm.walk around a channelling carrier.

### 2. Leaving camps, and the team shuffling around a tank swap
- 55% of hurt exits out of a farm had another healthy 1000+ health ally in the camp's reach. The hurt rules
  (`under_bar` 35%, `exit_due` from `camp_exit_cost`) never ask whether someone else can take the aggro.
- The tank's exit pulls ranged allies out through the hold rule, then everyone walks back in once a melee is near
  again. 49% of exits are hit again within 10 rounds; the re-entry is almost always the same farm target, because the
  shared slot never changes when the group is given up.
- Ties are not a safe place to stand: the camp hit us 30% of the time on a tie (lower champion id 44%, higher 17%),
  so the engine's tie-break is not a clean rule. Plans below never rely on ties.

### 3. Accidental wakes
- **HoR movement dashes.** Only `hop_route` screens HoR landings, and only against camps in sight. `dash_along`
  (every route walk, the melee closing walk) and `dash_ranked` (escape, camp exit, kite) do not screen at all.
  00great-wall: the centre chamber holds two monolith groups behind walls; HoR hops or dashes to (18,18) / (18,19) /
  (21,18) beside a monolith he cannot see, wakes it, and dies 2-4 turns later (4 of his 5 deaths in the v134 mirror,
  team B the same at r187). Also `guard.camp` sends him to "a free camp near the altar (22,19)" inside that chamber.
- **Area spells** (`aoe_scan` clear list) also use visible camps only. Measured wakes from area spells are rare, but
  the hole is the same.
- **Dunes golem (ladder 7f8dc9f5).** The golem group (one golem, eight gem clusters, centre) is the shared target from
  round 1: need 3 emblems, free part = roster size 5. On the ladder Ryth took the golem's 120 every round from r60 to
  his death at r71. Locally the team opened it together at r60. Per the user: the free part must count only the
  champions present to farm, and the emblems only theirs.

## Fixes (each behind a switch in `constants.h`, benched on its own, then together)

### F1 CAPTURE_STAGE: nobody but the collector targets a group under capture
1. `pick_target`: when the table is empty and `s == skip`, re-run the pick without `capture_near` and return that
   group as a *staged* target (stored with the capture slot, so followers read it from the shared slot as now).
2. `farm()`: if `s` is the capture slot and we are not the collector, never walk onto its spawn tiles. Move toward the
   staged next target but stop at the edge of `CAPTURE_NEAR` of the reward (the collector stays in sight and catches
   up on the same line). Already on such a tile: do not move. No `farm.off` / `farm.walk` loop is possible.
3. `observe()`: a group in reward state is not "up": `ready_at = now + capture_left + respawn` (or a separate
   `capturing` flag), so wait logic and `walk_target` never send anyone onto its tiles.
- Expected: the 23.8k capture-target turns become stage turns that move toward the next group; 5k step-back-steps go.
- Fuel: one extra pick only when the table is empty (rare); no per-turn cost.

### F2 TANK_SWAP: hurt champions stay and step behind a sturdy ally instead of leaving
Sturdy = max health >= `TANK_HEALTH` (1000), health >= `RECALL_HEALTH`, alive, in sight, not channelling. A health
gate, not a champion type (Prism grows into it).
1. **Hurt champion** (under the bar or `exit_due`) farming with no enemy near, with a sturdy ally within the awake
   group's reach + 1: do not dodge, exit or recall. Pick a tile where some sturdy ally is *strictly* nearer than us to
   every awake camp of the group, inside our attack range of a camp when one exists (melee: adjacent, ranged: 3).
   Keep attacking. If none is in range, wait on the nearest such tile out of reach and rejoin when one appears.
   Leave only below `FINISH_HEALTH` (20%), when an enemy is near, or when no sturdy ally is present.
2. **Sturdy ally**: when an awake camp of our group is nearest to (or tied with) a hurt ally, step to a tile strictly
   nearer to that camp than the hurt ally ("take the aggro"), adjacent to the camp when possible. Melee swings from
   there; ranged sturdies still basic attack after the step when range allows.
3. Replace the strict `gap < my_gap` tests: `camp_tanked` and `finish_ok` compare real distances to each camp (no
   `min(d, attack_range)` cap for melees), and the chosen tile is the one that makes the test pass, so the decision
   does not flip when we step.
4. **Ranged hold**: `should_hold_back` holds only while no sturdy ally is within the camp's reach + 2. A ranged champion
   already engaged and not the camp's nearest target keeps attacking during a swap.
5. Hysteresis: a champion in "behind" mode stays in it until the group is dead or its health is back over the bar.
- Expected: most of the 1,128 hurt exits with a tank present become stay-and-hit turns; the hold-dance turns (2.7k) go.
- Fuel: per-turn work is a scan over the group's camps and the visible allies (both already read); the tile pick is
  one bitboard intersection.

### F3 TEAM_ABANDON: leaving a camp means everyone leaves until it sleeps
Shared memory has no free byte (bytes 0-7 all used), so the shared target carries the decision:
1. The leader abandons the farm target when an awake camp of it hits an ally who must leave (F2 says no sturdy ally
   can take it). The slot is dropped from `pick_target` until the group is seen asleep (state read) or
   `CAMP_SLEEP_ROUNDS` (2) + margin rounds with nobody in reach have passed. Followers get the new slot as now.
2. Because the old group is no longer the farm slot, the existing walk guard keeps every walk and dash out of its
   awake reach, and `foreign_camp_hit` sends anyone still inside out. Re-entry (49% of exits today) needs the group to
   be asleep.
3. Fights: the same group stays out of `fight()`'s steps only as a separate switch (`ABANDON_FIGHT`), benched on its
   own: fight keep-out lost before (v129b FIGHT_KEEP_OUT 86/160, v132 54-69).
- Expected: the 1,922 farm re-entries and part of the 265 deaths after exits go.

### F4 SAFE_LAND: no dash lands where its area hits a camp we did not choose to farm
1. One landing filter used by `dash_along`, `dash_ranked`, `hop_route`, `hor_dive` and `hor_camp_dash`: for a dash
   with a landing radius (HoR's ability_dash radius 1), a landing is barred if within the radius of a camp spawn tile
   (static map, from `camp_groups()` once) of a live or unknown group that is not farmable now. Farmable now =
   clusters only, or our farm target / its group while it passes the present-champion gate (F5). A tile in sight
   with no camp (dead or respawning) is free.
2. No legal landing: no dash. The walk runs as usual (the hop route walks to the edge and waits, or falls back to the
   walking route as it already does when no landing exists).
3. `aoe_scan`: the clear list adds the static spawn tiles of live non-target groups, so camps in bushes or behind
   walls are kept clear too.
- Expected: 243 off-target landings, 17 deaths and 174 exits per 65 games go; on 00great-wall HoR stops dying in the
  monolith chamber.
- Fuel: a static board built once; per dash one bitboard AND with the barred board (cheaper than v101's per-tile camp
  loop it replaces in `hop_route`).

### F5 PRESENT_GATE: the emblem gate counts only the champions present
1. Present = alive, not recalling or teleporting, within `TEAM_RANGE` of the target group (in sight, or last seen
   within `ALLY_GRACE` rounds). Free part = number present (was roster size); emblems = sum over present champions
   (was every key in `emblems`).
2. Team pick (`drop_hard`, `hard_ok`): uses those counts, so 3 champions out farm by their own 3 free + own emblems.
3. Per champion, before the first hit on an unstarted gated group (`hard_need > 0`): count allies within the group's
   reach + 3 and their emblems; if the gate fails, stage on the ring just outside the group's reach and wait for them
   (cap `STAGE_ROUNDS`, 4). Clusters-only and wolf groups never wait.
4. Same staging also fixes the arrival stagger for groups with strength: the opener waits on the ring until the
   present gate passes, so the team opens together (today median +3, 23% +8 rounds).
- Expected: no solo golem or elk-pack openings (ladder dunes r60-71), fewer camp deaths early.
- Fuel: present count reuses the vision list and the cached emblem map.

## Order, measurement and acceptance
1. Implement as `bots/v135` from v134 with the v135a audit lines; each fix behind its switch (F1-F5 above, plus
   `ABANDON_FIGHT`), default off, then on one at a time.
2. Debug audit after each fix (65-map mirror, same scripts) to confirm it fires and removes its waste:
   - F1: capture-target turns, step-back-step count (`waste.py`, the capture count in this plan).
   - F2: hurt exits with a tank present, `farm.camp.hold` turns, hurt.dodge <-> farm.camp alternations.
   - F3: re-hit-within-10-rounds after exits, deaths after exits.
   - F4: off-target landings beside live camps (`@land`), HoR camp deaths on 00great-wall.
   - F5: one-champion openings of gated groups, first-hit lag.
3. Release bench per switch vs v134: 65 maps both sides, 3 fuel-burn seeds (`bots/v133t/bench.py`), report record,
   banked gems, K/D, camp deaths and mean / out-of-fuel turns (`bots/v123/fuel_bench.py`). Noise is about +-5 wins
   per 130-game run, so keep a switch only if it is ahead on the 3-seed total.
4. Then all kept switches together vs v134, same bench.

## Scripts (in this folder, Python; run on logs extracted from the replays, NULs stripped)
`waste.py` back-and-forth and exit re-entry by branch; `oscwhy.py` reasons behind the top loops; `audit.py` tie
test, hurt exits with a tank present, landings; `turns.py LOG lo hi [cN,...]` prints turns for reading.

## Escort audit (added after the first plan; decision: remove it, F6 NO_ESCORT)
`replays/baudit2` (260 v135a logs): 774 escort requests, 657 called off as "ran too long", 43 banked, 47 ended home or
recalling, 12 with the carrier dead. 535 of the 657 time-outs came within 4 rounds of the request and 556 were printed
by the carrier itself: `update_carrier` sets the local `carrier`, so `sync_decision` never resets `carrier_since` on
the carrier's own bot, and a carrier that leads calls its own escort off at once and asks again. The carrier also
stopped to collect rewards (collect runs before carry), turned back to regroup when an escort drifted past
ESCORT_RANGE, and on 01boss-battle its altar walk found no step 260 times (the altar route is held by two monoliths).
The user chose to remove escorting; its 4 bits in the mode byte now carry TANK_ON_DUTY for the tank swap.
