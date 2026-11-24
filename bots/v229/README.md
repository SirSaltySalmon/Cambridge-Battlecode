# v228 (v220 + survival and cluster-map fixes from the 2026-11-10 ladder study)

Contents in one line: bots/v220 + TRIBUTE_STAY + HOR_HOLD off + DODGE_HIT + PRISM_WANT_RANGE 1 + RESPAWN_CLEAR /
RESPAWN_WAY + ROAD_WALK + BAND_BANK_STICKY. The JOBS allocator (v221), the Cassi & Issac rosters (v222) and the leashed
jobs (v226) are compiled in and off.

Not live. Platform version 218 (bots/live218) is still the selected bot; see "Platform" below before switching.

## What changed and why

Every number is from this session; the tools are in tools/hc and tools/replay (usage lines at the top of each file).

| Switch | What it does | Evidence |
| --- | --- | --- |
| `HOR_HOLD = false` | Heart of Ruin no longer stays in a fight under the 35% recall bar "while his shield outlasts one burst". | He was 40% of our deaths to champions in 26 decoded platform games (59 of 147; next Ryth 25), 72% of them with as many or more of our bodies within 5 tiles. Patch 4.0.0 cut his shield and dash by a fifth. Mirror vs the same bot with it on: 141-115, gems +9.6%, his deaths 2.19 vs 2.70 a game. Platform, same lineage (90 paired cells): 24 wins vs 19, six cells flipped to a win and one to a loss; his deaths by r600 1.00 vs 1.28. A higher bar (`HOR_RECALL_HEALTH` 0.5) was level, so it stays 0. |
| `DODGE_HIT = true` | A melee champion stepping out of enemy reach under the recall bar hits the weakest enemy in reach first (melee walk and hit in one turn). | The commonest death of HoR and Ryth in the mirror was five dodge turns in a row with no hit back (97 and 99 of 532 / 389). Mirror vs v223: 140-116, gems +9.8%, K/D 2174/1962. Fighting on instead of dodging (`DODGE_STAND` 1 or 3) lost: 119-137 and 127-129. |
| `TRIBUTE_STAY = true` | A tribute is only cancelled for an enemy in reach under 40% health. Damage never breaks a tribute. | 00swirl vs an idle enemy standing 3 tiles from the altar: 593 tributes started and cancelled. In the live bot's mirror 134 of 7,641 tributes were cancelled (15 on 00the-line). |
| `PRISM_WANT_RANGE = 1` (was 8) | On the Prism team a cleared gem reward is hers only while she is within 1 tile of the group's nearest tile; further off, whoever cleared it captures it. | The team walks as one group, so she was always "near" and every capture queued behind her: 48% of turns on 04gem-caverns were "cleared and hers: wait off its tiles". Unopposed, 13 cluster maps: 1,193 -> 886 rounds to 1,000 gems, banked by r600 114 -> 285. Platform, 26 cluster cells: gems collected by r600 660 vs 597, banked 298 vs 222, deaths 1.8 vs 2.2 (wins 9-17 vs 10-16: noise). |
| `RESPAWN_CLEAR`, `RESPAWN_WAY_RANGE = 4` | A group seen respawning is never walked onto, and while an ally stands on one of its tiles the others wait 4 tiles off so it can step off. | adgato-gemcombat5's 20-cluster group sits in a walled pocket with a three-tile mouth: the waiters filled the mouth, Prism could not leave her tile, and the group never respawned (0 more gems for 1,875 rounds; v220 too). Now 574 rounds unopposed. |
| `ROAD_WALK` | A living group none of whose camps are in sight is walked to by the tiles not seen empty; standing on a dead member's tile of a living group is fine. | 01yellow-brick-road's 29-cluster road with its near end cleared (found while testing JOBS). |
| `BAND_BANK_STICKY = 12` | The hurt-band bank walk keeps its altar for 12 rounds. | 04pinwheel: the walk and the next rule undid each other at the range's edge. |

## Local benches (128 maps)

