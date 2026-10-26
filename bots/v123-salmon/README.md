# v123-salmon

`v122` + Prism on five-player teams. Not submitted. Benched twice vs v122 (below); current switches = bench 2, cap 300.

## Draft
- Five: Prism takes Marcella's pick (`Ryth, Heart of Ruin, Willow, Vesper Vale, Prism`), and Varg's on Varg maps (`PRISM_OVER_VARG`).
- Four and three: unchanged from v122 (Ryth kept on four; Marcella on three stays the second collector). Prism for Ryth
  on four went 1-15 in the first bench.

## Switches (`constants.h`)
- `PRISM_CLAIM`: claims by `collect_rank` (route.cpp). Gem-only rewards (gems, no gold: gem clusters) go to Prism first,
  then a funnelled Vesper; rewards with gold go to a funnelled Vesper first, then Prism (Marcella's old place). Once
  Vesper's funnel ends (`VESPER_FUNNEL_EMBLEMS`), Prism is first for everything. Without Prism on the team, Vesper is
  first everywhere as in v122.
- `PRISM_HOLD` (`prism_holds`): Prism never tributes until round `PRISM_CASH_ROUND` (1800), or until our tributed gems
  plus her bag (or every bag: `win_tribute`) reach 1000, or the enemy has tributed `PRISM_CASH_ENEMY_GEMS` (750; she
  stops holding for the rest of the game), or her bag reaches `PRISM_BAG_CAP` (300; banked whole, then she holds again). Gated: near-bank, hunted bank, band bank, step-7 bank, spawn
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

## Bench 2 vs v122 (debug with logs, 40 maps both sides; `replays/b123t/<variant>`)
Ryth back on four; enemy-750 cash-in on in all three. Three- and four-player games are the same bot as v122 (6-6 and
9-7 are side noise), so the five-player line is the one that measures Prism.

| variant | overall | five-player | banked (5p) | Prism tributed | lost to deaths | held at end | first tribute (median) |
|---|---|---|---|---|---|---|---|
| first version (Prism on four too, 1800 only) | 28-52 | 21-31 | -33.7%* | 116 | 83 | 104 | r1613 |
| enemy 750, no cap | 38-42 | 23-29 | -16.4% | 182 | 86 | 41 | r1272 |
| enemy 750, cap 200 | 38-42 | 23-29 | -12.7% | 195 | 68 | 13 | r1040 |
| **enemy 750, cap 300 (= v123-salmon now)** | **41-39** | **26-26** | -12.5% | 200 | 84 | 39 | r1261 |

\* first-version Prism columns are over all 68 Prism games (four- and five-player). Per-game gem figures are Prism's own.
Prism dies ~3.3 times a game in every variant; her deaths still hand the enemy ~70-85 gems a game. The cap-200 vs
cap-300 gap (3 wins) is inside bench noise (+-5 wins).
