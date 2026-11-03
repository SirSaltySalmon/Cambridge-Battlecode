# v149-cap

v149-cap = v149-ratio with the fight score's emblem term `min(3 x ours, 20) + 10 x ln((ours + 1) / (theirs + 1))` (EMBLEM_RATIO_ADD on, EMBLEM_RATIO_CAP 20).
Code, results and the log analysis: `bots/v149-ratio/README.md`; results file `results/results-v149-cap-vs-v148-normal.txt`.

Fight-score numbers (with the emblem-ratio ones) live in `src/evo_constants.h`, included from `src/constants.h` with the same values; `// evo:` comments give tools/evotune each one's step and range.
