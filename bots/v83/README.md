# v83

`v82` + the v83-* changes, each behind its switch in `constants.h` (`bots/v83-<name>` = v82 + only that change, with
its own README and results). Submitted to the ladder on 2026-10-19 for unranked tests.

| Change | Switch | In v83 | Folder | What |
| --- | --- | --- | --- | --- |
| Hidden-unit step | `FIX_HIDDEN_STEP` | on | v83-step | A step refused by an unseen unit is retried on the next tiles (was: stand still; 370 idle turns per 34 games -> 7). |
| Aggressive pocket on our side | `AGGR_OUR_SIDE` | on | v83-aggr | Mike's "Ren walks to the middle at round 162": the centre pocket was a ~50-round walk onto the enemy's row on rows. |
| Decision memory | `MODE_MEMORY`, `CHASE_MEMORY` | on | v83-hold | Indecision: the leader holds fight/retreat 3 rounds after it last saw an enemy; a chaser follows a vanished enemy 2 rounds. Bounces 627 -> 306. |
| Useful wards | `WARD_USEFUL` | on | v83-ward | No catch-up wards on open ground 2 tiles from us (72% of v82's ward moves); only into unseen bushes. |
| Altar locks | `LOCK_*` | on | v83-lock | Buy one after the core build; cancel enemy tributes, deny altars, guard our own tributes. |
| Farm-map split | `FARM_SPLIT` | on | v83-split | Mike's idea: on far-apart maps (corners, expanse, monolith, stampede) Cassi & Issac farm a second pocket. |
| Race banking | `RACE_BANK` | on | v83-race | Once the enemy has tributed 700, every bag goes to the nearest altar. |
| Fight threshold 0 | `FIGHT_THRESHOLD` 5 -> 0 | on | v83-bold0 | The always-fighting brawler beats v82; fight more readily. |
| Mage standoff cutoff | `MAGE_CHEESE_EMBLEMS` = 4 | on | v83-cheese4 | Mike: stop mage cheesing at some emblem count; 1-2 hurt, 3+ neutral. |
| Decision log on the platform | `LOG_RELEASE`, `LOG_TEXT` | on | v83-log | One ~60-byte line per turn in release builds; log text only built in debug builds (-19% fuel, decisions identical). |
| Group tribute at 200 | `GROUP_BANK` | off | v83-group | Mike's idea; worse in local tests. |
| Teleport crystals | `TP_CRYSTAL` | off | v83-tp | Rarely fires; 76-94 vs v82's 87-83. |
| Smaller bags | (`GEM_BANK` 25, Ren 80) | not applied | v83-bank | Neutral locally. |

## Results (local)
34 games per column, all 17 maps from both sides, debug builds (the full log is free there). The brawler is v82 that
always fights (`bots/spar-brawler`), the locker locks every altar it passes (`bots/spar-locker`). Over 170 games
differences under ~9 wins are noise; v83 is +6 wins on v82's row and its gem margin is better against all but v80-5.

| Bot | vs v82  | vs v82 (unlimited) | vs brawler  | vs v80-5  | vs locker  | Total |
| --- | --- | --- | --- | --- | --- | --- |
| v83 | 20-14 (+4.7%) | 19-15 (+8.5%) | 17-17 (-4.1%) | 17-17 (+9.1%) | 20-14 (+5.8%) | **93-77** |
| v82 | 17-17 (+0.0%) | 17-17 (+0.0%) | 15-19 (-16.6%) | 21-13 (+10.3%) | 17-17 (+3.6%) | **87-83** |

Release builds (as on the platform), normal fuel, vs v82 release: **22-12, gems +9.2%** (v82 mirror 17-17). Fuel per turn:
mean 30.4k, p99 68.5k, 18 turns over 100k in 34 games (0.01%); v82: 36.8k, 84.5k, 258 (0.15%). In debug builds (log text
built) v83 averages 37.5k, like v82.

Also fixed: two log reads right after a mage cast (`mage_camp`, `mage_fight`) read the target the spell had just
killed and threw "Unit is not in vision" (~5 a game in v82, ending that turn); the values are now read before the cast.

## Ladder (unranked)
Submitted 2026-10-19 (platform bot version 83) and played in unranked match groups against the same 8 opponents and the
same 5 maps each as v82's last 40 ladder games (CompetitiveBasmaga, Ninja Turtles, hold up, cheesynachos, Gem_alert,
FurFortnite, Ascendance, Rogue Lock Pickers; ranks ~150-200). `tools/ladder.py replays/ladder-v83`. We were always side
A in these groups; v82's games had mixed sides.

| Per game | v82 (ranked, same matchups) | v83 (unranked) |
| --- | --- | --- |
| Record | 16-24 | 15-25 |
| Gems tributed | 720 | 788 |
| Gems lost on death | 205 | 138 |
| Our deaths / enemy deaths | 11.3 / 11.3 | 10.0 / 12.8 |
| Tributes | 10.2 | 12.2 (opponents 17.3) |
| Ward moves | 212 | 40 |
| Altar locks bought | 0 | 7.8 |

Matched pairs (same opponent and map): v82 11 wins of 29, v83 12. Rows went 0-4 -> 2-2 (the third loss 990-1,056);
monolith 388 -> 726 and elkring 450 -> 648 in losses; expanse 3-1 -> 1-3, pilgrimage and packs 1-1 -> 0-2, collision
484 -> 44 (18 deaths, 2 tributes). Wins did not move, the underlying numbers did.

What the new release log shows about the losses: rushbait_v2 (0-4 again) and monolith (0-3) are farming races, not
fights (dead 4-13% of turns); 16-23% of our turns are walking between camps (`farm.walk`), and opponents capture 34 camps
a game to our 27. Farming in more than one group on more maps is the next lever.
