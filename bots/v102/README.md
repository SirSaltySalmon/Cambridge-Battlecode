# v102

Our `v101` (Willow combat and healing) merged into the friend's `v101-salmon` (Heart of Ruin), both forks of `v100`.
Not submitted.

## How it was merged
Three-way merge with `git merge-file`, base `v100/src`: `v101-salmon/src` + the `v100 -> v101` changes. The five files
both forks touched (`bot.h`, `constants.h`, `kits.cpp`, `route.cpp`, `turn.cpp`) merged without conflicts.
Checked line by line: the changes from `v101-salmon` to `v102` are exactly the `v100 -> v101` Willow changes, and the
changes from `v101` to `v102` are exactly the `v100 -> v101-salmon` Heart of Ruin changes; `hor.cpp` is untouched. Both
features keep their switches (`WILLOW_*`, `HEAL_MIN_SHARE` = 0.25 in `constants.h`; `HOR_*` for Heart of Ruin).

## What it plays
- Draft (from v101-salmon): 5 picks Varg, Ren, Heart of Ruin, Willow, Vesper; 3-4 picks Heart of Ruin replaces Varg.
- Willow (from v101): fights from the back, safe flee path, camp safety, no heals on recalling allies, heals only allies
  missing 25%+ of her heal, drops claims when an enemy champion is in sight. See `bots/v101/README.md`.
- Heart of Ruin (from v101-salmon, `hor.cpp`): dash-stun dive, held shield, stays in fights under the recall bar.

## First check
One game on 00eight vs v101-salmon: v102 won; Willow 0 deaths and 27,635 healing (v101-salmon's Willow: 12 and 8,059);
Heart of Ruin 0 deaths (theirs: 12).
