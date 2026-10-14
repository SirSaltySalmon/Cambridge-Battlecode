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

Not yet in scope: If enemies have 300 more gems, buy lock for altars