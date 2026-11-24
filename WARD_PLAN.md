# Ward experiment plan (base: bots/v213)

Written 2026-11-03. Each experiment here goes to one subagent. **Every agent reads Part A (shared context) in full,
then its own brief in Part B.** Part C covers the order, the merge and what goes back to the user.

**The question.** Our ward is one charge that takes 10 turns to come back. Today most of it goes on vision that
changes nothing we do. Then, when an enemy we are fighting slips into a bush, the charge is gone. These experiments
make the ward a short-term tool: keep it for moments when it reveals an enemy we can hit, or one that is hitting us.
Each experiment is a flag on top of v213. With the flag off, the bot must play exactly as v213.

The user's framing, which every brief follows: our decisions are short-term, so vision of far-off enemies rarely
changes them. Ward for immediate gains, such as a bush an enemy was just seen entering. Fuel spent tracking enemies
in ways that don't change our action is waste.

---

## Part A: shared context (every agent)

### A1. Ward and vision rules (the facts these experiments rest on)

The full rules are in `CLAUDE.md` and `GameRules.md` (section "Vision & wards"). The parts that matter:

- **One ward per champion.**
  - It is placed within **2 tiles** (Chebyshev) of the champion and sees **6 tiles**.
  - Placing it costs no move, attack or cast. It can be done on any turn, before or after other actions.
  - Placing a new ward replaces the old one.
  - Wards are permanent: they never expire and can't be damaged or cleared by the enemy.
- **The charge recharges over 10 owner turn ends** (`bc::constants::ward_cooldown`; `me.ward_cooldown()` returns the
  rounds left). One ward every 10 turns at most.
- **Own sight is 8 tiles** (symmetric shadowcasting).
  - Walls block sight. Bushes block sight from outside.
  - Standing in a bush, you see across the whole connected bush cluster you are in, and out of it, but not into
    other bushes.
  - Holes don't block sight.
- **Geometry consequence: a ward next to you adds nothing in open ground** (2 + 6 = 8, the same as your own sight).
  It only pays in three ways:
  - seeing into a bush (a ward inside a bush sees across that bush, just as a champion does);
  - seeing around a wall corner;
  - staying behind after you walk away.
- **Vision is not shared.** Each champion sees only its own sight plus its own ward. Allies' wards and sightings are
  invisible to us. All 8 shared bytes are already used (`constants.h` ~1995-2010; byte 7 is `PING_BYTE`), so sharing
  sightings would cost an existing feature.
- **Hidden occupants.**
  - `place_ward(tile)` returns **false** when an unseen unit stands on the tile. The occupant is then revealed for
    the rest of the round, and the charge is kept.
  - A destination already known to be occupied throws `bc::error`.
  - The same rules apply to moves and dashes.
- **Enemies move between our turns.** Champion turns alternate teams within a round, and the starting team swaps
  each round. An enemy we see at our turn start has usually just acted.
- **Basic attack reach:** 1 (melee) or 3 (ranged). Spells reach 4. So every target we can hit is well inside our own
  8-tile sight, *unless* it is in a bush we aren't in, or behind a wall. **The ward only enables a hit in those
  cases.**

### A2. How v213 wards today (code map)

All paths are under `bots/v213/src/`. Line numbers are as of v213.

