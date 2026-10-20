# v91

`v90-log` + mages never tank a camp, and a hurt tank no longer counts as one.

## The problem (ambush, v90-log as team A, rounds 127-142)

Issac tanked the camp at (30,44) while Cassi, Stellar and Willow hit it from 3 tiles (inside its reach of 3). At r133 Issac
fell under the recall bar, stepped out and recalled. Cassi and Stellar kept basic attacking from inside the reach for
about ten rounds, taking the camp's hits, until Ryth arrived; Willow hit on for a round too. Two rules caused it:

- `V86_MAGE_RULE` sends a mage in the safe zone that is healthy or has an ally near down the ordinary ranged path ("and
  tank them when nobody else does"). That path holds back only while a melee is in sight, so once the tank was gone the
  mages opened up from inside the reach instead of standing off (`mage_camp`).
- `camp_tanked()` and `coming_to_camp()` counted a tank under the recall bar that had not yet stepped away.

## Fix

- `V91_MAGE_NO_TANK` (`mage_cheese()`): the v86 rule applies only while a sturdy ally tanks the camp (`camp_tanked()`).
  With nobody tanking, the mage stands off: it leaves the reach, casts from 4 and waits out cooldowns there.
  `mage_camp` still basic attacks when the finish is cheap (at most `MAGE_FINISH_SHARE` = 10% of its health), so a
  little tanking is still accepted.
- `V91_TANK_HURT`: `camp_tanked()` and `coming_to_camp()` skip an ally under `RECALL_HEALTH`. This also makes Willow and
  other ranged hold back the turn the tank starts leaving.
- Same scene in v91: at r133 Cassi and Stellar step out to the cast ring and cast from 4 (`farm.camp.leave`, then
  `.standoff` / `.cooldown`); Willow holds; all go back to hitting once a tank arrives.

## Results (34 games vs v90-log, normal fuel; `analyze_squishy.py`, `analyze_errors.py`)

| | record | gems | caster camp damage taken / game | Cassi | Stellar | Willow | caster deaths | caster camp damage done / game | mean fuel |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| v91 | 17-17 | 25,624 | 1,091 | 307 | 338 | 583 | 239 | 32,224 | 28.6k |
| v90-log | 17-17 | 25,968 | 1,479 | 533 | 462 | 632 | 243 | 31,536 | 28.3k |

- Casters take 26% less camp damage (Cassi -42%, Stellar -27%, Willow -8%) and deal 2% more camp damage; the team's
  total camp damage taken is 3% lower.
- Record, gems (-1.3%) and deaths are within noise (+-2 games, ~18% gems per 34 games). Most caster deaths come from
  fights, which this does not touch.
- Fuel: +1% a turn (one more `camp_tanked()` read per mage camp turn); out-of-fuel turns unchanged (175 vs 173).
- Result file: `results-v91-vs-v90-log.txt` (repo root).