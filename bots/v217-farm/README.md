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

Each variant vs v216-focus-v, all 86 maps x 2 sides (172 games), release, seed 0: results to be added.
One 172-game run is about +-7 wins of noise; confirm anything promising on a second seed / the pool.
