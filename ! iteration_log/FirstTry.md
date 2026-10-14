# Strategy for first iteration

Some basic ideas than can be expanded upon:
- Optimal location in map is often center / where gems are most concentrated
==> Go there as often as possible
- Go as a team and engage when
- Maintain range while engaging, build an aggressive score
1. Enemy is stronger in terms of numbers and health? Stay away, retreat when needed
2. Enemy is weaker? Try to close distance. If enemy retreats too far from center, abandon and keep farming.
3. Strategy definitely varies based on champion. Fast / mobile assasin should pursue
champs often, especially if have lots of gems.
- Meta champs and counterpicks?
1. As far as I'm seeing Cassi and Issac are always to be picked
2. CC probably are quite underutilized, needs to benchmark to see how strong it is
3. Have different teams and strategies for each map. Don't worry too much about enemy teams for now. Some teams perhaps are built to be more aggressive, some are less. => More ranged in open maps, more melee and mobility in closed maps. CC and retaliation pairs well with melee, pure tanks pair well with ranged is my guess.
4. Willow is definitely underutilised... average algorithms probably doesn't factor in healing too much. But it might just because it sucks
- Code should be VERY VERY modular with tunable parameters for testing and pitting against each other.
- Buying splits are interesting...
1. Do take a lot of damage from jungle... but early game it is probably smart to contest first, so buy Power Strength / Focus Jungle
2. Then buy Power Jungle / Focus Monster
3. Then buy Power / Health Bruiser depending on assasin or tank, spam it, or Focus Durability

Plans for first draft:
One single team: Cassi & Isaac, Varg, Vendetta, Willow, Ren, no need for counterpicks.
Strategy: Always go together, find center and farm, engage and retreat
Priorities:
1. Low health: retreat & recall, then buy items
2. Go to central farming spot as a group - How do you find a central spot?
3. Engage with enemies if aggression score is suitable (shared memory)
4. Engage with monster camp, from a reasonable distance for easy retreats
5. Find nearest altar and tribute gems (if locked, implicitly converted to an attack)

How to calculate aggression scores? Should be tunable and benchmarkable
Outnumber enemy: + 10, else -5 * difference
Have higher combined health values: + 5, else -5
Have higher combined strength and focus: + 5, else -5
Enemy have high gems: + 2
Have n emblems: +n per character
Distance increased from camp if we are to pursue: -0.5 per tile
Own count shouldn't affect aggression scoring. If we fight, we must win.
Score must be at least 1 or above to engage. Else, retreat.
Always stay at sufficiently away in terms of attack range of enemy if not engaging.
Mobile characters should be able to move up, attack, then return to original distance, if not engaging or retreating.

Next priority ideas:
- Stay away from enemy spawns, since they can attack you and stay at full health there and there's usually 0 merit
- Switch targets after have become strong enough to contest for elks and then monoliths

Not yet in scope:
- If enemies have 300 more gems, buy lock for altars

# Log

## Implemented: first-draft turn loop (bot.py, draft.py)
- Draft picks in order: Cassi & Issac, Vendetta, Varg, Willow, Ren. Picks before any heavy computation so a fuel overrun can't cost us the pick.
- Draft finds a "pocket" (dense cluster of wolf / gem / gold / elk / monolith camp groups) and writes it into draft memory. Bots fall back to computing it themselves if draft memory is empty.
- Turn order per champion: shop while dead -> don't interrupt channels -> on spawn buy + heal then head out -> recall below 35% hp (step out of threat range first) -> bank at 40 gems -> follow team mode (fight / retreat / farm).
- Team mode lives in the 8 bytes of shared memory (mode, target slot, round stamp). First living champion each round scores it, others read it. Priority order: Cassi > Issac > Vendetta > Varg > Willow > Ren.
- Aggression score implemented as written above (outnumber +10 / -5 per missing body, health +-5, power +-5, 0.04 per enemy gem, +1 per emblem, -0.5 per tile from camp). Fight at >= 1.
- Ranged champions hold 4 tiles off a camp until a melee ally is closer, so the camp hits the melee first.
- Kits: Varg shields when threatened, Ren stuns before moving, Issac / Vendetta / Ren dash to targets, Issac / Vendetta attack-reset, Cassi stuns (channelling enemies first) and nukes, Willow heals the lowest ally.
- Shopping: melee buys Jungle -> Monster -> Bruiser Strength, ranged buys Jungle -> Monster -> Durability Focus, one emblem at a time, then spams Power shards.
- Cleanup: removed the separate params file, all constants live at the top of bot.py.

## Problems seen after the first draft
- Pockets were picked by "most groups within 8 tiles", ties broken by map centre. Altar distance was ignored and a gold cluster counted the same as a wolf, so pockets were often far from the altar.
- After clearing, the team stepped through pocket slots in order and only skipped groups it could currently see respawning. It didn't roam to nearby groups, and it stood on spawn tiles of respawning groups (occupied spawn tiles delay the respawn).
- Gold piled up with no reason to go home and shop.

## Changes
1. Pocket scoring (draft.py)
   - Each group gets a value in gems: gems + 0.2 * gold - 0.01 * (damage the team takes while clearing). Clear time is estimated from camp health / 200 team damage per round. Camp health/strength comes from the rules table since camps outside vision hide it.
   - Income = value / (respawn + clear time + capture time).
   - Pocket score = sum of member income * 15 / (15 + average walking distance to the nearest altar). Walking distance comes from one distance field spread from the altars, around walls and holes. ALTAR_FALLOFF = 15 means a pocket 15 tiles from an altar is worth half.
   - Map-centre tie-break removed, the altar term replaces it.
   - The damage term is what keeps Monoliths out: 260 gem-value minus ~200 for eating 200 dmg/turn for ~100 rounds.
