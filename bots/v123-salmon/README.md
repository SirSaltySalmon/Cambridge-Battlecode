# v123-salmon

`v122` + Prism on five-player maps with at least 380 consistent camp gems (9 of the 40 maps). Not submitted. Current
switches = bench 2 (cap 300) + the gem bar (bench 3).

## Draft
- Five: Prism takes Marcella's pick (`Ryth, Heart of Ruin, Willow, Vesper Vale, Prism`), and Varg's on Varg maps
  (`PRISM_OVER_VARG`), only when the map's consistent camp gems (`prism_gems`: wolves 4, elks 15, gem clusters 7 each;
  golems, monoliths and gold clusters left out) reach `PRISM_MIN_GEMS_5` (380). Otherwise v122's pick.
- Four and three: unchanged from v122 (Ryth kept on four; Marcella on three stays the second collector).
  `PRISM_MIN_GEMS_4` is off (100000): Prism for Ryth lost on every four-player map.

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

## Bench 3: the gem bar (`replays/b123u`, `replays/b123v`; map stats in `replays/mapstat2/stats.json`)
Per-map Prism results vs v122 over 3 runs (unseeded cap 300 + fuel-burn seeds 1 and 2, 6 games a map). A map under the
bar plays v122's roster, so it counts as a mirror (half the games, banked even).
- Five-player (26 maps): Prism everywhere 72/156; never 78/156; bars 364-460 all 79-80/156 with Prism-map banked
  +0.5k to +1.8k. Under ~360 she mostly loses (lost-world 0-6, eight 1-5, ambush 1-5, elk-arena 1-5). Bar 380 = 9 maps:
  corners, great-wall, quadrants, road-of-riches, big-world, boss-battle, fracture, islands, yellow-brick-road.
- Four-player (8 maps, seeds 0-2): 2-46 with Prism; best is never (Ryth).
- Check on fresh seeds 3 and 4, those 9 maps only: 19-17, banked +1.4% (islands 4-0, quadrants 3-1, road-of-riches 3-1;
  corners 0-4, boss-battle 1-3). With the fitting runs, 48-42 on these maps over 90 games: a small edge, near noise.
- Draft check: lost-world (356) drafts Marcella, gemfight (4p) Ryth, islands Prism.

## Bench 4: PRISM_CARRY_FIGHT (`replays/b123w`, the 9 Prism maps, both sides, seeds 0-4 = 90 games)
Her bag no longer sends her back from the front or stops her answering pings. Against the same 90 games before it
(`b123t`/`b123u`/`b123v` cap-300 runs): 46-44 vs 48-42, banked +6.5% vs +3.2%, team K/D 11.2/11.7 vs 10.7/11.7 a game.
Prism deaths unchanged (1.69 vs 1.68 a game); the gems lost when she dies rose 113 -> 144 a game (bigger bags on the
front line); she tributed 222 vs 230. Per map within one or two games everywhere: no measurable change. Kept on.

## Bench 5: FIGHT_LOCAL (`replays/b123x/<v123-salmon | v123s-fl1>`, 40 maps both sides, seeds 0 and 1, 160 games)
The fight score counted every ally in sight (our ward's view too, recalling allies too) and every enemy within
FIGHT_STEPS of any of them. Now: allies within FIGHT_ALLY_RANGE (8) of us or of the near enemy closest to us, not
channelling; enemies within FIGHT_STEPS walking steps of us or of an ally within 8 of us.
- It fires: of 35.4k logged fight scores, 11% left out a far ally and 3.6% a far enemy (14% either). (The leader's line
  is only logged when it changes, so these are shares of logged scores, not of turns.)
- Results vs v122: seed 0 40-40 (-1.6%), seed 1 37-43 (-2.8%). On the 31 maps without Prism (where the bot was v122,
  so even before this change): 29-33 / 29-33, banked -4.0 / -6.4%, K/D 1164/1255, 1171/1194. On the 9 Prism maps
  11-7 / 8-10 (before it: 48-42 over 90 games). Slightly worse than not having it, inside noise.

## Bench 6: FIGHT_LOCAL_RANGE and FIGHT_LOCAL_WALK (`replays/b123y`, `replays/b123z`, 160 games each vs v122)
The range is now its own constant, FIGHT_LOCAL_RANGE. FIGHT_LOCAL_WALK measures it in walking steps (one capped search
from us, a second from the near enemy only for an ally out of our reach but within range of it in tiles).

| variant              | wins    | banked | K/D       | teamfights W-L | non-Prism maps     |
|----------------------|---------|--------|-----------|----------------|--------------------|
| 12 tiles (kept)      | 79-81   | +5.9%  | 2760/2469 | 507-381        | 60-64, +3.2%       |
| 8 tiles              | 77-83   | -2.2%  | 2751/2496 | 503-429        | 58-66, -5.2%       |
| 9 tiles              | 73-87   | -6.0%  | 2649/2781 | 468-473        | 54-70, -11.8%      |
| 9 walking steps      | 77-83   | -5.2%  | 2569/2629 | 466-434        | 62-62, -5.4%       |
| 12 walking steps     | 70-90   | -10.8% | 2552/2724 | 464-461        | 55-69, -13.0%      |
| 15 walking steps     | 71-89   | -6.1%  | 2722/2745 | 488-478        | 57-67, -5.1%       |

(Teamfight: two or more champion deaths within 8 rounds and 15 tiles; won = more enemy deaths than ours.)
- Do counted allies come? An ally in sight of a fighting champion joins (in a fight branch, or within 4 tiles of it,
  within 4 rounds) 84% of the time at 5 tiles, 55% at 8, 45% at 9, 23% at 12. Joined or walked 3+ tiles closer: 55% at
  9 down to 48% at 12, and about 47% at 13-16, where it does not count: being counted does not make an ally come.
- Every cut tighter than 12 tiles lost; walking steps (never fewer than tiles) cut more allies and lost more. 12 tiles
  is level with no cut on the non-Prism maps and fixes the ward over-count: kept, FIGHT_LOCAL_WALK off.
- Fuel (12 profiled games each, fight-heavy maps): a walking search is ~1,660 fuel, on ~28% of scoring turns: +465 a
  turn on average, near_by_steps 1,448 -> 1,993 a call. With it off it compiles out. The assist list is now built only
  when score_mode's cached terms miss (half the calls) into a reused vector; decisions are unchanged (replays match
  until the first turn the old build ran out of fuel and the new one did not).
