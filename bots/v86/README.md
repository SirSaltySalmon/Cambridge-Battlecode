# v86

`v82` + Mike's farming tree: farm fast where it is safe, as a team where it is not, and on the enemy's side only right
after a kill. Submitted as platform version 86. Every part has its own switch in `src/constants.h` (`V86_*`); the new
code is in `src/phase.cpp`.

## What it does

**Zones (draft).** The draft turn (10x fuel) labels every camp group by walking steps to each spawn: *safe* when
steps from ours x 1.5 <= steps from theirs, *unsafe* the other way round, *uncertain* in between. The labels, both
step counts and whether a pair can clear the group alone go to draft memory, so no champion pays for the fields.

**Phases (the leader, shared byte 5).** The leader system stays; it now also sets a phase that decides which zones
the target pick may use:

| Phase | Targets | When |
| --- | --- | --- |
| centre | safe + uncertain | 2+ of us alive and fewer than 2 champions down, else 3 together or 2+ emblems per champion over theirs |
| group | safe only | the centre rule fails |
| split | safe only | quiet (no enemy seen for 10 rounds), main target safe, a far safe group the pair can clear |
| raid | everything | within 15 rounds of a kill, more of us together (all over 60%) than enemies alive, nothing else up for 8 rounds, and one unsafe group fits before the dead enemy respawns |

The shared round stamp is now one byte (round % 255 + 1), which frees byte 5: phase in bits 6-7, the split pair's
target in bits 0-5. Only the leader starts a phase; anyone can end a split.

**Kill timers.** Every champion reads every enemy's life each turn and remembers the round it died (respawn = death +
60). Enemy emblems are remembered from sightings (read on sight, then every 5 rounds).

**Split.** Cassi & Issac (one melee, one ranged, they die together; Willow never splits) farm a second safe group 10-26
tiles from the main target. Issac picks the pair's group; Cassi captures, and Issac opens the next pair group only if it
is within 6 tiles of her. The split ends when either sees a near enemy, the partner is out of sight for 3 rounds (a
recall, a death), one carries 150 gems, there is no leader, or the main team's safe groups run out; then 30 rounds'
cooldown.

**Per champion.**
- 35-60% health off a fight: bank a bag of 10+ at an altar within 10; with an ally in sight farm on; alone outside the
  safe zone farm a safe group within 12, else recall.
- Under 35%: a champion keeps finishing a camp of its target that is already started, while the hits it would take
  leave it over 20% (no enemy in sight, never with a 60+ bag).
- Mages basic attack camps (and tank them when nobody else does); they cast from outside camp reach only alone (or as
  the pair) in the safe zone under 60%, on a started camp outside the safe zone, and on monoliths.
- While a reward outside the safe zone is captured, the rest only move on to groups within 6 tiles of it.

**Fuel.** Mean 32k per turn against v82's 37k: v82 built its log text (shared decision, score reason) in release
builds too; it is now inside `debug_block`, and ally emblems are read every 4 rounds (they only change at a shop).

## Results (34 games each, all 17 maps from both sides)

| Bot | vs v82 | vs v82 (unlimited) | vs brawler | vs v80-5 |
| --- | --- | --- | --- | --- |
| v86 | 18-16 (+7.5%) | 21-13 (+16.8%) | 17-17 (-2.9%) | 20-14 (+19.3%) |
| v82 | 17-17 | 17-17 | 15-19 (-16.6%) | 21-13 (+10.3%) |

Release build (platform): 16-18 (+4.6%) vs v82, 109 turns at 100k+ fuel per 34 games (v82 258).

## Ladder (unranked: the same 16 groups, team + 5 maps, as v82's A/B; `replays/ladder-v86/plan.txt`)

| | record | tributed / game | camp gems by r200 / r400 / total | opp camp gems total | kills / deaths |
| --- | --- | --- | --- | --- | --- |
| v86 | 33-47 | 745 vs 868 | 231 / 453 / 807 | 906 | 8.8 / 7.8 |
| v82 | 33-47 | 711 vs 816 | 201 / 421 / 799 | 826 | 10.7 / 8.8 |

Same record. v86 is ahead early and dies less, but ends with the same camp gems, and the opponents farm more against it
(their side is left alone unless a raid fires, and raids almost never fired). Details: `results/results-v86-ladder.txt`.

Rejected while building it (unlimited fuel vs v82, against v86's first draft at 14-20):
- Recall to shop at 150 gold without banking first (`V86_GOLD_RECALL`): off 21-13; the trips home cost more farming
  than the earlier items gave.
- A centre gate on the leader's own sight (max(2, enemies alive - 1) together): 20-14 vs 22-12 with no gate; the leader
  walks ahead and undercounts. Replaced by the alive counts above.
- Mage rule off: 18-16 (-5.4%) vs 20-14, so the rule stays. Split off: 21-13 vs 20-14 (neutral), kept.
