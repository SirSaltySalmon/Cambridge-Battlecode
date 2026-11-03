# v202 (v170 + farming changes from the Fish replays, + pre-armed recall reading)

Built from the 2026-11-03 study of 25 Intercontinental Fish ladder replays against v170 on the same maps; the
study and every test result are in `results/fish-farming-study.txt`. Every change sits behind a flag in
`src/constants.h` (search "v202"); with all of them off the bot plays exactly as v170. The Prism team (gem-cluster
maps) is unchanged. Not uploaded and not committed.

## Changes

- **CAPTURE_HYGIENE** (capture time leaks): whoever stands on a cleared reward keeps it (CAPTURE_KEEP); the champion
  that cleared a group walks onto its reward even while another claim is open (CAPTURE_CLEARER); cleared or claimed
  targets are never held, and a reward an ally stands on is no longer loot, goal or claim (CAPTURE_MOVE_FAST); shop
  and hurt recalls start on the reward once its timer fits inside the recall (CAPTURE_RECALL).
- **PT_GATE** (when a camp is allowed): elk, golem and monolith groups open by power-to-threat (the present squad's
  camp damage and health against the camp's expected damage) instead of emblem counts. Budgets: elk 1.0, golem 0.75,
  keep 1.5, monolith 1.5 (the old emblem bar stays as a second way in). PT_COMMIT finishes started groups (dead-camp
  detection, an engaged bit in the stamp byte); PT_SCAR remembers damaged groups (camps never heal).
- **SHOP_CADENCE** (items sooner): shop trips on income (the gold that finishes the current emblem, 90-150) without
  waiting for a tribute or an empty bag, preferably when cheap (under 65% health, just after a capture, or a free
  detour); a collector can recall on the reward tile so the reward lands during the channel; Vesper buys one emblem
  per trip and opens with Capture; gold-only groups count as normal loot and fillers while the team has under 2
  emblems per champion (EARLY_GOLD).
- **PREARM_READ** (combat reading): top teams start a recall at the end of nearly every turn and cancel it at their
  next turn start, and v170's fight score counted them as 0. Once an enemy shows a recall at the same timer on
  consecutive rounds (a re-arm), first-tick recalls of that team stop counting as real. No change against teams that
  do not pre-arm.
- **Merge fixes** (from a 3-reviewer check of the combined code): recalls from a reward keep the arrival
  bookkeeping, so the team does not chase a camp that is already respawning; keepers are not pulled off their reward
  by claims or camp exits; the killing hit no longer holds a cleared target one more round; income trips do not
  interrupt a committed clear; a cheaper dead-camp count; the tank-swap step re-reads its camps after moving (v170
  threw "Unit is not in vision" there and lost the turn's hit).

## Results (local, vs v170, 86 platform maps x 2 sides x 3 timing seeds = 516 games)

280-236 (54.3%), banked +4.1%. Per game: kills/deaths 18.8/15.4 vs 15.0/19.2; captures 53.2 vs 45.6 (11.2 vs 9.7
by r300); camp gems 744 vs 696; gold 7629 vs 6664; team emblems by r400 8.6 vs 7.2; tributes 15.8 vs 12.5. Fuel per
turn +4.0% (partly more turns alive), turns at the cap even, 0 faults. Pooled local noise is about +-3.5% banked, and
local results have overstated the ladder before: judge it on top-50 ladder games.

## Tested and left out

- **BUSY_JOBS** (Fish-style splitting: spare champions farm their own camp): 73-99, -23.8% in its Fish-like form,
  158-186, -7.0% with safety gates. Split champions were picked off by raids.
- **Pre-arming our own recalls**: a v170 that only pre-armed was even with v170 (+0.3%).
