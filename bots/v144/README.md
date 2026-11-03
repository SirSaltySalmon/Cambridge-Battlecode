# v144

This folder holds the shipped v144 configuration (benched as v144v, submitted as platform version 143): CONTEST_VALUE
on, MIDDLE_FIRST off, CONTEST_ANYWHERE off. The other variants below were removed.

v144 = v143 + the middle switches (user), in `src/constants.h` after WILLOW_REGROUP. Benches vs v143 as for v143.

## The problem
Ladder, 70 ranked games (35-35), gems captured per game by zone: wins took 110 middle gems to 64 before r300, losses 54
to 98, and losses farmed home MORE (69 vs 58). Enemies take ~1-21 of our side's gems a game. Until `aggressive` (Vesper
core + 4 team emblems + all alive) best_goal aims only at the altar pocket; any death switches it off again.

## Switches
- MIDDLE_FIRST: centre pocket (cores[1]) from round 0.
- CONTEST_VALUE: value x (1 + CONTEST_DENY x contest), contest 1 at even walking steps from both spawns (draft's home /
  enemy steps), 0 at CONTEST_WIDTH (0.3) off even. Setup only, no turn fuel.
- CONTEST_ANYWHERE: before aggressive, best_goal rates every group (no pocket).

## Results (vs v143, 420 games each)
| Variant | Record | Banked | Per seed | middle gems before r300 (ours/theirs) |
|---|---|---|---|---|
| middle-first only | 190-219-11 | -4.7% | -0.2 / -10.0 / -3.6 | 89/81 (K/D down, camp deaths 255 vs 206) |
| + contest | 206-201-13 | +0.3% | +0.6 / -2.0 / +2.3 | 92/77 |
| + contest + anywhere | 214-188-18 | +2.8% | -1.7 / +4.3 / +5.9 | 93/82 |
| **contest only (v144v)** | **217-188-15** | **+4.4%** | +3.1 / +4.3 / +5.8 | 93/81 |
v143 mirror: 85/85. v144v fuel +0.8% a turn; 01boss-battle seat-B trap worse (4159 vs 1656 out-of-fuel turns).
v144v was submitted (platform v143). Unranked vs the 10 teams we lost to most: 24-26, 16 of 32 same-map losses flipped;
losses now leak carried gems (lose 194 a game on deaths vs their 62).

## Seed copies (removed in final-stretches)

`bots/v144-s1` and `bots/v144-s2` were fuel-burn seed copies: the same code with `{ volatile int sink = 0; for (int i = 0; i < 300; ++i) sink += i; }` before `cambc::end_turn()` in `src/main.cpp`, so fuel timing (and every timing-dependent outcome) reshuffles. Benches pool seed 0 (this folder) with the two copies; recreate them by adding that line.
