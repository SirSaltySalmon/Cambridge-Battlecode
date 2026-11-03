# v151 (folder v151-which-is-v149tuned)

v151 = v149-cap (v148-normal + the capped emblem-ratio fight-score term, below) with the user's tools/evotune numbers
in `src/evo_constants.h`. Platform v151, the ladder bot until v152. bots/v151-mapfix (platform v154) and the shop
builds in bots/v155 were built and benched on it.

| Constant | v149-cap | v151 (evotune) |
|---|---|---|
| FIGHT_THRESHOLD | 5 | 2 |
| FIGHT_HYSTERESIS | 10 | 13 |
| MIN_HOLD | 3 | 1 |
| MISSING_BODY_PENALTY | 5 | 3 |
| EMBLEM_BONUS | 3.0 | 3.74153 |
| DISTANCE_PENALTY | 0.5 | 0.454557 |
| OVEREXTEND_PENALTY | 10 | 9 |
| EMBLEM_RATIO_WEIGHT | 10.0 | 14.0155 |
| EMBLEM_RATIO_SMOOTH | 1.0 | 1.0507 |
| EMBLEM_RATIO_CAP | 20 | 24 |

Fight-score numbers (with the emblem-ratio ones) live in `src/evo_constants.h`, included from `src/constants.h`;
`// evo:` comments give tools/evotune each one's step and range. On v152's base these numbers did not help (bots/v155's
README, v152-evo and v152-evof).

## Test folders removed in final-stretches

These folders were local test variants that never shipped. Their READMEs are kept below; where they say `bots/<folder>/README.md` for a removed folder, the text is in this section. The code is in git history (vargvale-strikes-back, commit a31dc30 and before).

### bots/v149-cap

v149-cap = v149-ratio with the fight score's emblem term `min(3 x ours, 20) + 10 x ln((ours + 1) / (theirs + 1))` (EMBLEM_RATIO_ADD on, EMBLEM_RATIO_CAP 20).
Code, results and the log analysis: `bots/v149-ratio/README.md`; results file `results/results-v149-cap-vs-v148-normal.txt`.

Fight-score numbers (with the emblem-ratio ones) live in `src/evo_constants.h`, included from `src/constants.h` with the same values; `// evo:` comments give tools/evotune each one's step and range.

### bots/v149-ratio

v149-ratio = v148-normal + EMBLEM_RATIO (user): the fight score's emblem term compares the two sides instead of counting ours.
Switches in `src/constants.h` next to EMBLEM_RELATIVE; the term is in `score_mode` (`src/route.cpp`).

#### What changed
| | Emblem term |
|---|---|
| v148-normal | `3 x our fighters' emblems` (the enemy's are not counted) |
| v149-ratio | `10 x ln((ours + 1) / (theirs + 1))`, both sides weighted like the bodies |

The idea: one emblem of difference matters less as both sides build. Examples: 1v0 +7, 4v2 +5, 10v5 +6, 10v9 +1,
0v3 -14, level 0. v148-normal gives level sides with 10 emblems each +30.

#### Result (all 70 maps both sides, release, seed 0, 140 games): rejected
| | v149-ratio | v148-normal | |
|---|---|---|---|
| Record | 50-86-4 | | |
| Banked gems | 80,465 | 105,067 | -23.4% |
| Gems collected | 107,669 | 130,298 | -17.4% |
| Gold earned | 827,943 | 1,026,517 | -19.3% |
| Kills / deaths | 1682 / 2512 | 2414 / 1768 | |
| Camp deaths | 98 | 86 | |
| Fuel per turn (mean) | 33,795 | 35,420 | -4.6% |
| Out-of-fuel turns | 2328 | 2135 | |

Not close, so the extra fuel-burn seeds were not run.

#### Follow-up: keep the flat term (v149-add, v149-cap)
Same code, switches EMBLEM_RATIO_ADD / EMBLEM_RATIO_CAP: v149-add = `3 x ours + ratio`, v149-cap = `min(3 x ours, 20) + ratio`.
All 70 maps both sides, release, 3 seeds (0 plus fuel-burn copies s1, s2), 420 games each, vs v148-normal:

| | Record | Banked | Collected | Gold earned | Per seed (banked) | K / D (ours vs theirs) |
|---|---|---|---|---|---|---|
| v149-add | 182-224-14 | -8.8% | -5.1% | -2.8% | -3.5 / -12.6 / -10.0 | 6325/7083 vs 6839/6610 |
| v149-cap | 186-219-15 | -7.1% | -4.4% | -2.6% | -4.4 / -11.0 / -5.8 | 6085/6899 vs 6644/6368 |

Fuel per turn (seed 0; s1/s2 burn fuel on purpose): -0.4% and -0.5%. Both rejected; every seed of both is negative.

#### Why (debug decision logs, 140 games per bot, `tools/v149/fights149.py`, `flips149.py`)
- A retreat call is the worst thing the team does: in the 10 rounds after one, 0.03-0.05 kills and 0.43-0.45 deaths
  per call, for every bot (v148-normal too). A fight call gives 0.41-0.44 kills and 0.25-0.30 deaths.
- The ratio is negative whenever the near enemies own more emblems, so it pushes fights near the bar into retreats:
  ratio-caused retreats 989 (v149-ratio), 1366 (add), 1686 (cap); after them 0.04-0.06 kills, 0.33-0.46 deaths per call.
  The few fights it caused were fine (v149-ratio: 951 at 0.31 / 0.20) but cannot pay for that.
- Retreat rounds: v148-normal ~17.4k per 140 games, v149-add 22.7k, v149-cap 24.5k, v149-ratio 29.8k. The order matches the
  results. Retreating from a stronger team does not save the champion: it is chased down without trading back.

So the emblem lead was never the problem; retreating is. Next lever: retreat behaviour itself (fight on when the
enemy can catch us anyway, or retreat earlier), not the fight score's emblem term.
Results: `results/results-v149-add-vs-v148-normal.txt`, `results/results-v149-cap-vs-v148-normal.txt`; replays
`replays/b149b*`, debug `replays/b149dbg`. Same failure as EMBLEM_RELATIVE in v115 (34-46): with the flat term
gone, even fights score near the bar, the team breaks off more and gets run down (deaths +42%, kills -30%).
Replays: `replays/b149ratio/v149`; summary in `results/results-v149-ratio-vs-v148-normal.txt`.

Fight-score numbers (with the emblem-ratio ones) live in `src/evo_constants.h`, included from `src/constants.h` with the same values; `// evo:` comments give tools/evotune each one's step and range.
