# zz-wardprobe (v213 + ward measurement lines)

Not a candidate bot. This is v213 with log-only "WP ..." lines that measure how the ward charge is spent. Every
probe statement sits inside `LOG(...)` (a `debug_block`), so in debug builds it costs no fuel, and release builds
leave it out. Play should be identical to v213. One difference: `LOG(log_turn(me))` is removed from `turn.cpp` so
the probe lines fit under the 1 MB per-team log limit.

## Files

- `src/probe.h`: helpers.
  - `ward_only(tile)`: true when `tile` is outside the champion's *own* sight, measured with
    `champion::visible_tiles_from(position)`. Do not use `champion::visible_tiles()`: it seemed to include the ward
    and reported zero ward-only sightings.
  - `gain(ward)`: tiles the ward would see that we don't already see ourselves.
  - `ev(text)`: prints `WP r<round> c<champion enum> <text>`.
- `probe.patch`: the whole probe as a diff against `bots/v213/src`. To add it to a copy of v213, run this from the
  repo root before you make your own edits (it touches `scout_ward`, `ghost_ward` and `answer_hidden`):
  `git apply -p2 --directory=bots/<your-folder> bots/zz-wardprobe/probe.patch`
- `wp_analyze.py`: `python -I bots/zz-wardprobe/wp_analyze.py <dir with *.repl26a>`. It reads the team under test
  (team A in `-a` replays, team B in `-b` replays, as `bench.py manifest` lays them out) and prints the summary below.

## Lines it prints

| Line | When |
|---|---|
| `place scout-bush\|scout-catch\|scout-other gain=G dist=D rev=R` | `scout_ward` placed. R = enemy champions in sight after minus before. |
| `scout refused X Y` | `place_ward` returned false: a hidden unit stands on (X, Y). It is revealed for the round and the charge is kept. |
| `place ghost ok=0/1 gain=G rev=R` | `ghost_ward` tried a tile. |
| `blocked ghost cd=C gone=N` | Live ghost (target unseen for N ≤ `GHOST_ROUNDS` rounds), but the charge is recharging. |
| `place hidden ok=0/1 rev=R` / `blocked hidden` | `answer_hidden` placed, or wanted to but had no charge. |
| `hit atk\|cast\|area wardonly=0/1` | We hit an enemy champion (basic attack, unit cast, or a ground cast with an enemy within 1 of the target tile). 1 means the target was outside our own sight: only the ward showed it. |
| `wonly n=K of M` | At turn start, K of the M enemies within 10 tiles are seen only through our ward. |
| `stat turns=T ready=R near=N wonly=W` | Every 50 rounds, cumulative per champion: alive turns, turns with the charge ready, turns with an enemy within 10, turns with a ward-only enemy within 10. |

## Baseline (2026-11-03, 140 games vs v210, all 70 maps, both sides, `replays/bwardprobe/`)

Record 71-68-1, banked +3.7%.

- Charge ready on 51% of alive turns.
- Placements:

  | Placement | Count | New tiles | Revealed an enemy | Ward-needed hits per placement |
  |---|---|---|---|---|
  | scout-catch | 22,938 | 5.3 avg, 0 in 65% | 0.6% | 0.008 |
  | scout-bush | 14,034 | 50.5 | 8.4% | 0.092 |
  | ghost | 402 | – | 70% | 0.76 |
  | hidden | 98 | – | 83% | 1.33 |

- Hits on enemy champions: 28,051, of which 1,898 (6.8%) needed the ward.
- Ghost episodes: 3,240, of which 2,996 (92%) found the charge spent and 2,838 never got a ward. The charge was spent
  by scout-catch 1,286, scout-bush 1,395, ghost 177, hidden 54; median 5 rounds earlier (p25 2, p75 7).
- Hidden-attacker turns with no charge: 354.
- Scout refusals: 12,296, of which 73% came from a tile the same champion had already been refused on 5+ times that
  game (up to 144).