| Bench | Result |
| --- | --- |
| v228 vs bots/live218 (platform 218), both sides, release | 131-125, gems +9.6%, K/D 2392/2029, camp deaths 129 vs 131 |
| v228 vs v220, both sides, release | 136-120, gems +5.8%, K/D 2321/1970 |
| v228 vs v223 (v220 + TRIBUTE_STAY + HOR_HOLD off) | 141-115, gems +8.3% |
| Unopposed (vs bots/idle), rounds to 1,000 gems | 1,090 (v220 1,129); unfinished maps 7 (v220 10) |
| Fuel, unopposed | mean 40,650 a turn (v220 40,367), out-of-fuel turns 0.45% (0.43%) |

One 256-game bench swings about ±4% between two near-identical builds (two no-op variants scored 48.0% and 48.8%
against their parent), so each single switch above is weak on its own; the K/D and gem deltas all point the same way.

## Platform (results/plat, tools/hc/plat.py)

The test set is 20 opponents ranked 5-160 x 5 maps (13 cluster maps, our 20 worst ordinary maps, 17 others), 100
unranked games. It is a hard set: the live bot went 23-77 on it.

| Version | Record | Cluster maps | Other maps |
| --- | --- | --- | --- |
| base218 (live) | 23-77 | 8-18 | 15-59 |
| pd0 (v220 + TRIBUTE_STAY, 90 cells) | 19-71 | | |
| p223 (+ HOR_HOLD off) | 26-74 | 10-16 | 16-58 |
| ppw1 (+ DODGE_HIT, PRISM_WANT_RANGE 1) | 25-75 | 9-17 | 16-58 |
| p228 (this folder) | see results/plat/p228.json | | |

Two builds that play the same on ordinary maps differed by 4 wins there (16-58 and 20-54), so a 100-game run cannot
show a +3 change in wins; the per-champion deaths and the gem curves (tools/hc/platcurve.py) are the steadier read.

## Tried and dropped (flags stay in the source, off)

- **JOBS, mode ALL** (v221: every champion its own group, five 6-bit claims in the shared bytes): unopposed 04gem-rush
  2,000+ -> 666 rounds and -1,000 rounds on other cluster maps, but lone champions lose the 1v1s and 1v2s. Mirror
  109-147; platform cluster maps 2-24 against 8-18, 7.0 deaths by r600 against 2.7.
- **JOBS, mode CARRY** (only Vesper takes jobs): +4% gems unopposed; platform 24 vs 26 of 100, flips 8 up / 10 down.
- **JOBS, mode LEASH** (v226: Prism-team allies take cluster groups within 8 tiles of her): no faster unopposed than
  PRISM_WANT_RANGE 1 alone (882 vs 886 rounds), and 3.0 deaths by r600 on platform cluster maps against 1.8.
- **DRAFT_SET 1** (Cassi & Issac for Ryth): +9% camp damage by r100 and 135-121 in the mirror, but Cassi (600 health)
  dies first in every pair death (280 of 280 in a 128-game mirror, from full health six turns earlier) and takes Issac
  with her. Platform 22-78 and 20-75. It needs a Cassi that stays out of reach and buys health first.
- **Greed emblems as the carries' fallback** (40 shards for 750 gold since patch 4.0.0): pace 1,137 vs 1,129 rounds,
  mirror 121-135. Late-game farming is limited by walking and respawns, not by the carry's damage.
- **Relaxed camp caution** (unopposed screens): RECALL_HEALTH 0.25, BAND_HEALTH 0.45, CAMP_EXIT_ALWAYS off,
  V93_CAMP_EXIT off, TANK_SWAP off: all within 2% of 1,111 rounds. PT_GATE off: 1,088 rounds (-2%), untested in fights.

## Where the gap to better teams is (for the next change)

From 100 baseline platform games and 26 decoded replays:
- In our losses on ordinary maps the opponents deal 9.3k camp damage by r100 to our 7.7k and 60.9k to 43.8k by r600.
  Split: 12% more champion-turns (Cassi & Issac are two bodies), 31.5% of turns on a camp against our 28.3% in r1-300,
  25.5% against 19.6% in r301-600, and the same damage per camp turn. Both sides walk about 45% of their turns.
- They are alone or in pairs 57% of the time (we 44%); we stand three or more together 55% of the time (they 42%).
- Own-half gem supply per map and round (ceiling): elk groups 0.72, wolf groups 0.62, pure gem clusters 0.34,
  monoliths 0.27. Unopposed we take about a quarter of it.
- 04gem-rush and 04gem-caverns (58 and 114 single clusters) cannot be won by one group: opponents farm them alone and
  finish in 600-750 rounds.
