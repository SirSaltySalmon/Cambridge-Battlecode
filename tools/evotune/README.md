# evotune

SPSA tuning of the numbers in a bot's `evo_constants.h`, with a local dashboard.

```
python tools/evotune/evotune.py                     # dashboard at http://127.0.0.1:8765
python tools/evotune/evotune.py --inspect bots/v149 # what would be tuned
python tools/evotune/evotune.py --run v149-fight1 --base bots/v149   # start (or resume) without clicking
```

## The bot side
The bot keeps its tuned numbers in `src/evo_constants.h` (included from `constants.h`). Every
`inline constexpr int|long|double|float NAME = <number>;` there is tuned; bools and expressions are listed as fixed.
Optional comment on the line above (or trailing):

```cpp
// evo: delta=2 min=0 max=20
inline constexpr int FIGHT_HYSTERESIS = 10;
// evo: off
inline constexpr int MIN_HOLD = 3;
```

Without one: delta = 20% of the value (ints at least 1), range value/4 .. value*4 keeping the sign (at least 4
deltas wide). `bots/v148-evo` is an EVO COPY of v148-normal with ten fight-score numbers moved out, for testing.

## What one iteration does
1. Every parameter moves `c` deltas up or down at random (theta+) and the mirror way (theta-). `c` starts at 1 delta
   and decays slowly (k^-0.101, never under 0.5).
2. Both play the center (the last best, built as an evo copy) and the base bot, on a random `map_fraction` of the
   maps, both sides, in one `cambc run-batch --release --log-limit 0` (same maps for both, so the pair is matched).
3. f = (w_best * score vs center + w_base * score vs base) / (w_best + w_base), score = (wins + draws/2) / games
   (equal banked gems = draw, as in bench.py).
4. Each parameter moves a_k * (f+ - f-) / (its realised theta+/theta- gap in deltas), clipped to `max_step` deltas.
   a_k = lr / (k + 11)^0.602.

Every `checkpoint_every` iterations the center plays the base on all maps, both sides, for `checkpoint_seeds` copies
(seed s burns 300*s fuel at turn end, as bench.py seed). That is the progress line, the fuel line and the export
list. While the center equals the base, "vs center" and "vs base" are the same games and are played once.

## Files
`evo_runs/<run>/` (git-ignored): `config.json` (settings + parameter table), `state.json` (iteration, center, the
current iteration's plan), `journal.jsonl` (one line per iteration / checkpoint), `log.txt`,
`games/<batch>/results.jsonl` (one line per finished game; raw replays and stats are deleted once read),
`b/evo-*` (the evo copies; theta copies are deleted after their iteration).

Pause finishes the current iteration. Stop (or Ctrl+C) kills the batch tree; a resume replays the same plan and only
the games without a result. A resume may change jobs, checkpoint settings and max iterations, not the objective.

## Export
The dashboard shows the evo_constants.h text for the current center or any checkpoint (copy it), or creates
`bots/<name>` = base bot + those values. A checkpoint's score is one noisy sample: bench the export as usual.
