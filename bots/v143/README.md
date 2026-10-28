# v143

v143 = v142 + WILLOW_REGROUP (user), in `src/constants.h` after HOLD_LIMIT. Benches: all 70 maps, both sides, release,
seed 0 + fuel-burn s1 / s2 (`bots/v133t/bench.py`), vs v142.

## What it does
After HOLD_LIMIT (30) rounds holding beside a camp with no tank, Willow no longer opens it herself (FIX_STUCK_HOLD still
does for the others). She goes to the freshest ally sighting (within REGROUP_SEEN 15 rounds, away from her), else to
our spawn (walk or safe recall, `recall_to_shop`), and waits there; over once an ally is within TEAM_RANGE, or after
REGROUP_MAX 60 rounds. Shared memory holds no ally positions (target, tank, capture claim, a 2-round fight ping that
reaches 10-12 tiles), so sightings and spawn are all she has.

## Results (vs v142)
420 games 201-201-18, banked -0.6% (level). Debug pass: fired 21 times in 140 games, 20 ended meeting an ally; no loop.
