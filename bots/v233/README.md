# v231 (v230 + RALLY and WILLOW_SHOOT)

Parent: bots/v230. Built by `tools/hc/p231.py` on a copy of v230. Two fight switches that came out of re-reading
`FIGHT_COORD_PLAN.md` against ladder replays (2026-11-11). Not live: platform version 230 (bots/v230) is selected.

## Switches (the "v231" block at the top of src/constants.h)

| Switch | Default | What it does |
| --- | --- | --- |
| `RALLY`, `RALLY_FOE_RANGE` 6, `RALLY_MATE_RANGE` 4 | true | A champion an enemy champion is hurting (60+ health lost in a round, enemy within 4) sends the ping whenever the enemies within 6 tiles are at least as many as the allies within 4 (itself counted), in any mode and with allies beside it. Before: only with nobody within 3 and only outside fight mode. The answer rules (healthy, light bag, within 10 of the pinged slot) and the outnumbered fall-back (`retreat_to_allies`) are unchanged; both now run for group fights too. |
| `WILLOW_SHOOT`, `WILLOW_SHOOT_HEALTH` 0.6 | true | Over 60% health Willow basic-attacks champions from outside their basic reach (2 tiles from a melee, 3 from a ranged, and every camp's reach), with the reset for a second shot when she had nobody to heal, and steps to such a tile within heal range of the front ally. Before (WILLOW_BACKLINE): she stood 5 tiles from every enemy and never fired. |

Plan parts: RALLY is a small F2 (the fight's place is shared when a real fight is not already ours). WILLOW_SHOOT is a
small F5 (a ranged champion's fight tile chosen by enemy reach). No shared byte changes.

## Why (ladder replays, 2026-11-11)

Control run: platform version 230 on test set 2 (`tools/hc/plat2.py`: the five teams ranked 115-131, just above our
133, and the top five, five maps each): 12-38, 10-15 against the near teams and 2-23 against the top five.

- **Willow deals no champion damage.** Platform, 23 games each: 0 damage, 0 kills, 14.3k healing; opponents' Willows
  deal 1.1k-2.6k. `willow_backline` keeps her outside THREAT_RANGE 4 of every enemy and her attack range is 3.
- **Fights without the allies standing by.** Against the top five, 97 of our deaths came by round 300 (3.9 a game);
  78% were outnumbered where they stood (the killers come three or four together), 47% had a healthy ally 6-14 tiles
  away that was in no fight, and for 20% those allies would have evened the numbers. Ad Astra on 03membrane, rounds
  59-88: Ryth and Willow fight three enemies while Marcella and Heart of Ruin shuffle between two tiles 9 tiles away,
  then all five die one after another. In our logs "fight mode, but no enemy to fight: go on" is 214 turns a game, and
  37% of the turns with an ally fighting within 12 tiles go to farming, walking or collecting. The ping only went out
  for a champion alone outside fight mode (12 a game; 27% of the senders died).
- Against the near teams the fights are not the gap: in those 15 losses kills and deaths are level (4.8 / 5.5 a game)
  and their camp damage is 1.65x ours over the game (1.34x by round 300: 26.4k against 19.7k, with one death a side by
  then). See "Where the gap is".

## Benches

Local, 25 maps of test set 2, both sides, debug with logs, against bots/live218 (`tools/hc/mini.py`):

| Bot | Record | Gems | K/D | Willow damage / deaths | Fuel mean / p95 / out-of-fuel |
| --- | --- | --- | --- | --- | --- |
| v230 (parent) | 22-28 | -8.5% | 466/420 | 0 / 1.78 | 39,480 / 70,483 / 0.30% |
| v231 (both) | 27-23 | +5.7% | 495/396 | 886 / 1.59 | 39,914 / 70,912 / 0.30% |
| v231r (RALLY only) | 28-22 | +16.4% | 487/385 | 0 / 1.48 | 39,812 / 70,538 / 0.30% |
| v231w (WILLOW_SHOOT only) | 28-22 | +12.6% | 513/410 | 903 / 1.93 | 39,734 / 70,576 / 0.29% |

A 50-game bench swings about ±5 wins, so the three variants are one result: better than the parent by about the noise.
In v231's logs: pings 26 a game (12), answer turns 32 (19), outnumbered fall-back turns 15 (8), claims given back for a
ping 6.2 (2.7), pings that ended with a kill 2.0 a game (0.4), pinger died 4.3 (2.8). Willow fires on 11 turns a game
and steps to a firing tile on 23. No new errors (0.5 error lines a game, 0.4 before).

Platform, test set 2 with each opponent pinned to the bot version it used in the control run (`--source-group`):

| Version | Record | Near teams | Top five | Near: kills / deaths | Near: margin change |
| --- | --- | --- | --- | --- | --- |
| c230 (control, live 230) | 12-38 | 10-15 | 2-23 | 9.7 / 4.9 | |
| p231 (this folder, platform version 231) | 9-41 | 9-16 | 0-25 | 10.4 / 5.2 | mean +36 gems, median -8; better in 11, worse in 13 |

Three cells flipped to a win and six to a loss; one game of each class was identical. Level: 25 games a class cannot
show less than about ±4 wins. Willow on the platform: 618 damage a game against the near teams (235 against the top
five), deaths 1.39 against 1.17 (1.52 against 1.43).

Verdict: both switches work as written, cost no fuel and gain locally, and neither moves the platform record. They are
not what decides ladder games.

## Where the gap is (control run, 50 games; tools/hc/contacts.py, finish.py, idle_allies.py, escapes.py)

- Near teams, our 15 losses: camp damage 6.8k against 8.7k by round 100 and 19.7k against 26.4k by round 300, with one
  death a side by then. Gold earned by round 300: ours Vesper 872, Marcella 135, Heart of Ruin 91, Ryth 86, Willow 33;
  theirs Vesper 440, Marcella 383, Heart of Ruin 233, Ryth 216, Vendetta 204. Our Marcella deals 1.6k camp damage by
  round 300 (their melee champions 4.3k-7.0k).
- In our logs (five-champion games, rounds 1-300) a melee champion hits a camp on 4-13% of its turns: 19-26% go to
  walking to the next group, half of the "camp" turns to closing on a camp others already hit, 8-10% to waiting beside a
  respawn, 15-19% to fight mode.
- Near teams, our 10 wins: 13.7 kills a game (Vesper 7.5) against 4.4 in losses. Contacts are as frequent in both (17 and
  19.5 a game); in wins 71% of the enemies we bring under 40% health die, in losses 46%.
- Top five: 9.2k camp damage by round 100 against our 4.5k, two of our champions dead by then, 2.6x our gold. We lose
  contacts we enter with more bodies (24 kills, 89 deaths).

## Tools added (tools/hc; usage at the top of each)

`plat2.py` (test set 2, pinned opponents), `platpair.py` (paired cells of two runs), `mini.py` (the 25 maps locally,
with logs), `contacts.py`, `finish.py`, `escapes.py`, `idle_allies.py`, `epview.py` (platform replays), `disagree.py`,
`lowfoe.py`, `shuffle.py`, `champs.py` (local logs and statistics).

## Clean-up

Test variants, results recorded above: bots/v231r, bots/v231w (single switches). bots/v232a is the next step (see its
README). Replays: replays/plat2 (50 decoded platform games), replays/hc/mini-* (local benches with logs).
