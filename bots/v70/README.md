# v70 (ladder upload; was v73 locally)

`v71` + only the parts of `v72` that held up against other bots:
- **Fuel, exact** (`v71-fuel` + `v71-draft`): camps from the map camp list with one raw read per visible
  camp, raw camp-group state in `observe()`, and the ward spots + solo/escort altar split worked out in the
  draft and passed in draft memory (each champion's first turn 25-40k cheaper).
- **Gem-starved maps** (`v71-gemless`): on a map where only the monoliths carry gems (contest), the monolith
  is the goal once its gates pass. As side A on contest: 400 gems tributed (v71: 0).

Checked vs v71 with unlimited fuel: decision log byte-identical on every map except contest (32 of 34
games); avg fuel 47.3k -> 46.3k per turn, turns over 100k 1151 -> 699 (-39%). At normal fuel 16-18, which is
fuel noise: the rules are v71's (v71-draft alone, same rules, went 19-15 / 24-10 vs v67 / v64 where v71 went
22-12 / 22-12).

Left out from v72: the dash permit (38-30 vs v67 + v64 against v71's 44-24; gems vs v64 -13%) and
score-a (no gain anywhere). See `v72/README.md` for every variant tried.

# Base: v71

# v71 (copy of v70-mono-near)

`v70-stellar-ren` + both `v70-mono` (monolith as a last resort) and `v70-near` (near enemies). See those READMEs.

# Base: v70-stellar-ren


`v64-bushward` with Stellar and Ren instead of Vesper Vale and Varg. Ren is the early tank carry: loot
is funnelled to him until his core build is done, then the team plays aggressive.

Results (34 games, all 17 maps from both sides, normal fuel):
- vs v64-bushward: **20-14**, gems 25,417 vs 21,432 (+19%), deaths 572 vs 744 (-23%), kills 544 vs 421.
- Fuel: about 46k per turn on average across all maps (53k on the four heaviest), 99% of turns under
  96k, 0.4-0.5% of turns over the 100k budget.

## What changed from v64-bushward

**Draft** (`draft.cpp`): Ren replaces Varg and Stellar replaces Vesper on every team size.
- 5 picks: Cassi & Issac, Ren, Ryth, Willow, Stellar
- 4 picks: Cassi & Issac, Ren, Stellar, Willow
- 3 picks: Cassi & Issac, Ren, Stellar

**Builds** (`constants.h`):
- Ren: Bruiser Emblem of Health, Durability Emblem of Health, then Power Focus shards without limit.
- Stellar: Power Emblem of Focus, Bruiser Emblem of Health, then Power Focus shards.

**Ren's rush, until he owns 2 emblems** (`REN_CORE_EMBLEMS`):
- Captures are collected by Ren first (`ren_funnel()`, `ren_claim()` in `route.cpp`).
- He recalls to shop at every 150 gold, before banking and with gems still carried (`turn.cpp` 7a).
- He banks like everyone else (40+ gems).
- He stuns a camp only when already next to it, since the stun costs his walk.

**Ren after 2 emblems:**
- Banks only at 150 gems, above everything except a fight or a retreat, alone at the nearest altar,
  then recalls to shop (`turn.cpp` 5b).
- Takes a fight his own fight score likes only with an ally within 5 tiles (`REN_FIGHT_ALLY_RANGE`).
- In a fight he stuns the lowest-health enemy in stun range; while farming, an enemy in range is
  stunned before the camp.

**Aggressive route:** once Ren has 2 emblems and the team has at least 4 (`TEAM_AGGRESSIVE_EMBLEMS`).
Only Ren can read his own items, so he sets the "Ren done" bit in shared memory (the old Vesper bit,
byte 0, 0x80). A team without Ren is never held back by it.

**Turn order:** Ren acts first (priority 7), so his stun lands before the team's damage.

**Unchanged:** route planning, fight score, banking for everyone else, wards (follow the team), the
monolith rule (12 team emblems).

## Fixes in this version
- A turn that runs out of fuel resumes next round at the same line, with vision lists a round old;
  reading a unit that has left sight threw "Unit is not in vision". If a turn fails and the round has
  changed, it is played again with fresh vision (`play_turn()` in `turn.cpp`; no cost on normal
  turns). vs without it: 18-16, gems +7%, no crashes.
- Inherited from v64-bushward: no crash when a dash loses sight of the target (`sighted()`).

## Tried and dropped (34 games each)
| Idea | Result |
| --- | --- |
| Ren dashes toward the lowest-health enemy to stun it next turn | 15-19 vs without |
| Fight score counts enemy emblems (3 x (ours - theirs)) | 17-17; deaths +33% (more retreats) |
| Fight score counts only allies within 5 tiles | 18-16, no effect |
| Side B: skip contested camps for 150 rounds | 14-20 |
| Side B: group up before contested camps (always / side B only) | 17-17 / 17-17 |
| Side B: nobody starts a fight alone | 17-17 |
| Route anchor on our own half of the map | 16-18; side A still wins the mirror 11/17 |
| Support ping (3 versions: individual, team fight, teammate's manuscript) | 14-20 / 15-19 vs v64-bushward / no gain |

Side A wins about 70% of v70 vs v70 games: the side gap looks built into the game (turn order around
the first fight), not a strategy flaw.

## Notes
- Fuel per turn: actions 28%, decision log 22% (~11.5k; could be wrapped in `debug_block` to make it
  free on the platform, but the team chose not to for now), shared decision 10%, loot sharing 7%,
  respawn tracking 7%.
- The draft's spare fuel can do much more (it has 1M on its first turn and uses ~150-480k).
