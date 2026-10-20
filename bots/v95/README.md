# v94

`v93` for the balance patch (GameRules.md): a new roster, Vesper Vale back as the loot carry, and camp farming that
uses each camp's real reach. **Not benchmarked:** the local engine (cambc 2.2.5, the latest release) still runs the old
numbers and has no golem, so v94 only builds and runs there. Benchmark it once the patched engine ships.

## Why

- Cassi & Issac and Stellar were nerfed; Vesper Vale gains 33% gold (was 20%) and 50 strength.
- Camps: wolves hit at 3, elks and golems (new: 5,000 health, 120 strength, 36 gems) at 4, the monolith at 5. A spell
  reaches 4, so only wolves can still be outranged: the v81-v92 mage standoff no longer works on anything else.
- The new maps have many clusters, which suits Varg's splash.

## Changes

- **Roster** (`draft.cpp`): Varg replaces Cassi & Issac, Vesper Vale replaces Stellar. 5 picks: Varg, Ren, Ryth,
  Willow, Vesper; 4: Varg, Ren, Vesper, Willow; 3: Varg, Ren, Vesper. Without Cassi and Issac the split phase never
  starts, and without mages the mage code stays idle.
- **Vesper is the loot funnel again** (`vesper_funnel`, `vesper_claim`, the shared done bit 0x80 `VESPER_DONE_FLAG`):
  she collects captures first until she owns `VESPER_CORE_EMBLEMS` (4). The aggressive route waits for her core build.
  Ren had this from v70 to v93.
- **Vesper's build** (`VESPER_STRENGTH`, `vesper_recall_gold`): Treasure Strength first, so 53% bonus gold. She recalls
  at 150 gold for it, then once her gold covers the other three core emblems (Monster Strength, Power Strength,
  Durability Health), and buys them in one trip. After the core she recalls at 600 (`VESPER_RECALL_GOLD`). She keeps her
  bag on these recalls and banks like everyone else. The unused v64 batch counter (`batch_spent`) is gone.
- **Ren keeps his carry rules** once he owns `REN_CORE_EMBLEMS` (2) from his own shopping: he banks at 150 gems, fights
  only with an ally within 5, answers pings while carrying under 100 gems, recalls at 150 gold while rushing, keeps his
  stun logic and acts first.
- **Varg** (`VARG_SPLASH`, `VARG_CAMP_SHIELD`): on a camp, he hits the camp in reach (of the target's group) whose
  radius-1 splash covers the most other living camps. His shield also goes up while awake camps hit him for 60 or more
  a round.
- **Camp reach** (`camp_kind_reach`, `MONOLITH_REACH` 5, `MONOLITH_GUARD_RANGE` 6): out-of-sight camp spots, the finish
  check's tank test and the hold-back fallbacks use each kind's reach (any kind this SDK does not name is treated as the
  golem: reach 4, strength 120, 5,000 health in the draft's estimate). The hold ring was already reach + 1: 4 for wolves,
  5 for elks and golems, 6 for the monolith.
- **Mage standoff** (`MAGE_OUTRANGE`, `outranged`): the standoff runs only on a group whose every hitting camp in sight is
  outranged by the spell. On any other camp a mage holds back for a tank and then hits like a ranged champion.
  `monolith_delegated()` is false while `MONOLITH_REACH >= MAGE_CAST_RANGE`.

## Follow-ups

- Ward cooldown 10: the scout ward (`FIX_SCOUT_WARD`) can leave no charge for `FIX_HIDDEN`'s reveal ward.
- Re-tune `FINISH_HEALTH`, `V93_CAMP_EXIT` and `PAIR_*` for the golem's 120 strength.