**Turn order of ward use** ([turn.cpp:147-159](bots/v213/src/turn.cpp#L147-L159)). These run after channels and lock
placement, before shopping and movement. The first that fires wins:

1. **`answer_hidden`** ([combat.cpp:2054](bots/v213/src/combat.cpp#L2054); flag `FIX_HIDDEN`, v71, constants.h ~999).
   - At turn start, `check_hidden` ([combat.cpp:2012](bots/v213/src/combat.cpp#L2012)) compares the health we lost
     since last turn (`remember_health`, combat.cpp:2002) with what the unseen camps in reach could have dealt. More
     than that means an enemy is hiding in a bush. The unseen bush tiles within `HIDDEN_REACH` (4) are remembered
     for `HIDDEN_ROUNDS` (2).
   - `answer_hidden` puts the ward on the nearest of those tiles in placement reach, trying the next tile on a
     refusal. If no charge or nothing is revealed, the champion steps out of reach of those bushes.
2. **`ghost_ward`** ([combat.cpp:1925](bots/v213/src/combat.cpp#L1925); flags `GHOST`, `GHOST_WARD`, v140,
   constants.h ~475-496).
   - `mark_ghost` ([combat.cpp:1857](bots/v213/src/combat.cpp#L1857), called at engage, combat.cpp:332) remembers
     the enemy we fight.
   - `ghost_where` ([combat.cpp:1873](bots/v213/src/combat.cpp#L1873)): once that enemy has been unseen for up to
     `GHOST_ROUNDS` (5), it guesses where the enemy went. The guess extends its last step; `ghost_hide` is every
     unseen tile it could stand on. The ghost is dropped when the enemy is dead, heads into its home chase ring,
     or every possible tile is in sight.
   - `ghost_ward` puts the ward on the placement tile in `ghost_hide` nearest the guess.
   - `chase_ghost` ([combat.cpp:1961](bots/v213/src/combat.cpp#L1961), called at turn.cpp:469) walks there.
3. **`scout_ward`** ([warding.cpp:298](bots/v213/src/warding.cpp#L298); flag `FIX_SCOUT_WARD`, v72, constants.h
   1071-1078). It runs on every turn the charge is ready.
   - It keeps a ward already in a bush within reach + 1 of us.
   - Otherwise it places one in either case:
     - **scout-bush:** an unseen bush tile is within 2 of us.
     - **catch-up:** `behind`, meaning we have no ward, or it is more than `SCOUT_REFRESH` (8) away
       ([warding.cpp:316](bots/v213/src/warding.cpp#L316)).
   - Candidates are the placement tiles, minus terrain and visible allied ward tiles.
   - Score: +`SCOUT_BUSH_SCORE` (100) for a hidden bush tile, +2 per tile away from us, −3 per tile from the route
     target's centre.
   - On a refusal (`place_ward` false) it calls `seen::reset()` and gives up for the turn. Exceptions are caught and
     return false silently.

**Dead code.** `FIX_SCOUT_WARD=true` and `SCOUT_KEEP_POSTS=false` make `scouting` always true, so these never run in
v213:
- `place_ward`, `bush_ward`, `follow_ward` and `open_ward_spots` (warding.cpp);
- `ward_detour` (gated at turn.cpp:592).

Leftovers that still cost something:
- `build_bush_boards` still runs once per champion at a turn end. It is gated by `BUSH_BOARD_FUEL` (15k) at
  [route.cpp:1278](bots/v213/src/route.cpp#L1278) and only feeds the dead `bush_ward`.
- `compute_ward_spots` still runs in the draft and writes the ward spots into the draft setup block
  (`encode_setup`, draft.cpp:166; decoded at draft.cpp:651 and route.cpp:161-166).
- `ghost_ward` and `answer_hidden` clear a bit of `WARD_MASK_BYTE` (= byte 7, the same byte as `PING_BYTE`)
  `if (ward_slot)`. Only the dead `place_ward` ever sets `ward_slot`, so this never fires.

**Related systems that read ward vision:**
- `seen::` caches each vision list once per turn and drops the cache after every action, a ward placement included
  (`trace::act` calls `seen::reset`; see `seen.h`). **So each placement or refusal makes the next reads re-fetch
  their lists.**
- The fight score ignores enemies more than `FIGHT_STEPS` (10) walking steps from us or from an ally within
  `FIGHT_LOCAL_RANGE` (12) (v123 FIGHT_LOCAL, constants.h ~1144). A far ward sighting is information only.
- `last_enemy_round` (bot.h:172) is updated in `update_enemies` ([phase.cpp:217](bots/v213/src/phase.cpp#L217)):
  - It covers any visible enemy champion, ward sightings and far-off ones included.
  - It only updates when `V86_ZONES && zones_known` (turn.cpp:78).
  - Check it before reusing it.
- `mode` (FARM / FIGHT / RETREAT / TRIBUTE) is read from shared memory in `sync_decision` (route.cpp:2025), before
  the ward block, so it holds this round's team decision there.

**History** (memory notes in `C:\Users\salty.THE-AQUARIUM\.claude\projects\c--Users-salty-THE-AQUARIUM-Desktop-Vinamilk-1--Workbench-Code-Cambridge-Battlecode\memory\`):
- v72 added `FIX_SCOUT_WARD` because "ladder teams move their ward almost every round; ours sat on fixed posts"
  (constants.h:1071). It copied what they did. The catch-up branch was never measured on its own.
- v140 (`ghost-finding.md`): residual vision gave +10.0% vs v138 over 3 seeds.
  - The ghost ward is about 3 points of that: with `GHOST_WARD` off the gain was +6.2% instead of +9.0%.
  - Five rounds was best; six lost.
- v71 `FIX_HIDDEN` (constants.h ~999) answers bush attackers with the ward or a step away.

### A3. The evidence: what the ward does in v213

Measured with `bots/zz-wardprobe`:
- v213 plus log-only probe lines; see `bots/zz-wardprobe/README.md` for every line it prints and the full table.
- 140 games vs v210 on all 70 maps, both sides. Replays are in `replays/bwardprobe/`.
- Record 71-68-1, banked +3.7%.

| Placement | Count | New tiles beyond own sight | Revealed an enemy at once | Hits that needed the ward, per placement |
|---|---|---|---|---|
| scout catch-up | 22,938 | 5.3 avg, **0 in 65%** | 0.6% | 0.008 |
| scout-bush | 14,034 | 50.5 | 8.4% | 0.092 |
| ghost | 402 | 49 | **70%** | **0.76** |
| hidden attacker | 98 | – | **83%** | **1.33** |

- **The charge is ready on only 51% of alive turns.**
- **Ghost chases:**
  - 3,240 in total. **92% (2,996) found the charge already spent, and 88% (2,838) never got a ward.**
  - The charge had been spent by catch-up 1,286, scout-bush 1,395, an earlier ghost 177 and hidden 54. The median
    was 5 rounds before the chase (p25 2, p75 7).
  - Every chase that found the charge ready did place a ward.
- **Hidden attacker:** 354 turns where we were hit from a bush with no charge, so we could only step away.
- **Hits needing the ward:**
  - 6.8% of our hits on enemy champions were on targets outside our own sight (1,898 of 28,051).
  - By the owner's latest placement: scout-bush 1,289, ghost 304, catch-up 175, hidden 130.
- **Scout refusals:**
  - 12,296 in total. **73% came from a tile the same champion had already been refused on 5+ times in that game**
    (up to 144).
  - This is probably a camp standing in a bush, retried on every ready turn. Each retry costs an engine call and
    drops the vision cache.

Caveats an agent should keep in mind:
- **The ghost count is an upper bound.** A blocked chase is counted when the target is unseen and within 5 rounds.
  Some would later have been dropped for other reasons: the guess heading into the enemy's home ring, or every tile
  it could be on already in sight.
- **Attribution is approximate.** Each ward-needed hit is credited to the owner's latest placement.
- **Own sight must be measured with `champion::visible_tiles_from(position)`.** `champion::visible_tiles()` seemed to
  include the ward and reported zero ward-only sightings.

### A4. Sources, and what carries over to us

- **WARDS: Modelling the Worth of Vision in MOBAs.** Chitayat, Kokkinakis, Patra et al., SAI Computing Conference
  2020, Springer, DOI 10.1007/978-3-030-52246-9_5. PDF: https://eprints.whiterose.ac.uk/170119/1/WARDS_SAI_3_.pdf
  - Experts were interviewed about what makes a ward valuable.
  - A ward's score is its lifetime plus the enemy heroes, items and levels it detects, all scaled by an "optimality"
    term. Optimality covers vision area and points of interest, minus a penalty for overlap with other friendly
    wards.
  - The total is multiplied by **consequence kills**: kills of a hero within 45 s of this ward detecting it.
  - **For us:** a ward earns its value when it detects an enemy we then hit or kill, soon. A ward that overlaps
    vision we already have is penalised. Our "hits that needed the ward" is the in-game version of a consequence
    kill.
- **Dispersing the fog of war: a new approach to ward evaluation in MOBAs.** White paper on the same work.
  https://gdlt.sdu.dk/wp-content/uploads/2022/06/White-Paper_Dispersing-the-fog-of-war_-A-new-approach-to-ward-evaluation-in-MOBAs-.pdf
  - Riot's Vision Score rewards ward lifetime but gives **fewer points to a ward placed near other allied vision**.
  - It also lowers a ward's value "if it has not spotted any key units … in a while".
  - **For us:** a catch-up ward 2 tiles from us is the redundant case.
- **Riot Vision Score** (community descriptions; the formula is not official):
  - https://wiki.leagueoflegends.com/en-us/Vision_score
  - https://blog.loltheory.gg/vision-score-lol/
  - Redundancy and "pointless" ward penalties are the same idea as above.
- **Coaching guides:**
  - Dignitas, Warding 101: https://dignitas.gg/articles/warding-101-comprehensive-guide-to-effective-warding
  - Dignitas, what your wards tell the enemy: https://dignitas.gg/articles/what-your-wards-tell-the-enemy-team
  - wecoach, Challenger vision control: https://wecoach.gg/blog/article/ward-like-a-pro-challenger-league-of-legends-vision-control-ward-guide
  - What they say: defensive wards (own side, objectives) are safe; offensive wards are risky but can swing a game.
    Ward the brush on the paths into your lane. Hold specific spots before contested objectives. Pick placement by
    game state.
  - **For us:** the short-term half applies:
    - ward the bush you are about to walk past;
    - ward the bush an enemy just entered;
    - hold vision at the camp you are about to fight at.
  - The long-term half (deep wards, map control, denying enemy vision) doesn't. Our wards can't be cleared, and our
    decisions don't look far ahead.
- **A Particle Model for State Estimation in Real-Time Strategy Games.** Weber, Mateas & Jhala, AIIDE 2011.
  https://ojs.aaai.org/index.php/AIIDE/article/view/12424
  - Particles estimate where enemy units seen earlier have gone, with parameters learned from expert replays.
  - It gave their StarCraft bot (EISBot) a 10% gain over baselines.
  - **For us:** our ghost is a one-particle version (5 rounds, extend the last step), and it already gains about
    +10% (A2 history). The data says it is limited by having no charge (92% blocked), not by how good the guess is.
    A richer model would cost fuel for little gain.
- **OpenAI Five benchmark results.** https://openai.com/blog/openai-five-benchmark-results/
  - Five "would often drop wards seemingly in the direction it was trying to go". OpenAI guessed it was spending
    its capacity on movement instead.
  - That is close to our catch-up ward. It is not a pattern worth copying.
- **Off The Beaten Lane: AI challenges in MOBAs beyond player control.** https://arxiv.org/pdf/1706.03122
  - Names ward placement as an open problem.
- **Expected value of information (decision theory; standard idea, no single source):**
  - Information is worth something only if it could change the best action.
  - Active-perception work in robotics trades the information a sensing action gains against its cost (motion,
    energy). Example: Bonn humanoid robotics notes,
    https://hrl.uni-bonn.de/teaching/ss26/humanoid-robotics/hr03_perception_3.pdf/@@download/file
  - **For us:** the ward charge has an opportunity cost of 10 turns. Spend it where the reveal can change this turn's
    or the next few turns' action.

### A5. Hard constraints

- **Fuel** (CLAUDE.md, the user's priority): report mean fuel per turn and out-of-fuel turns against v213 for every
  variant.
  - All log text goes through the `LOG` / `WHY` / `NOTE` / `SUB` macros (inside `debug_block`), never a bare
    `note(format(...))`.
  - Measured costs (memory `fuel-cost-model.md`):
    - champion reads (`position()`, `health()`, `ward_cooldown()`): about 100 each, engine reads;
    - a new bitboard: 210-310; an op into an existing board: about 100;
    - a guest statement: about 10; a printed line: 1-1.5k.
  - Each ward placement or refusal drops the `seen::` cache.
  - Hoist reads out of loops and reuse boards (`*_into` calls, static scratch boards, as `scout_ward` already does).
  - Build static boards once in setup.
- **Base rule:** with your flag off, the build plays exactly as v213. Check once: flag off vs v213 on a short bench
  should give identical results (same winners, same gems).
- **No team coordination is needed.** Wards and vision are per champion, so nothing has to agree across bots. Don't
  add shared-memory traffic: all 8 bytes are taken.

### A6. Workflow

1. **Make your folder.** `python bots/v133t/bench.py variant v213 v214-ward-<tag>` copies v213 (it skips `.cambc` and
   `*.py`) and can set existing constants with `FLAG=VALUE`.
   - The farm-split plan uses `v214-<tag>` names, so keep the `ward-` part.
   - If you copy by hand, delete `.cambc/` in the copy.
2. **Add the probe before you edit.** From the repo root:
   `git apply -p2 --directory=bots/v214-ward-<tag> bots/zz-wardprobe/probe.patch`
   - It touches `scout_ward`, `ghost_ward` and `answer_hidden`, so apply it first and edit on top.
   - It also removes the per-turn decision log (`LOG(log_turn(me))` in turn.cpp) to keep logs under 1 MB. Put it
     back if you need the decision log for a debug game.
   - Every probe statement is inside `LOG`: zero fuel in debug builds, absent in release.
   - Add probe lines for your own mechanism the same way.
3. **Code style:** follow `constants.h`. Each new constant gets a comment block: version (v214-ward-<tag>), what it
   does, why (with the A3 numbers), and later its results. Put switches near related code: ward constants are at
   constants.h ~1071 and ~1685.
4. **Smoke test:** one game, `cambc run bots/v214-ward-<tag> bots/v210 --map 00ambush --log-limit 40000000 -o <dir>`.
   Unzip `team-a.stdout` from the `.repl26a` (a zip) and read your lines.
5. **Bench:**
   - Opponents: v213 (mirror), v210, v208.
   - Timing seeds: `python bots/v133t/bench.py seed <bot> N` (N = 1, 2). Run seeds 0-2 for **both** v213 and your
     variant. A seed copy burns `300*N` fuel at turn end to shake timing.
   - Manifest: `python bots/v133t/bench.py manifest <OUT> <OPPONENT> <bot>[,<bot>...]` covers every
     `maps/*.map26a` on both sides (the user's rule: all 70 maps).
   - Run: `cambc run-batch b<OUT>.txt --jobs N --json` from the repo root.
   - The machine has 20 cores, and other agents may be benchmarking: use `--jobs 5`.
6. **Noise** (memory `benchmarking.md`): one 140-game run swings about ±10% banked; three pooled seeds about ±3.5%.
   Treat smaller effects as noise unless the mechanism shows in the probe numbers.
7. **Metrics to report** (variant vs v213, same opponents and seeds):
   - `python bots/v133t/bench.py summary DIR...`: record, banked, K/D, camp deaths, out-of-fuel turns.
   - Banked by r300 / r600: `bots/v114/analyze_gold.py`.
   - Mean fuel per turn: `bots/v114/fuel_bench.py`, `fuel_compare.py`.
   - **Ward metrics:** `python -I bots/zz-wardprobe/wp_analyze.py <replay dir>`:
     - placements by kind;
     - charge-ready share;
     - ghost chases blocked / never warded;
     - hidden-attacker blocks;
     - hits needing the ward;
     - refusals and repeats.
   - The probe is only in your build, so ward metrics come from your variant's replays. For the v213 side, use the
     A3 baseline, or run `bots/zz-wardprobe` itself with the same seed and opponent.
8. **Deliverables:**
   - `bots/v214-ward-<tag>/README.md` in the style of `bots/v213/README.md`: what changed, why, the bench table,
     the ward metrics, and what was tried and dropped.
   - A short report back with the table, the decision you recommend, and anything in this plan you found wrong.
9. **Do not** commit, push, touch git state, or run `cambc submit` / `submissions activate` / `match-groups create`.
   As of writing, the working tree has a merge in progress (`UU .gitignore`); leave it alone.

---

## Part B: experiment briefs

Each brief has: sources, motivation, hypothesis, design, code placement, flags, fuel, risks, test, success
criteria, and open questions for you to resolve.

### W1. Stop catch-up wards. Agent: "catchup". Flag `SCOUT_CATCH`

**Sources.**
- WARDS overlap penalty and Riot's redundancy penalty (A4): a ward inside vision we already have is worth little.
- The OpenAI Five note (A4): dropping wards toward where you are going is a habit, not a strategy.

**Motivation (A3).**
- Catch-up is 61% of all placements.
- Two-thirds of catch-up wards add **zero** tiles beyond our own sight, and only 0.6% reveal an enemy.
- They produced 0.008 ward-needed hits each, against 0.76 for a ghost ward.
- They spent the charge in 1,286 of the 2,996 blocked ghost chases.
- In v213 they exist because v72 copied ladder teams that move their ward often. Nobody measured them.

**Hypothesis.** Without catch-up, the charge is ready more often, ghost and hidden wards fire more, and kills and K/D
rise. Fuel falls slightly: fewer `place_ward` calls and fewer vision-cache drops.

**Design.**
- In `scout_ward` ([warding.cpp:316](bots/v213/src/warding.cpp#L316)), make `behind` depend on the flag:
  `const bool behind = SCOUT_CATCH && (own.empty() || distance(position, own.front()) > SCOUT_REFRESH);`
  - With `behind` false, the function leaves at the next check unless an unseen bush is within reach.
  - That early exit also skips the placement-tile and ward-tile reads, which saves fuel.
- `SCOUT_CATCH = true` reproduces v213 (base rule check).
- Variant (b): instead of off, a higher `SCOUT_REFRESH`, for example 16.

**Things to check first:**
- Does a champion's ward survive its death? If it doesn't, `own.empty()` after a respawn currently triggers a ward
  on the spawn (the r1 placements in the probe have gain 0).
- With the flag off, a champion has no ward until its first bush. Is anything else relying on having a ward
  somewhere? grep for `ward_positions()` and `ward_tiles(`.

**Flags.** `SCOUT_CATCH` (bool), and optionally a `SCOUT_REFRESH` sweep.

**Fuel.** Expect slightly lower mean fuel per turn. Report it.

**Risks.**
- Some catch-up wards help by staying behind us and watching where we came from: 175 ward-needed hits (9% of all
  ward-enabled hits).
- Wall-corner value is invisible in the gain average. Check the gain>0 catch-up cases.

**Test.**
- Base rule check: `SCOUT_CATCH=true` vs v213.
- Then 3 seeds × {v213, v210, v208}, flag off vs v213.
- Ward metrics: catch-up placements should drop to about 0. Watch the ghost-blocked share (92% baseline) and
  ward-needed hits (1,898 per 140 games baseline).

**Success.**
- Pooled banked ≥ +3.5% over v213, or a K/D gain with banked level and the mechanism showing in the probe numbers
  (ghost-blocked share down, ghost placements and ward-needed hits up).
- No rise in out-of-fuel turns.

**Open questions.**
- Should catch-up stay allowed when it gains ≥ K tiles? Measuring gain needs `bc::ward::visible_tiles_from(tile)`
  for the best candidate only: one engine read, and only on catch-up turns. Try it only if plain off loses.

### W2. Hold the charge while a fight is likely. Agent: "hold". Flags `WARD_HOLD`, `WARD_HOLD_ROUNDS`, `WARD_HOLD_RANGE`

**Sources.**
- Expected value of information and active perception (A4): spend a sensing action where it can change the next
  action, and weigh its cost. The charge's cost is 10 turns of being blind into bushes.
- WARDS consequence kills (A4): value comes from detections shortly before kills, so the charge should be available
  in the seconds around a fight.
- The coaching rule "ward before the objective you are about to fight at": vision timed to the fight, not spent
  just before it.

**Motivation (A3).**
- The ghost ward reveals 70% of the time and the hidden-attacker ward 83%: about 8× and 14× a scout-bush ward per
  charge.
- 92% of ghost chases found the charge spent, mostly by scout wards placed a median 5 rounds before. That is the
  run-up to a fight, when enemies are near.
- 354 hidden-attacker turns had no charge.

**Hypothesis.** If `scout_ward` doesn't spend the charge while a fight is likely, the ghost and hidden wards fire
when they matter. Kills and K/D rise, and fewer of our champions are run down from bushes.

**Design.**
- At the top of `scout_ward`, after the cheap cooldown check, return false (keep the charge) if any of:
  - `mode == FIGHT`;
  - an enemy champion in `seen::positions(Team::enemy)` is within `WARD_HOLD_RANGE` (start at 10) of
    `seen::self_position()`;
  - `round - last_near_enemy_round < WARD_HOLD_ROUNDS`, where `last_near_enemy_round` is a new member set whenever
    the previous condition is true.
- Use your own `last_near_enemy_round`, not `last_enemy_round`. The existing one counts far-off ward sightings and
  only updates when `V86_ZONES && zones_known` (A2).
- Leave `ghost_ward` and `answer_hidden` unchanged: they are what the held charge is for.
- `WARD_HOLD=false` reproduces v213.
- Sweep: `WARD_HOLD_ROUNDS` = 0 (fight mode / enemy in range only), 5, 10.

**Measure what you give up.**
- Extend the probe's `place scout-...` line with whether an enemy was in range or `mode == FIGHT` at placement.
- In v213, some of scout-bush's 1,182 reveals and 1,289 ward-enabled hits happened in exactly these moments. The
  hold trades those for ghost and hidden wards.
- Report both sides of the trade.

**Flags.** `WARD_HOLD` (bool), `WARD_HOLD_ROUNDS` (int), `WARD_HOLD_RANGE` (int).

**Fuel.** One loop over the visible enemy positions (already cached) and an integer compare. Fewer placements. Expect
level or lower.

**Risks.**
- A second enemy waiting in a bush beside a visible one goes unwarded until it hits us. `answer_hidden` covers that
  with a turn's delay.
- A hold that is too long (10) may leave champions with a ready charge and nothing to spend it on. That costs
  nothing unless scout-bush reveals were doing real work; the probe tells you.

**Test.**
- Base rule check, then 3 seeds × 3 opponents for each `WARD_HOLD_ROUNDS` value.
- If W1 has a result by then, also bench the best W2 setting stacked on W1's winner. Memory `small-map-plan` says
  winning switches don't always add up.

**Success.** As W1. Also the ghost-blocked share well below the 92% baseline, and more ghost placements per game.

**Open questions.**
- Should Willow (backline healer, never chases ghosts; `mark_ghost` excludes her) be exempt from the hold?
- Should the hold apply in RETREAT mode? A retreating champion's ghost isn't chased, but `answer_hidden` still
  matters.
- Is `WARD_HOLD_RANGE` better as walking steps? Probably not worth the fuel: use Chebyshev distance.

### W3. Stop retrying occupied tiles. Agent: "mask". Flag `SCOUT_MASK`

**Sources.**
- The rules on hidden occupants (A1): a refusal returns false, reveals the occupant for the round and keeps the
  charge.
- The fuel model (A5): each refusal is an engine call plus a vision-cache drop.

**Motivation (A3).**
- 12,296 scout refusals per 140 games, 73% from a tile the same champion had already been refused on 5+ times in
  that game (up to 144).
- Each time, `scout_ward` gives up for the turn ([warding.cpp:355-358](bots/v213/src/warding.cpp#L355-L358)), so it
  never tries the next-best bush.
- `ghost_ward` and `answer_hidden` handle refusals differently (A2). Leave them alone: for them, a refusal is a useful
  reveal.

**Step 1: measure (no behaviour change).**
- Classify each refused tile:
  - a camp spawn tile, from the static spawn tiles (`bc::map::camp_groups()` → `spawn_tiles()`; the bot already
    walks them in `trace.cpp`'s `camp_spawns()` and in `route`);
  - or not, which probably means an enemy champion. Check whether `seen::champions(Team::enemy)` after the refusal
    has a champion on that tile.
- Also count `bc::error` throws in `scout_ward`'s `place_ward` (caught silently today). A throw means a tile already
  known to be occupied. Visible champions and camps aren't masked from the candidates, only terrain and ally wards.
- Expectation: repeat refusals are camps in bushes. A refusal that reveals an enemy champion is *valuable*: it is a
  free reveal, so don't mask those.

**Step 2: design.**
- Build a static board of camp spawn tiles once (setup, or first use).
- With `SCOUT_MASK`, remove those tiles from `scout_ward`'s candidates: `tiles &= ~camp_spawn_board` next to the
  existing `part` masking at warding.cpp:324-328.
- If step 1 shows many throws, also mask the tiles of visible champions and camps
  (`bc::vision::champion_tiles_into`, `camp_tiles_into` into the existing scratch boards).
- Cost of masking all camp spawn tiles: we can't ward a cleared camp's spawn tile that sits in a bush. Neighbouring
  bush tiles do the same job.

**Flags.** `SCOUT_MASK` (bool).

**Fuel.**
- One board op per scout turn (about 100) against about 88 refusals per game, each an engine call plus re-reads.
- Expect lower. Measure with `fuel_compare.py`, and also count fewer refusal lines.

**Risks.**
- **Interaction with the charge:** a refusal keeps the charge, so masking makes scout placements *succeed* more
  often, which spends the charge more. Alone, W3 may make W2's problem worse.
- So bench W3 alone, and stacked on the W1 / W2 winners when they exist. Report both.

**Success.** Refusals drop by about 70%+, fuel level or lower, results level or better. If alone it is level and
stacked it is positive, recommend it only as part of the stack.

**Open question.** Do any refusals happen on tiles of camps outside bushes (in sight, so a throw rather than false)?
That would point to a candidate-mask bug.

### W4. Remove the dead fixed-post ward code. Agent: "cleanup". Flag: none (code removal)

**Motivation (A2).**
- `build_bush_boards` (up to 15k fuel once per champion) and the draft's ward spots only feed code that never runs.
- `place_ward`, `bush_ward`, `follow_ward`, `ward_detour`, `open_ward_spots` and the `WARD_MASK_BYTE` writes are
  unreachable with `FIX_SCOUT_WARD=true`, `SCOUT_KEEP_POSTS=false`.
- That mask byte is the same byte as `PING_BYTE` (constants.h:1361, :2005; the static_assert at :1366). Leaving the
  dead writes in is a trap for anyone who flips `SCOUT_KEEP_POSTS`.

**Design.**
- Remove the dead functions, their members (bot.h ~360-374), the `ward_detour` call (turn.cpp:592), the
  `ward_slot` / `WARD_MASK_BYTE` clean-up in `ghost_ward` and `answer_hidden`, and the `build_bush_boards` call
  (route.cpp:1278).
- Remove the ward-spot section from the draft setup block, keeping encode and decode consistent:
  - `encode_setup` (draft.cpp:166);
  - the decode at draft.cpp:651;
  - `ward_spots` in `draft.h:87` and route.cpp:161-166.
  - The layout is `SETUP_MAGIC`, spot count, spots, altar count, altar flags. Drop the spot part on both sides and
    check the magic / length check still rejects stale data.
- Keep `scout_ward`'s `SCOUT_KEEP_POSTS` branch out too, or leave the constant with a comment saying the posts were
  removed in v214-ward-cleanup.

**Test.**
- Decisions must be unchanged. Only fuel timing may differ.
- Run v214-ward-cleanup and v213 against the same opponent with a huge fuel budget
  (`cambc run-batch ... --fuel-quantum 100000000`), so no turn runs out. Results should match game for game. Any
  difference is a bug.
- Then a normal 3-seed bench for fuel: mean fuel per turn and out-of-fuel turns. Expect a small drop on the first
  turns after setup.

**Success.** Identical decisions at unlimited fuel, fuel level or lower. Low priority: do it last, or let the merge
fold it in.

### Considered and not planned (with reasons, so nobody re-derives them)

- **Choosing the ghost ward tile by how much of `ghost_hide` it reveals.** It would need
  `ward::visible_tiles_from` per candidate (up to 32 engine reads). The current nearest-to-guess rule already
  reveals 70% of the time. The bottleneck is the charge, not the tile.
- **Sharing ward sightings with allies.** Vision isn't shared and all 8 shared bytes are used (A1). It would cost an
  existing feature, for information the fight score mostly ignores beyond 10 steps (A2).
- **Deeper enemy-position models (particle filters, DefogGAN-style prediction).** These are the Weber et al. line of
  work (A4). They cost fuel, and the cheap one-guess ghost already gains about +10%. Nothing says the guess is the
  limit.
- **Long-range vision posts (centre, enemy spawn ring).** This is the old v72-era post system. Our decisions are
  short-term, and far sightings mostly aren't acted on (FIGHT_LOCAL).
- **Using enemy wards' positions** (for example, not ambushing from a bush they watch). Possible, but there is no
  evidence it matters. Revisit only if replays show our bush ambushes being spotted.

---

## Part C: order, merge and hand-back

**Order and parallelism.**
- W1, W2 and W3 can start at once on v213. W4 is independent and lowest priority.
- W3 and the best W2 setting should each be re-benched on top of the W1 winner when it exists. Stacking may not add
  (memory `small-map-plan`).
- All agents share one machine: `--jobs 5` each (A6.5).

**Merge (main session, not a subagent).**
1. Combine the kept flags (plus W4 if it passed) into the next free version number. Bench it against v213 and each
   single winner, 3 seeds × 3 opponents.
2. Write its README with the ward metrics next to the A3 baseline.
3. Save a memory note per finding with numbers. Update `ward-probe-finding.md`, the baseline this plan comes from.

**What goes back to the user.**
- The bench tables, the ward metrics, and the recommended stack.
- Local wins have overstated ladder results before (memory `ladder-behaviour-finding`, `platform-testing`). A
  platform test on the standing 10-opponent set is the user's call, since submitting, activating and queuing use
  quota or change the live bot. Propose it and wait.
