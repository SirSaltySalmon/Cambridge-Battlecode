# v63-cpp-both

`v63-cpp-home` + `v63-cpp-spawnskip` (see those READMEs); nothing else.

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
