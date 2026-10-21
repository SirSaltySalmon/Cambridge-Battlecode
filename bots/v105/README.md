# v105

`v104` + leave awake camps first when reacting to an enemy (`CAMP_EXIT_REACT`, `camp_heat` / `camp_exit` in
`combat.cpp`). Strong-camp early pricing (`STRONG_EARLY`) is written but off: it lost in every bench. Not submitted.

## Changes
1. `CAMP_EXIT_REACT`: a champion reacting to an enemy (fight, retreat, hurt dodge, ping retreat) while an awake camp
   hits it (it is the nearest champion in that camp's reach, ours or theirs) leaves the camps first. Three tests per
   tile: out of every awake camp's reach, farther from every near enemy than now, at most one step slower than the
   quickest exit. It walks to the nearest tile passing all three, else out + away, else out + fast, else away + fast.
   Melee hit back on the way; a channelling enemy in reach is still shot (its bag); Willow keeps her backline.
   Not hit, a fighter closes on the enemy around awake camps' reach when the target can be hit from outside it.
   Fuel: a camp's kind and spawn tile are client-side, so the scan is one raw state read per near camp; the exit is a
   few `bitboard::reachable_tiles_into` calls and board ops, no guest loop over tiles.
2. `STRONG_EARLY` (off): elks/golems' health cost in route values x(1 + `STRONG_EARLY_EXTRA`) at no emblems, fading to
   x1 at `STRONG_FADE_EMBLEMS` emblems per champion (`Bot::reprice`, `draft::ValueParts`). Off = v104's values exactly.
3. `fight()`: a melee's pre-exit swing can kill the target; with no exit step it no longer reads the dead target.

## Bench (release, 35 maps, both sides, one seed, vs v104; `replays/b105c`, `b105d`, `b105e`)
| variant | record | banked | gold | kills | deaths | banked r300 / r600 / r900 | out of fuel (v104 407) |
|---|---|---|---|---|---|---|---|
| exit only (= v105) | 37-33 | +8.0% | +0.2% | -0.7% | -3.0% | +26.5% / +16.4% / +18.7% | 429 |
| exit + price x4 | 31-39 | -3.6% | -5.1% | -8.3% | +1.0% | +11.4% / +2.9% / +4.3% | 601 |
| price x4 only | 30-40 | -6.7% | -3.9% | -5.8% | +4.6% | -4.1% / -8.8% / -6.0% | 478 |
| exit + price x2 | 33-37 | -2.0% | -2.6% | -0.8% | -2.9% | +17.9% / +14.8% / +5.6% | 457 |

Exit + price x4 was also 29-41 vs v103 (v104: 35-35). Mean fuel ~26.4k a turn (v104 25.7k).

## Hooks
Still side-decided (team A wins 1012-0 whichever bot is B). Team B wakes its elk at (15,16) around round 14, team A
arrives a round later, and the opening fight happens inside B's awake elk reach while A's camps sleep; A snowballs
(16k gold vs 584). The exit fires ~200 times a game there but cannot undo that. Every hooks gem group holds an elk, so
pricing cannot change the target either. Next: do not open a strong camp when enemies are expected within its clear
time.
