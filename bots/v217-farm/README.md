# v217-farm (v216-focus-v + Nic's v202 farming switches)

Nic's farming logic from bots/v202 (built on v170; study in `results/fish-farming-study.txt`) moved onto
bots/v216-focus-v. Only the farming logic was taken; wherever the two disagree, v216-focus-v's logic stays the base.
Made by a 3-way merge per file (base bots/v170, ours v216-focus-v, theirs v202), conflicts resolved by hand.

## Switches (src/constants.h)

| Switch | What it does |
| --- | --- |
| CAPTURE_HYGIENE (KEEP, CLEARER, MOVE_FAST, RECALL) | whoever stands on a cleared reward keeps it; the clearer walks onto its reward; cleared/claimed groups are not held as the team target; recalls start on the reward once the capture timer fits inside the recall |
| PT_GATE, PT_GATE_MONO, PT_COMMIT, PT_SCAR | elk/golem groups open by power-to-threat (expected camp damage vs the present squad's health: elk 1.0, golem 0.75, keep 1.5); monolith model as a second way in; started groups are finished; damaged groups remembered |
| SHOP_CADENCE (SHOP_INCOME, SHOP_CAPTURE_RECALL, VESPER_EACH_EMBLEM, EARLY_GOLD, CAPTURE_FIRST) | shop trips on income (the gold that finishes the current emblem, 90-150) without a tribute first; recall from a reward; Vesper one emblem a trip, Capture first; gold-only groups walked to further while under 2 emblems a champion |
| PREARM_READ | an enemy recall counts only once it has survived one of its owner's turns (top teams pre-arm a recall every turn) |
| TANK_STEP_FIX (new, **off**) | Nic's non-farming fix: re-read the camps after TANK_SWAP's step. Off, so the base stays v216 |

## Where v216 was kept over v202

- Stamp byte: v216's FOCUS_KEEP uses bit 7, which PT_COMMIT also wanted. With PT_COMMIT on, the round stamp is 3 bits
  (round % 7 + 1) and ENGAGED_FLAG is bit 3; off, the byte is v216's exactly.
- Monoliths: v216's gates (MONO_SESSIONS, MONO_EARLY, MONOLITH_GATHER) stay; PT_GATE_MONO's model (`mono_ready`) is
  only a second way in. v216's MONO_SCALE clear time stays for monoliths; PT_SCAR's for other damaged groups.
- Group setup keeps v216's draft-kind counts; the PT threat lists are an extra pass only with PT on.
- EARLY_GOLD prices with v216's ECON_PHASES / CLOSE_OUT (not v170's value_of), and its distances start from v216's
  `gold_only_near()`. ECON_PHASES already gives gold-only groups full value before 2 emblems a champion, so only
  EARLY_GOLD's walking distances are new.
- CAPTURE_FIRST: Vesper's build is v216's list with Capture moved first.

## Exactness

`bots/v217-off` (every switch off) = v216-focus-v: 16 games vs v213 at `--fuel-quantum 100000000`
(00ambush, 00collision, 00monolith, 00packs, 01gold-rush, 02treasure-box, 03showdown, adgato-gemcombat3, both sides),
statistics files byte-identical. The first try differed until TANK_STEP_FIX was gated.

## Variants

`mkvar.sh NAME HYG PT SHOP PREARM` (in this folder) copies src/ into bots/NAME with the switches set:
v217-off (all off), v217-hyg, v217-pt (all four PT flags), v217-shop, v217-prearm, and v217-farm itself (all on).

## Bench

Each variant vs v216-focus-v, all 86 maps x 2 sides (172 games), release, seed 0, `--jobs 2` (2026-11-04).
One 172-game run is about +-7 wins of noise; confirm anything promising on a second seed / the pool.

| Variant | Record | Banked (ours vs focus-v) | K/D | Fuel mean (focus-v pool s0: 37272) | Out-of-fuel turns |
| --- | --- | --- | --- | --- | --- |
| v217-hyg (CAPTURE_HYGIENE) | 78-94 | 115676 vs 129392 (-10.6%) | 1469/1923 | 37104 (-0.4%) | 0.40% |
| v217-pt (PT_*) | 94-78 | 123387 vs 114033 (+8.2%) | 1674/1817 | 37598 (+0.9%) | 0.41% |
| v217-shop (SHOP_CADENCE) | 96-76 | 132813 vs 114116 (+16.4%) | 1702/1516 | 37388 (+0.3%) | 0.35% |
| v217-prearm (PREARM_READ) | 85-87 | 118518 vs 124002 (-4.4%) | 1520/1713 | 37418 (+0.4%) | 0.37% |
| v217-farm (all four) | 93-79 | 128020 vs 116386 (+10.0%) | 1565/1607 | 37813 (+1.5%) | 0.39% |

Read: SHOP_CADENCE is the clear one (+10 wins over even, fewer deaths). PT_* leans positive. CAPTURE_HYGIENE loses
(deaths +25% vs focus-v's). PREARM_READ is neutral as expected (focus-v does not pre-arm, so it only moves fuel
timing locally; it matters against pre-arming ladder teams). All four together is below SHOP alone (hygiene drags).
Fuel is compared with focus-v's pool seed-0 mean (different opponents), so treat it as approximate.
Next: SHOP + PT combined, second seed, then the pool.

## Round 2 (bots/v217-sp = SHOP_CADENCE + PT_*, no hygiene; -s1 = fuel seed 1)

| Run | Record | Banked |
| --- | --- | --- |
| v217-shop-s1 vs focus-v | 83-89 | -2.2% (SHOP alone's +16.4% did not hold) |
| v217-sp vs focus-v, s0 | 96-76 | +13.5% |
| v217-sp-s1 vs focus-v | 106-66 | +28.1% |
| v217-sp pool s0 (v210/v206/v208/v185) | 404-284 (v210 106, v206 98, v208 102, v185 98) | 536706 vs 430869; focus-v pool s0 was 365-323, 503847 |

Pool vs focus-v: +39 wins, our banked +6.5%, K/D 7657/5658 (focus-v 7553/6803). Fuel mean 38053 (+2.1% vs 37272),
out-of-fuel 0.41% (0.37%). v217-sp is the candidate; next: pool seed 1, then ladder.
