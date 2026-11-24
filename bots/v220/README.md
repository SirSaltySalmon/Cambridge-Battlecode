# v220 (v219-ward + v217 teleport crystals + v216-focus-v + v217-farm's SHOP_CADENCE and PT_*)

Contents in one line: v213 + ward cleanup / SCOUT_MASK=2 / catch-up hold + teleport crystals + FOCUS_KEEP / FOCUS_VALUE
(PEEL off) + SHOP_CADENCE + PT_GATE / PT_GATE_MONO / PT_COMMIT / PT_SCAR. CAPTURE_HYGIENE and PREARM_READ are compiled
in and off. The source equals the downloaded v218 (`downloaded v218.zip`, v216-focus-v + SHOP + PT) plus the ward and
crystal changes: built both ways (focus-v changes into v220; v220's ward + crystal changes into the download), the two
results differ only in one comment line.

A three-way merge over v213 (`git merge-file`, file by file), with no conflicts:
- v219-ward changes: ward cleanup, SCOUT_MASK=2, catch-up hold (see bots/v219-ward/README.md);
- v217 changes: teleport crystal travel and its fuel fixes (see bots/v217/README.md; new files crystal.cpp/.h).

Both sides touched bot.h, constants.h, draft.cpp, route.cpp and turn.cpp, in different places. Where they meet:
- **Draft memory:** v219 shortened the setup block at the front (no ward spots, SETUP_MAGIC 0xA8). v217 puts its crystal
  block at the tail and reserves room for it before trimming the aggressive route. They don't overlap, and the
  shorter setup block leaves v217 more room. Checked on 00ambush: `setup: crystal value 32, 90 jumps, buying` on every
  champion.
- **turn.cpp:** v219 changed the ward line (turn order step 1); v217 added the crystal gate after `update_carrier` and
  the restock recall in the shop step. These are separate lines.
- No v217 code uses the ward code that v219 removed.

Checks: debug and release builds compile. One debug game on 00ambush vs v213: 967-937 (round limit), no errors, crystal
and scout-bush ward activity both in the log.

## Farming merge (2026-11-03): the two v217-farm winners

Nic's v202 farming code (bots/v217-farm, ported onto v216-focus-v) merged in with `git merge-file`, base
bots/v216-focus-v, theirs bots/v217-farm. Benched there vs v216-focus-v, seed 0, 172 games each:

| Switch | Record | Banked | K/D | State here |
| --- | --- | --- | --- | --- |
| SHOP_CADENCE (income trips, recall on the reward, Vesper an emblem a trip + Capture first, EARLY_GOLD) | 96-76 | +16.4% | 1702/1516 | **on** |
| PT_GATE, PT_GATE_MONO, PT_COMMIT, PT_SCAR (power-to-threat gates, finish started groups, scarred groups) | 94-78 | +8.2% | 1674/1817 | **on** |
| CAPTURE_HYGIENE | 78-94 | -10.6% | 1469/1923 | off |
| PREARM_READ | 85-87 | -4.4% | 1520/1713 | off |

The two losers stay in the source, off (`CAPTURE_HYGIENE = false`, `PREARM_READ = false`, `TANK_STEP_FIX = false`), so
the code paths are the ones benched in v217-pt and v217-shop (each ran with the others compiled in and off). Why they lose
(code reading, no logs):
- CAPTURE_HYGIENE: CAPTURE_MOVE_FAST drops an ally-held reward from every goal and from TARGET_HOLD, so each extra
  concurrent capture leaves one keeper alone for 3-6 rounds. keep_capture runs before the fight step and holds through an
  enemy within 10 steps with <= 2 rounds left, while the fight score still counts the keeper as a body. Vesper also loses
  rewards to keepers (she claims a held one only from about 4 tiles).
- PREARM_READ: teams alternate inside a round and the starting team swaps every round, so for some observer/enemy pairs a
  real recall's timer drops 2 then 0 between our turns. The "same timer on consecutive rounds" test then flags a team
  that never pre-arms, and first-round (or more) recalls stop counting as channelling: target priority, recall-breaking
  stuns, chase lines and enemy weighting all skip them. A fix would need two flat readings in a row (three rounds).

Where the lineages meet:
- **Stamp byte:** v217-farm's layout: round stamp bits 0-2 (round % 7 + 1, PT_COMMIT), ENGAGED_FLAG bit 3, focus index
  bits 4-6 (FOCUS_INDEX_MASK 0x07), FOCUS_KEEP's refresh bit 7. write_focus keeps the flag and the stamp. Stamps are only
  compared with this round's and last round's.
- **Focus merge (v213 -> v216-focus-v into v220):** bot.h, combat.cpp, constants.h, state.h and turn.cpp merged
  without conflicts; state.cpp's write_focus took the download's version (refresh bit + engaged flag). The first farm
  merge had left constants.h, shop.cpp, state.cpp (and v219's warding.cpp) with LF endings; all files are CRLF again.
- **shop.cpp:** buy_items resets SHOP_CADENCE's emblem-rest cache after a purchase and then buys v217's crystal from the
  leftover gold (TP_FIRST false), in that order.
- **turn.cpp step 8:** income_shop runs first, then the existing shop bar, then v217's TP_RECALL restock. Every shop
  recall goes through recall_to_shop, which now stamps shop_trip_round (SHOP_SPACING).
- One ungated change from v217-farm comes along (it was in every v217 variant bench): share_loot only adds the deciding
  champion to the claim candidates when it is free (free(me)), as for allies.

Smoke check (not a bench): debug and release builds compile. One debug game on 00packs vs v213: v220 won 1022-274 at
r1040, kills 14-1, no crashes, no error lines (only recall refusals, which v213 logs too). In the log: 17 income trips
(13 "finishes the emblem", 2 hurt, 2 free trips), 37 recalls started on a reward (`capture.shop.recall`), 14 PT openings,
38 PT_COMMIT keeps, EARLY_GOLD fading 1.00 to 0.00 (that game was before the focus merge).

After the focus merge: debug and release builds compile. One debug game on 00ambush vs v213: lost 804-901 at the round
limit, kills 10-16 (one game; the pre-farm v220 went 967-937 there), no crashes, no error lines from v220. All parts
active: crystals bought on all five (90 jumps), 67 teleport lines, focus set / by value / stale lines, 92 PT openings,
28 PT_COMMIT keeps, 10 income trips, 30 recalls from a reward, scout wards. Fuel: mean 42.0k per turn vs v213's 41.4k
(+1.6%), p95 72.1k vs 68.5k, out-of-fuel turns 26 of 9,995 vs 18 of 9,999.

**Not benched.** SHOP + PT were never run together (v217-farm's README planned that next), nor on top of the ward,
crystal and focus changes. Fallbacks: `SHOP_CADENCE = false`, `PT_GATE / PT_GATE_MONO / PT_COMMIT / PT_SCAR = false`,
`WARD_HOLD = false`, `TP = false`, `FOCUS_KEEP / FOCUS_VALUE = false`. Fuel was not measured (v217-farm's numbers: SHOP +0.3%, PT +0.9% per turn).