- 15 walking steps (`replays/b123w15`) also lost: its search is ~1,830 fuel (near_by_steps 2,160 a call vs 1,448).

## Bench 7: fight-score weighting and tuning (`replays/b124`, `replays/tune1`, `replays/tune2`)
Flags (constants.h, v124): FIGHT_ALLY_WEIGH (no hurt or sent-back carrier allies; the rest by JOIN_WEIGHT, the measured
join rate by distance), FIGHT_ENEMY_WEIGH (no channelling or hurt enemies; the rest by JOIN_WEIGHT from the enemy
nearest us), FIGHT_COMMIT_PING (a champion committing to a fight that counted an ally over 8 tiles off pings; answers
from 12 tiles).

Each vs v122, 40 maps both sides, seeds 0 and 1 (160 games); "farming" = counted far allies (9-12 tiles) farming
while the fight goes on:

| variant  | wins | banked | K/D       | teamfights | farming |
|----------|------|--------|-----------|------------|---------|
| neither  | 79   | +5.9%  | 2760/2469 | 507-381    | 28.9%   |
| ally     | 80   | +1.0%  | 2752/2526 | 514-432    | 25.8%   |
| enemy    | 83   | +10.5% | 2890/2682 | 527-489    | 28.0%   |
| ping     | 68   | -12.1% | 2574/2629 | 460-455    | 21.9%   |
| all      | 81   | +1.2%  | 2792/2697 | 510-502    | 20.6%   |

The ping brings far allies in, but pulling farmers off camps costs more than it wins: off.

Weight screen (seed 0, 80 games vs v122, one step of FIGHT_THRESHOLD, MISSING_BODY_PENALTY, OUTNUMBER_BONUS,
HEALTH_EDGE, POWER_EDGE, EMBLEM_BONUS, FIGHT_HYSTERESIS each way, on enemy and on ally + enemy): enemy alone was the
better base; HEALTH_EDGE 2 led (46/80, +23.6%, teamfights 311-165), then HYSTERESIS 5, EMBLEM 5, POWER 10.
Combinations, seed 0, vs v122 + v120 (160 games):

| variant (all with enemy weighting) | wins | banked | K/D       | teamfights |
|------------------------------------|------|--------|-----------|------------|
| current before (neither)           | 80   | +6.0%  | 2829/2566 | 509-433    |
| enemy weighting                    | 83   | +10.7% | 2959/2686 | 554-486    |
| + HEALTH_EDGE 2 (shipped)          | 93   | +21.6% | 3211/2275 | 613-344    |
| + HEALTH_EDGE 2, EMBLEM 5          | 93   | +13.4% | 3079/2446 | 560-425    |
| + HEALTH_EDGE 2, HYSTERESIS 5, EMBLEM 5 | 88 | +9.9% | 3036/2464 | 563-444    |
| + HEALTH_EDGE 2, HYSTERESIS 5      | 82   | +2.9%  | 2891/2481 | 523-428    |
| + those three and POWER_EDGE 10    | 81   | +9.3%  | 2771/2512 | 484-434    |

Shipped: FIGHT_ENEMY_WEIGH on, HEALTH_EDGE 2. It wins 64% of teamfights (54% before) and both opponents alike
(46/80 vs v122, 47/80 vs v120). Steps that won alone lost when stacked (hysteresis 5 above all).
- Shipped + FIGHT_ALLY_WEIGH (Prism never a sent-back carrier, as before; `replays/tune3`): 81/160 vs v122 + v120,
  banked +2.4%, K/D 2795/2629, teamfights 487-475, against the shipped 93/160, +21.6%, 613-344: stays off.
