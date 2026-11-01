# v170 (v155 + the Prism team on gem-cluster maps)

Same number as its platform version, v170. It was called local v169 while being tested, so `src/` comments and the results file use that name. Plays as v155 except on maps where over 70% of camps are gem clusters (`draft::prism_map`, 9 of the 70 local maps),
where it uses the Prism team. The earlier test folders were deleted; these are all its differences from `bots/v155`.

## Prism team (`Bot::prism_team`)

- **Draft:** Prism, Heart of Ruin, Marcella, Ryth, Willow. Four picks: Prism, Heart of Ruin, Marcella, Willow. Three picks: the first three.
- **Items:** Prism buys Durability, Prospector, Tribute (all Health), then Power Health. The others open with Prospector, then their build
  without Monster, Jungle or Treasure.
- **Gems:** Prism is first in line for gem rewards within 8 tiles, as a funnelled Vesper is for loot. With her further off, an ally may take one.
  Gold-only rewards go to her until her first two emblems are done, then to allies without Prospector.
- **Hold:** she tributes when tributed gems plus her bag reach 1000. Safety starts: the round limit, the enemy's pace, or the team's expected
  income finishing the 1000 during her channel. Allies guard her altar, except during that last kind of early start.
- **Team play:** no splitting; the team stays with her while she captures; allies leave fights away from her unless caught; Willow stays within
  3 tiles of her while farming; while her max health is under 1000 she stays behind melee allies in a fight.
- **Camps:** groups we can walk to no later than the enemy come first, with gem clusters valued highest and gold clusters lowest. With none up
  within 15 rounds of arrival, the team picks a goal anywhere.

## Results

See `results/results-v158-vs-v155.txt`. On the ladder: 29-16 over 45 games against 9 opponents, including 9-6 against 3 it had not played.
