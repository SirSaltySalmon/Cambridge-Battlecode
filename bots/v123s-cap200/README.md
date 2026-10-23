# v123-salmon

`v122` + Prism. Not submitted, not benched (6-map smoke test only).

## Draft
- Five: Prism takes Marcella's pick (`Ryth, Heart of Ruin, Willow, Vesper Vale, Prism`), and Varg's on Varg maps (`PRISM_OVER_VARG`).
- Four: Prism takes Ryth's pick (`Heart of Ruin, Vesper Vale, Willow, Prism`).
- Three: unchanged (Marcella stays, and stays the second collector).

## Switches (`constants.h`)
- `PRISM_CLAIM`: claims by `collect_rank` (route.cpp). Gem-only rewards (gems, no gold: gem clusters) go to Prism first,
  then a funnelled Vesper; rewards with gold go to a funnelled Vesper first, then Prism (Marcella's old place). Once
  Vesper's funnel ends (`VESPER_FUNNEL_EMBLEMS`), Prism is first for everything. Without Prism on the team, Vesper is
  first everywhere as in v122.
- `PRISM_HOLD` (`prism_holds`): Prism never tributes until round `PRISM_CASH_ROUND` (1800), or until our tributed gems
  plus her bag (or every bag: `win_tribute`) reach 1000. Gated: near-bank, hunted bank, band bank, step-7 bank, spawn
  banks, escort requests and escort banking. She recalls to shop with her bag (a recall keeps it). The carrier safety
  rules (recall bar, stay behind the front when hurt with a heavy bag) still apply to her.
- Build: Treasure Focus, Durability Health, Monster Health, Home Health, Monster Health; fallback 2x Bruiser Health to
  1x Bruiser Focus, repeated.
- Play: she is not a mage (no standoff casting); `engage_ranged` casts her spells like Stellar's picker (stun-hit 1.5x
  Focus as the single spell, area 1x) and she still basic attacks after a cast. Under `SQUISHY_HEALTH` she counts as
  squishy like anyone, until her Health emblems lift her past it. Priority 3.

## Smoke test (debug, vs v122, 6 maps, team A only)
4-2. Prism drafted on gem-fields and expanse (both five-player): gem-fields 17 Prism claims (11 gem-only), no
deaths, one tribute, at round 908, the one that won the game; expanse she died at round 604 holding her bag.

## Bench vs v122 (debug with logs, 40 maps both sides; `replays/b123s`, A_ = v123-salmon as team A)
28-52, banked -28.9% (43,434 vs 61,085), K/D 1195/1557. By round 300/600/900: 5.8k/14.2k/22.0k vs 8.7k/20.3k/34.5k.
- Three-player (no Prism, same bot): 6-6.
- Four-player (Prism for Ryth): 1-15. Team deaths 19 vs 10 a game, kills 8 vs 17; our Vesper dies 4.4 vs 1.6.
  Prism: 5.0 deaths, 288 collected, 46 tributed, 184 still carried when the game ended.
- Five-player (Prism for Marcella / Varg): 21-31. Prism 3.2 deaths, 309 collected, 138 tributed, 79 carried at the end;
  team collected 834 vs 920 (our Vesper 304 vs 411: Prism takes the gem-only share).
- Prism over 68 games: 22-46, tributed in 23 games, lost 5,668 gems to deaths (~83 a game, 224 of 247 deaths to
  champions, so the enemy got them). 49 of the 68 games ended before round 1800 (median end round 1405), so her
  cash-in round mostly never came.
