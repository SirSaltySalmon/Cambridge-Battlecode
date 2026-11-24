# v219-ward (v213 + ward cleanup + SCOUT_MASK=2 + catch-up hold)

The kept results of the ward experiments (WARD_PLAN.md, 2026-11-03/04), combined. Each part was benched on its own
against v213 + the log-only probe (bots/zz-wardprobe; replays/bwardbase-*), all 70 maps on both sides. **The
combination has not been benched** (the user stopped bench time).

| Part | From | Result alone |
|---|---|---|
| Dead fixed-post ward code removed (posts, key-bush ward, detour, ward-spot draft bytes, `WARD_MASK_BYTE` writes to the ping byte) | bots/v214-ward-cleanup | 140/140 games identical to v213 at unlimited fuel; per-turn fuel level; first turn -13k, draft -40k |
| `SCOUT_MASK=2`: refused scout tiles (allies' hidden wards) skipped for 20 rounds, one same-turn retry, altars/enemy spawn masked | bots/v214-ward-mask | 670-590 (53.2%) vs 49.5%, 1,260 games; fuel level |
| `WARD_HOLD` (catch-up only, 10 rounds, range 10): no catch-up ward while a fight is likely | bots/v214-ward-hold (hb10) | 464-370-6 (55.2%) vs 50.1%, 840 games (v213+v210); +6.4% out-of-fuel turns |

Dropped:
- `SCOUT_REFRESH` 16: 52.5% alone, but +15.6% out-of-fuel turns, and little added on top of the mask (53.0% vs 52.6%,
  700 games).
- Catch-up wards off: 44.7%. The ward is also how a champion sees its allies.
- Holding bush wards during fights: 46-47% vs 48.1%.

Why the two flags should combine: the hold keeps the charge for the ghost and hidden-attacker wards and raised scout-bush
refusals; the mask removes repeated refusals. They act on different branches of `scout_ward`. Untested risk: the hold
results come from a build without the mask, and wins don't always add up (memory `small-map-plan`).

Code: `scout_ward` in src/warding.cpp holds both. The logic matches the benched builds with the probe lines left out.
Flags and results are in src/constants.h after `SCOUT_REFRESH`. To fall back: `WARD_HOLD = false` gives cleanup + mask
(the best-measured part); `SCOUT_MASK = 0` as well gives v213 play.
