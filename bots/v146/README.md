# v146

v146 = v145 + HOR_DIVE_FOLLOW, in `src/constants.h` after WAIT_OUTSIDE (VESPER_LEASH and VESPER_ESCAPE_ALLY built, off).
Benches: all 70 maps, both sides, release, seed 0 + fuel-burn s1 / s2, vs v145.

## The problem (ladder replays)
100 unranked games of v144v / v145 against the 10 teams we lose to most, decoded per round from the replays (engine
WASM + the visualiser's decoder, see memory replay-decoding): in losses, fights lost at even bodies 46%, half our deaths
with no ally within 4, HoR 6.1-6.4 deaths a game (half alone), Vesper 3.8-3.9 (0.7-0.8 in wins). the-line r34: HoR's
dash stunned three of them; nobody followed and he died; their pack ran the rest of us down.

## What it does
- HOR_DIVE_FOLLOW: a dive landing counts only with HOR_FOLLOW_MIN (2, fewer if fewer are alive) visible allies within
  HOR_FOLLOW_RANGE (3) of it.
- VESPER_LEASH (off): with a melee within 2 and no sturdy ally within 3, Vesper goes to the nearest sturdy ally.
- VESPER_ESCAPE_ALLY (off): her hurt escape leans toward that ally rather than our spawn.

## Results (vs v145, 420 games each)
| Variant | Record | Banked | Per seed | K/D ours vs theirs | camp deaths |
|---|---|---|---|---|---|
| all three | 196-210-14 | -3.3% | -8.8 / -3.1 / +2.2 | 6197/6862 vs 6573/6478 | 289 vs 281 |
| **HOR_DIVE_FOLLOW only (v146)** | **219-189-12** | **+5.3%** | +6.5 / +2.1 / +7.3 | 6563/6578 vs 6308/6843 | 270 vs 280 |
| Vesper switches only | 210-197-13 | +1.2% | -2.4 / +4.2 / +1.9 | 6333/7158 vs 6860/6572 | 298 vs 239 |
HoR gate: Vesper deaths 928 vs 1044 (-11%), HoR 2099 vs 2156. The leash raised every champion's deaths (Vesper 1104 vs
997): walking her to an ally drags the fight onto the team. Fuel (HoR gate, all but 01boss-battle): +0.4% a turn,
out-of-fuel 7566 vs 7534.
