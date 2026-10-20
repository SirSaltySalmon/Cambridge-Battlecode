# v101

**Final settings:** `WILLOW_BACKLINE`, `WILLOW_SAFE_DODGE`, `WILLOW_CAMP_SAFETY`, `WILLOW_HEAL_SKIP_RECALL`,
`WILLOW_CLAIM_DROP` on; `WILLOW_HEAL_THRESHOLD` on at 25% (`HEAL_MIN_SHARE`); `WILLOW_SAFE_CLAIM` off. The final build
went 41-43 (gems +8.5%) against v100, with Willow at 3.41 deaths and 18,065 healing a game (v100's: 5.87 and 15,021).
The sections below record each step.

`v100` + Willow combat changes (switches in `constants.h`). Not submitted. Tested on cambc 3.0.1 over the 21-map
pool (84 games per opponent: every map from both sides, at normal and at unlimited fuel).

Why: in v100 Willow died ~4-6 times a game (in v100 mirror games 30 of 40 deaths to champions, mostly mid-fight or
fleeing; 10 to camps, 7 of them while fleeing) and was dead ~19% of the game, and a dead Willow heals nothing.

## Changes
- `WILLOW_BACKLINE`: in a fight against champions she heals, then holds a tile out of enemy and awake-camp reach
  (`threat_zone`) within heal reach (4) of the ally nearest the enemy; else within 6 of it; else anywhere out of
  reach. She no longer walks in to shoot champions (her champion damage was ~760 a game). This also covers stepping
  back after a heal.
- `WILLOW_SAFE_DODGE`: fleeing at low health toward an ally only along a path that stays out of reach (otherwise the
  nearest safe tile, as before).
- `WILLOW_CAMP_SAFETY`: (a) waiting for a tank she stands out of every visible camp's reach, not only the target's;
  (b) a camp counts as tanked only by an ally at 50%+ health; (c) a recall broken by a hit from nothing in sight is
  not restarted on the same tile (she steps toward an ally or the spawn first); (d) before shooting a camp, every awake
  camp that reaches her needs a sturdy ally (50%+) closer to it than her (on 01boss-battle a group's other camp turned
  on her when the tank stepped away).
- `WILLOW_HEAL_SKIP_RECALL`: never heal an ally who is recalling or on our spawn (both end at full health). A probe
  of 2,643 heals found 5.0% went to recalling allies (~9% of her useful healing) and 1.1% to allies who started a
  recall within 2 rounds. Vs v101 without it: 42-42, gems +3.2% (neutral; kept because those heals are pure waste).
  The same probe: only 51% of her heal amount lands, and half her heals restore under half a heal (overheal).
- `WILLOW_HEAL_THRESHOLD` (user's rule, **set to 25%**): heal only an ally missing at least `HEAL_MIN_SHARE` of her heal
  (3 x Focus). At 55% it works as
  meant (heal amount used 51% -> 85%, no heals under half), but she heals 46% less often and total healing drops:
  alone vs v101 without it 41-43, gems -5.4%. 55% was dropped.
  At 25% (with the claim drop on): vs the claim drop alone 43-41, gems +8.6% (Willow deaths 3.35 vs 4.19, healing
  17,743 vs 16,570); vs v100 41-43, gems +8.5% (claim drop alone vs v100: 48-36, +7.8%). Probe: 1,936 heals (none 2,643,
  55% 1,422), heal amount used 67% (none 51%, 55% 85%).
- `WILLOW_CLAIM_DROP`: Willow hands a claim back as soon as an enemy champion is in her sight, and the leader does not
  give her one while it sees an enemy champion (if someone else can take it). Alone vs v101 without it: 43-41, gems +3.5%.
- Both together vs v101 without them: 38-46 (gems -9.8%); vs v100: 43-41 (+3.6%; v101 without them: 52-32, +28.5%);
  vs the brawler: 41-43 (+0.4%).
- `WILLOW_SAFE_CLAIM` (**off**): no risky claims for Willow. Alone it lost 38-46 (gems -8.3%): captures slowed more than
  it saved her.

## Results (84 games each)

| Opponent | v101 (final) | gems | Willow deaths/game (theirs) | Willow healing/game (theirs) |
| --- | --- | --- | --- | --- |
| v100 | **52-32** (26-16 at both fuels) | **+28.5%** | 3.46 (6.12) | 19,646 (14,856) |
| brawler (v100 that takes every fight) | 43-41 | +0.4% | 4.03 (4.72) | 18,440 (15,438) |

v100 against the same brawler: 43-41 (+2.5%). With `WILLOW_SAFE_CLAIM` on: 46-38 (+10.4%) vs v100, 47-37 (+10.2%) vs the
brawler (95-73 vs 93-75 over both, more gems without it).

Each change alone vs v100: backline 44-40 (gems +13.9%, Willow deaths 4.09 vs 6.00); safe dodge 44-40 (+5.8%); camp
safety 43-41 (+1.8%); safe claim 38-46 (-8.3%).

## Fuel (unlimited fuel, all 21 maps x2, probe copies)
v101 mean 23.6k a turn, p99 57.3k, 0.07% of turns over 100k (v100: 22.9k / 56.0k / 0.07%). Willow's own turns 25.0k
(v100: 21.6k).
