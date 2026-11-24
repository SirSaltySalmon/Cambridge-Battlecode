# v230 (v220 + survival and cluster-map fixes)

Parent: bots/v220. v230 is bots/v228 written out cleanly: v220 plus the seven switches below, without the experiments
that lost (JOBS, the job claims, DRAFT_SET, the leashed jobs; they stay in bots/v221-v228 for the record). Built by
`tools/hc/p230.py` on a copy of v220.

Not live. Platform version 218 (bots/live218) is still the selected bot; `python tools/hc/plat.py activate <N>` selects
another one (N = the platform version number `cambc submit` printed).

## Switches (all in the "v230" block at the top of src/constants.h)

| Switch | Default | What it does |
| --- | --- | --- |
| `HOR_HOLD` | false (was true) | Heart of Ruin no longer stays in a fight under the 35% recall bar "while his shield outlasts one burst". |
| `DODGE_HIT` | true | A melee champion stepping out of enemy reach under the recall bar hits the weakest enemy in reach first (melee walk and hit in one turn). |
| `TRIBUTE_STAY`, `TRIBUTE_STAY_HEALTH` | true, 0.4 | An enemy in reach ends a tribute only under 40% of our health. Damage never breaks a tribute. |
| `PRISM_WANT_RANGE` | 1 (was 8, from the group's centre) | On the Prism team a cleared gem reward is hers only while she is within 1 tile of the group's nearest tile; further off, whoever cleared it captures it. |
| `RESPAWN_CLEAR`, `RESPAWN_WAY_RANGE` | true, 4 | A group seen respawning is never walked onto; while an ally stands on one of its tiles the others wait 4 tiles off so it can step off. |
| `ROAD_WALK` | true | A living group with no camp in sight is walked to by its tiles not seen empty; a dead member's tile of a living group holds nothing down. |
| `BAND_BANK_STICKY` | 12 | The hurt-band bank walk (35-60% health) keeps its altar for 12 rounds. |

## Why (evidence per switch)

- **HOR_HOLD off.** In 26 decoded platform games he was 40% of our deaths to champions (59 of 147; next Ryth with 25),
  72% of them with as many or more of our bodies within 5 tiles, a median 13 rounds after he was last at 90% health.
  Patch 4.0.0 cut his shield and dash by a fifth (Focus 100 -> 80). Mirror against the same bot with it on: 141-115,
  gems +9.6%, his deaths 2.19 vs 2.70 a game. Platform, same lineage, 90 paired cells: 24 wins vs 19 (six cells flipped
  to a win, one to a loss); his deaths by r600 1.00 vs 1.28. A higher bar for him (50%) was level (130-126).
- **DODGE_HIT.** The commonest death of HoR and Ryth in a 128-game mirror was five dodge turns in a row with no hit back
  (97 of 532 and 99 of 389). Mirror: 140-116, gems +9.8%, K/D 2174/1962. Standing to fight after one or three dodge
  turns instead lost (119-137, 127-129), so the dodge itself stays.
- **TRIBUTE_STAY.** 00swirl against an idle enemy standing 3 tiles from the altar: 593 tributes started and cancelled.
  In the live bot's mirror 134 of 7,641 tributes were cancelled (15 on 00the-line).
- **PRISM_WANT_RANGE.** The Prism team walks as one group, so she was always "near" and every capture queued behind
  her: 48% of turns on 04gem-caverns were "cleared and hers: wait off its tiles". Unopposed, 13 cluster maps: 1,193 ->
  886 rounds to 1,000 gems, banked by r600 114 -> 285. Platform, 26 cluster cells: gems collected by r600 660 vs 597,
  banked 298 vs 222, deaths 1.8 vs 2.2 (wins 9-17 vs 10-16, inside the noise).
- **RESPAWN_CLEAR / RESPAWN_WAY.** adgato-gemcombat5's 20-cluster group sits in a walled pocket with a three-tile mouth.
  The waiters filled the mouth, Prism could not leave her spawn tile, and the group never respawned: 0 more gems for
  1,875 rounds (v220 too). Now 574 rounds unopposed. Ladder record on that map before: 7-14.
- **ROAD_WALK.** 01yellow-brick-road's 29-cluster road with its near end cleared: the walk went onto the dead tiles
  under us and the step-off rule sent us off again.
- **BAND_BANK_STICKY.** 04pinwheel: at the edge of BAND_BANK_RANGE the bank walk and the next rule undid each other.

## Benches (128 maps, tools/hc/bench.py)

| Bench | Record | Gems | K/D | Camp deaths |
| --- | --- | --- | --- | --- |
| v230 vs v220 (parent), both sides, release | 137-119 | +6.2% | 2331/1982 | 127 vs 143 |
| v230 vs live218 (platform 218), both sides, release | 133-123 | +11.3% | 2359/1983 | 114 vs 131 |
| v228 vs live218 (same switches, before the clean-up) | 131-125 | +9.6% | 2392/2029 | 129 vs 131 |

Unopposed (vs bots/idle, debug): 1,091 rounds to 1,000 gems (v220 1,129), 7 maps unfinished at round 2,000 (v220 10),
banked by r600 438 (416).

Fuel against v220: unopposed mean 40,541 a turn (40,367), p95 67,916 (68,391), out-of-fuel turns 0.41% (0.43%); in the
v220 head-to-head mean 39,154, p95 71,173, out-of-fuel 0.44%.

v230 against v228 (same opponent live218, same 256 games): 223 games identical in outcome, rounds and gems, 33 differ.
The two builds differ in code size (v228 carries the switched-off experiments), so fuel per turn and the out-of-fuel
turns differ (6,861 vs 7,163 for us in those games). I did not trace each of the 33 to its out-of-fuel turn.

Noise: one 256-game bench swings about ±4% between near-identical builds (two no-op variants scored 48.0% and 48.8%
against their parent). No single switch above is proven by wins alone; the K/D and gem deltas all point the same way.

## Platform test set (tools/hc/plat.py, results/plat)

20 opponents ranked 5-160 x 5 maps (13 cluster maps, our 20 worst ordinary maps, 17 others), 100 unranked games per
version. A hard set: the live bot went 23-77.

| Version | Record | Cluster maps | Other maps |
| --- | --- | --- | --- |
| base218 (live) | 23-77 | 8-18 | 15-59 |
| pd0 (v220 + TRIBUTE_STAY, 90 cells) | 19-71 | | |
| p223 (+ HOR_HOLD off) | 26-74 | 10-16 | 16-58 |
| ppw1 (+ DODGE_HIT, PRISM_WANT_RANGE 1) | 25-75 | 9-17 | 16-58 |
| p228 (v228) | 27-73 | 10-16 | 17-57 |
| p230 (this folder, platform version 230) | 26-74 | 9-17 | 17-57 |

Two builds that play the same on ordinary maps differed by 4 wins there (16-58 and 20-54), so a 100-game run cannot
show a +3 change in wins. Per-champion deaths and the gem curves (`tools/hc/platcurve.py`) are the steadier read.

## Tried on the way and dropped (code in the folders named, not here)

- **JOBS, everyone its own group** (bots/v221; five 6-bit claims in shared bytes 2, 3, 6 and byte 5): unopposed
  04gem-rush 2,000+ -> 666 rounds and -1,000 rounds on other cluster maps, but lone champions lose the 1v1s and 1v2s:
  mirror 109-147; platform cluster maps 2-24 against 8-18, 7.0 deaths by r600 against 2.7.
- **JOBS, only Vesper** (bots/v224): +4% gems unopposed; platform 24 vs 26 of 100, flips 8 up / 10 down.
- **JOBS on a leash around Prism** (bots/v226, v227): no faster unopposed than PRISM_WANT_RANGE 1 alone (882 vs 886
  rounds), 3.0 deaths by r600 on platform cluster maps against 1.8.
- **Cassi & Issac for Ryth** (bots/v222, DRAFT_SET 1): +9% camp damage by r100 and 135-121 in the mirror, but Cassi (600
  health) dies first in every pair death (280 of 280 in a 128-game mirror, from full health six turns earlier) and takes
  Issac with her. Platform 22-78 and 20-75. It needs a Cassi that stays out of reach and buys health first.
- **Greed emblems as the carries' fallback** (bots/x-greed): pace 1,137 vs 1,129 rounds, mirror 121-135.
- **FILL_SUSTAINED** (bots/v229: walk to far groups instead of waiting beside a respawn): waits 12.5% -> 11.0% of turns,
  walking 23.6% -> 26.5%, pace 1,096 vs 1,090 rounds.
- **Bank bar** GEM_BANK 25 / 60: mirror 125-131 / 135-121. **DODGE_STAND** 1 / 3: 119-137 / 127-129.
  **HOR_RECALL_HEALTH** 0.5: 130-126.
- **Relaxed camp caution** (unopposed): RECALL_HEALTH 0.25, BAND_HEALTH 0.45, CAMP_EXIT_ALWAYS off, V93_CAMP_EXIT off,
  TANK_SWAP off: all within 2% of 1,111 rounds. PT_GATE off: 1,088 rounds, untested in fights.

## Where the gap to better teams is

From 100 baseline platform games and 26 decoded replays (tools/hc/shares.py, platcurve.py, deathctx.py):
- In our losses on ordinary maps the opponents deal 9.3k camp damage by r100 to our 7.7k, and 60.9k to 43.8k by r600.
  Split: 12% more champion-turns (Cassi & Issac are two bodies), 31.5% of turns on a camp against our 28.3% in rounds
  1-300 and 25.5% against 19.6% in rounds 301-600, and the same damage per camp turn. Both sides walk about 45%.
- They are alone or in pairs 57% of the time (we 44%); we stand three or more together 55% of the time (they 42%).
- Own-half gem supply per map and round (ceiling): elk groups 0.72, wolf groups 0.62, pure gem clusters 0.34,
  monoliths 0.27. Unopposed we take about a quarter of it.
- 04gem-rush and 04gem-caverns (58 and 114 single clusters) cannot be won by one group: opponents farm them alone and
  finish in 600-750 rounds.

## Clean-up

Deleted 2026-11-10 on the user's say: the 30 scratch variants bots/x-* (switch sweeps of the folders above; they were
never committed, so the one-off patch scripts in tools/hc are what is left of them, e.g. `pgreed.py` for x-greed) and
replays/hc (6.6 GB of bench replays and statistics; `tools/hc/bench.py` regenerates any of them).

Still here: the steps bots/v221-v229. They hold the only copy of the JOBS and draft code (nothing is committed), so
they stay until that code is committed or nobody wants it.
