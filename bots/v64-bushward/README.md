# v64-bushward

`v64` + bush wards. A champion's one ward is placed once and then stays: either on a ward spot as
before, or in a key bush, whichever comes first.
- Key bushes (`build_bush_boards()` in `warding.cpp`, built once from leftover fuel at a turn end):
  bush tiles within 6 of an altar first, then within 10 of the map centre, then within 4 of a route
  camp (`BUSH_ALTAR_RANGE`, `BUSH_CENTER_RANGE`, `BUSH_CAMP_RANGE`).
- `bush_ward()`: with the ward unused and off cooldown, not a fight turn, no enemy in sight, not
  standing in a bush, and a key bush in reach (one read of a board grown by the placement range), the
  ward goes on a bush tile we cannot see, unless a visible allied ward is within 4 tiles
  (`BUSH_WARD_GAP`). A refused placement means something hidden was there, now revealed.
- `place_ward()` and `ward_detour()` leave a champion whose ward is in a bush alone.
vs v64 (34 games): normal fuel 21-13, gems 27,558 vs 22,002, deaths 566 vs 809; unlimited fuel 19-15,
gems 26,520 vs 23,372, deaths 630 vs 705.

# Base: v64 (was v63-cpp-lean)

`v63-cpp-port` + fuel optimizations only. With unlimited fuel its decision log is byte-identical to
`v63-cpp-port` on all 17 maps; average fuel per turn 52.2k -> 49.6k (-5%), turns over 100k
0.74% -> 0.47% (lanes 3.96% -> 1.59%). vs v63-cpp-port at normal fuel: 23-11, gems 28,411 vs 21,302,
deaths 594 vs 805.
- `seen.cpp` `visible_living()`: fixed arrays instead of vectors; with one unit in sight no tile board
  is read; tiles come out with `positions_into()` into a stack buffer instead of a new vector.
- `route.cpp`: `observe()` and `visible_loot()` use a reused sorted vector instead of `std::set`
  (same ascending order); `mid_clear()` reads the vision list directly.
- `board.cpp` `blocked_tiles()` and `actions.cpp` `move_towards_closest()`: board expressions built
  in place (`|=`, `&=`), as every `|`/`&`/`~` result is a new board (an allocation and a full copy).
Tried and dropped because they cost more than they saved: caching `blocked_tiles()`, item reads,
emblem counts or `under_threat()` per vision list (hits are rare; copying a board or item array
costs ~300 fuel).

# Base: v64-cpp-stellar-ren

C++ port of `v63-fuelB`. Same draft, shop, route, fight and log logic. With unlimited fuel
(`--fuel 100000000 --log-limit 100000000`) its decision log is byte-for-byte identical to v63-fuelB
on packs, rows, ambush and contest, from both sides.

Build and run like any bot: `cambc run v64-cpp-stellar-ren v63-fuelB --map packs`
(`cambc` downloads the WebAssembly compiler on first build).

## Files (Python module → C++ file)

| Python | C++ |
| --- | --- |
| `main.py` | `main.cpp` |
| `bot.py` (state) + every mixin's methods | `bot.h` (one `Bot` class) |
| `turn.py` | `turn.cpp` |
| `route.py` | `route.cpp` |
| `combat.py` | `combat.cpp` |
| `kits.py` | `kits.cpp` |
| `tribute.py` | `tribute.cpp` |
| `warding.py` | `warding.cpp` |
| `shop.py` | `shop.cpp` |
| `log.py` | `log.cpp` |
| `actions.py` | `actions.h/.cpp` |
| `board.py` | `board.h/.cpp` |
| `state.py` | `state.h/.cpp` |
| `seen.py` | `seen.h/.cpp` |
| `action_trace.py` | `trace.h/.cpp` |
| `constants.py` | `constants.h` |
| `draft.py` | `draft.h/.cpp` |

`randomness.py` was unused in v63-fuelB, so it was not ported.

## Fuel: what is different from Python

C++ gets 100k fuel per turn (Python 2.5M), and in WebAssembly some things cost far more than you
would expect. These were changed for fuel only; none of them changes a decision or a log line:

- Engine errors are never thrown on purpose. A thrown and caught `cambc::error` costs ~2.5k fuel, so
  item and gem reads on allies out of sight use raw engine calls that return a code (`state.cpp`).
- Visible champion lists are built from the cached roster (`seen.cpp`), in the engine's tile order.
- `format()` (`board.cpp`) is a small printf replacement: `vsnprintf` prints floats through
  software 128-bit `long double` (~3-5k fuel per number).
- Every `std::string`/`std::vector`/`std::map` allocation costs ~300-500 fuel, so hot paths use flat
  arrays (tile → slot grid, ally sightings, travel table) and return cached lists by reference.
- Python's `sum()` of floats is compensated (Neumaier); `PySum` copies it so scores tie the same way.
- The draft's route search and setup reuse one read of the camp groups (`draft::group_data()`).
- Spawn boards per route group are built on first use; goal fields are budgeted by measured cost.

Out-of-fuel turns are 0.1-0.6% on most maps (about 2.5% on lanes), similar to v63-fuelB.