2. Roaming route (draft.py + bot.py)
   - Draft memory now holds a route: every eligible group within 14 tiles of the pocket anchor, not just the 8-tile pocket.
   - Decision: static ranking + vision memory, not vision alone. Vision is per champion, only 8 tiles, and the 8 bytes of shared memory can't hold respawn timers. CampGroupRespawning.timer gives the exact rounds left, so one sighting is enough.
   - Each champion keeps its own `ready_at` per route group, updated from whatever it sees. Unseen groups are assumed up, so the worst case is one wasted walk.
   - The deciding champion picks the target with the best value / (travel + wait on arrival + clear + capture). A new target must beat the current one by 25% (SWITCH_MARGIN) to stop flip-flopping.
   - While the target is still respawning, wait within 2 tiles of it, not on its spawn tiles.
3. Downtime banking + gold recall (bot.py)
   - If our target is still respawning when we'd arrive, bank any bag of 10+ gems (IDLE_BANK) instead of waiting for 40.
   - After a tribute completes (bag empty) with 150+ gold (one full emblem, RECALL_GOLD): recall to shop. Walk instead if spawn is within recall time (5 tiles), and skip it while inside an enemy or awake-camp threat zone.

## Results vs starter-python (all wins)
| Map | Win round | Gems | Gold earned |
| --- | --- | --- | --- |
| collision | 786 | 1044 - 374 | 4990 |
| corners | 315 | 1036 - 0 | 250 |
| expanse | 616 | 1026 - 696 | 1600 |
| interlock | 550 | 1028 - 610 | 2080 |

On collision the team now cycles route slots 1 / 2 / 7 / 9 as each respawns, instead of stepping 0 -> 7 in order. No baseline numbers from before these changes on the same maps, so the improvement isn't measured yet.

## Bug fixes
- "Memory access requires the owning team's input boundary" (one per draft turn): the draft picked first, then submitted. Picking ends our input for that turn, so the submit (which reads draft memory first) was rejected. Draft memory never arrived and every champion silently recomputed the pocket on its first turn. Fix: compute + submit, then pick. Confirmed champions now read the route from draft memory.
- "Unit is not in vision" (~20 per game): using a camp/champion after we killed it earlier in the same turn. Melee checked `target.health()` after the killing blow (before attack reset); Cassi's damage spell could kill the camp before `should_hold_back` read its position. Each one threw away the rest of that champion's turn. Fix: check `alive([target])` after anything that can kill it.

## Change: one collector per cleared group
- Problem: after a group died, the whole team stood on it for the 5-round capture. Only one body is needed.
- Decision: the deciding champion assigns one collector (nearest visible, non-channelling ally to the group's spawn tiles). Everyone else gets a new target that skips the group being collected.
- Shared memory layout changed to fit it: mode, target slot, capture slot, collector champion type + 1, round stamp (now u16, rounds never pass 2000), claim round (u16). 255 in capture slot = nobody collecting. Champion type works as an ID because it's unique per team (Cassi and Issac are separate types).
- Each champion now also remembers which route groups it saw cleared with the reward still uncollected.
- The collector releases the claim itself (writes 255 into the capture byte) once it sees the group respawning. A claim older than 15 rounds (CAPTURE_TIMEOUT) is dropped so a dead or lost collector gets replaced. The deciding champion forgets a reward once it claims it, so only a fresh sighting can send a second collector.
- Collector duty comes right after the low-health recall and is skipped while something can hit the collector, so it still fights / retreats if threatened.
- Bonus: any cleared-but-uncollected group on the route gets claimed, including ones an enemy cleared and left.

## Results after both fixes + one collector (all wins)
| Map | Win round | Before | Gems | Gold earned |
| --- | --- | --- | --- | --- |
| collision | 690 | 786 | 1046 - 432 | 4980 |
| corners | 292 | 315 | 1036 - 0 | 250 |
| expanse | 776 | 616 | 1080 - 858 | 1760 |
| interlock | 493 | 550 | 1052 - 508 | 2500 |

Zero bot errors in the logs. Expanse got slower. Its route is only 3 groups (each an elk + 3 gem clusters), so the team rotates through them on long respawns. Likely cause: the collector now gets the whole reward, so one champion fills its 40-gem bag alone and leaves to bank, and the elk is fought a body short more often. Not confirmed yet.

## Open issues / next
- Expanse slowdown above. Ideas: let the collector rejoin before banking, or lower GEM_BANK for whoever just collected.
- The gold recall isn't directly confirmed in a log yet.
- Tunables to benchmark: ALTAR_FALLOFF, ROAM_RADIUS, GOLD_WEIGHT, DAMAGE_COST, TEAM_DAMAGE, SWITCH_MARGIN, IDLE_BANK, RECALL_GOLD, CAPTURE_TIMEOUT.
- Possible extra pocket terms: distance to our spawn, penalty near the enemy spawn (see "Next priority ideas").

## Real bots test
Fails miserably, usually losing by half. Not banking often enough?
Assumption is being with the whole team most of the time, you can't lose. But you do end up losing a teammate, and this assumption will fail you the game.
Need a lot more data