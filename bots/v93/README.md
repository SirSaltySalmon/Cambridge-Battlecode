# v93

`v92` + `V93_CAMP_EXIT`: a champion leaves a camp's reach once its health would not outlast the walk out, not only
under the fixed recall bar.

## What was killing Cassi "at a standoff"

Cassi never took those hits. Traced turn by turn (ambush r128-143), she sat at full health 4 tiles from the elks.
Issac, tanking both elks from 1 tile, crossed the 35% recall bar, and a camp at range 3 takes three steps to leave
while two awake elks hit about 120 a round. He died on the way out, and the Cassi and Issac pick dies together, so the
engine credited her death to the same camp. All 46 Cassi deaths to camps in the v90-log, v91 and v92 benches were an
Issac death in the same round. More broadly, 48 of 66 melee deaths to camps came on a `hurt.dodge` turn (the commonest
run: `farm.camp` x3, `hurt.dodge`, dead): the fixed bar starts the exit too late next to heavy camps. A Willow heal that
lifts a tank just over the bar, so it walks back in, made it worse. Camps never moved and never hit past 3 in the trace.

## Fix

- `camp_exit_cost()` (`state.cpp`): over the awake camps (hurt, or awake by their group) whose reach covers us and that
  have no visible ally nearer than us (a camp hits the nearest champion), their strength summed times the steps to leave
  the furthest reach. Step 3 of the turn (`turn.cpp`) treats health at or under it as hurt: recall where safe, else step
  out first (`hurt.dodge`, logged "too low to walk out of the awake camps' reach later"). `V86_FINISH` can still keep a
  champion on a camp it can finish safely.
- The flag stays on (`exit_hurt`) until spawn or `BAND_HEALTH` (60%), so the farm step does not walk straight back in.
- The camp scan runs only under `BAND_HEALTH`; above it the walk out is always affordable. Ungated it cost +3.6% fuel a
  turn, gated +2.2% (likely partly more turns alive).

## Results (34 games vs v92, normal fuel; `analyze_finish.py`, `analyze_errors.py`)

| | record | gems | deaths to camps (Cassi / Issac) | all deaths | camp groups cleared | gems from camps | mean fuel | out-of-fuel turns |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| v93 | 19-15 | 27,864 | 16 (5 / 5) | 429 | 978 | 29,024 | 28.7k | 162 |
| v92 | 15-19 | 25,300 | 32 (11 / 11) | 477 | 941 | 27,586 | 28.0k | 160 |

- Deaths to camps halve; all deaths -10%; gems +10%, clears +4%. The rule fired on 64 turns over 17 games.
- Ungated run (bench12): deaths to camps 15 vs 37, 18-16, gems +7%; same picture.
- Result file: `results-v93-vs-v92.txt` (repo root).