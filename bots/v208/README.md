# v208 (v206 + Mike's v185)

A three-way merge over their common parent v182 (the same source in both lineages):

- **v206** (ours): v182 -> v183 PRISM_SAT -> v202 SPREAD (off) -> v204/v205 OUTCLASS (gap -5) -> v206 PRISM_TEAM,
  STAY_FIX (off), BUILD_SET (0).
- **v185** (Mike, git ae3604b on vargvale-strikes-back, platform v208): v182 + KILL_TIME off, v170's Prism cash-in
  (PRISM_STRICT, PRISM_CASH_ANY_MODE off) and economy (SHOP_NO_TRIBUTE, SHOP_EMPTY_BAG, EARLY_BANK_GEMS 0,
  BANK_ON_WAY off, GOLD_ONLY_NEAR 2), FOCUS, JOIN_GAP, DRAFT_MAP. See v185's notes in `src/constants.h`.

v206's switch defaults are kept; v185's are taken as Mike set them.

## Conflicts

- `constants.h`: v185 sets PRISM_STRICT false, next to v206's new PRISM_TEAM_CAP and PRISM_HUNTED_RANGE (both kept;
  they only act on the Prism team's hold).
- `route.cpp` setup: v185 moved the chase ring into the draft (`draft::chase_ring`, DRAFT_MAP); v206 had put
  `spread_setup()` inside the old ring code. It now runs after the ring (it never reads `spawn_zone`).

Shared memory: FOCUS takes the stamp byte's high nibble; nothing on v206's side reads the raw stamp byte (all through
`shared.stamp`, which is masked). Draft memory: v206 adds nothing, so DRAFT_MAP's block fits as in v185.

## Exactness (unlimited fuel, vs v183, 8 maps incl. adgato-gemcombat4/5, both sides)

- v208 with v185's switches set back to v182's values = v206: 16/16 games identical.
- v208 with OUTCLASS and PRISM_SAT off = v185: 16/16 games identical.

## Local bench (2026-11-03, all 86 maps x 2 sides, normal fuel, release; tuner/replays/b208)

| bot | vs v183 | vs proxy-farmer | fuel mean / out-of-fuel turns |
| --- | --- | --- | --- |
| v208 | 89-83, banked +17.3% | 103-69, +11.4% | 36.9k / 0.34% |
| v206 | 87-85, -2.4% | 89-83, +0.4% | 36.8-37.1k / 0.31-0.35% |
| v185 | 88-84, +10.6% | 98-74, +3.9% | 34.4k / 0.28-0.30% |

Head to head: v208 vs v206 97-75 (banked +13.2%), vs v185 93-79 (+7.6%). Deaths vs v183: v208 1870, v206 1886,
v185 2531 (OUTCLASS). Fuel: v208 ~= v206, +7% over v185 (OUTCLASS's cost, as measured in v204).

Prism maps (adgato-gemcombat1-5) were the weak spot: 3-7 vs v185 with PRISM_SAT on. PRISM_SAT off (it only acts on the
Prism team): vs v185 3-7 -> 6-4, vs v183 6-4 -> 8-2, vs proxy-farmer 6-4 -> 6-4. **v208 ships with PRISM_SAT off.**
10 games per cell, so the Prism numbers are a lead, not proof.
