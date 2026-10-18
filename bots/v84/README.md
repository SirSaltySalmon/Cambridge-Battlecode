# v84

`v83` + `FIX_HOME_SIDE` (switch in `constants.h`, now **off**: no measurable effect). Not submitted.

## Idea
On the ladder, rows (a snake map: wolf rows by each spawn, altars in the middle rows) went 2-10 for us; we captured
8 camps a game to the opponent's 18, and 47% of our team-target turns pointed at the enemy's half. The rule: at
setup every group's walking distance from our spawn and from the enemy's is computed (`draft::group_walking_gaps`);
a group clearly closer to the enemy (enemy steps < ours - 4) keeps 30% of its value in the farming plan (and, in
the second version, in the aggressive plan too).

## Results
| | full 68 games vs v83 | rows / corners / eight / collision vs v76, v74, brawler (48 games) |
| --- | --- | --- |
| v83 | - | 25-23 (rows 6-6) |
| v84, normal plan only | 32-36, gems -0.2% | 25-23 (rows 6-6) |
| v84, normal + aggressive plan | 31-37, gems +0.5% | 21-27 (rows 4-8) |

Target turns on the enemy's half on rows (local): v83 29%, v84 25%, v84 + aggressive 23%. Locally rows is not a
problem: our bots capture as many camps as their opponents (~14.5 vs ~15.5).

## What the ladder rows games really show (12 games)
Captures per game by where the camp is: us 7.5 on our side, 0.8 on theirs; them 17.3 on their side, 0 on ours.
Nobody steals: the opponents farm their own three wolf groups more than twice as often as we farm ours. The gap is
farming efficiency on our own side (walking, banking trips through the snake to the middle altars, waiting), not
which side we target. Our local bots all share that habit, so local tests cannot show it.
