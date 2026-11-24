# Farm-split experiment plan (base: bots/v213)

Written 2026-11-03. This file hands one experiment each to one subagent. **Every agent reads Part A (shared
context) in full, then its own brief in Part B.** Part C covers the order, the merge and what goes back to the user.

The question behind it: top teams farm several camps at once with champions that are built for it, and we keep the
team together most of the time. These experiments test ideas from published work on multi-agent routing, task
allocation and combat modelling, adapted to our bot. Each one is a flag on top of v213. With the flag off, the bot must
play exactly as v213.

---

## Part A: shared context (every agent)

### A1. What the game rewards (only the rules these experiments touch)

The full rules are in `CLAUDE.md` and `GameRules.md`. The facts that matter here:

- **Camps** (health / strength per round / attack range / gold / gems / respawn rounds):
  - Wolf 1350 / 20 / 3 / 40 / 4 / 40
  - Elk 2400 / 60 / 4 / 90 / 15 / 60
  - Golem 5000 / 120 / 4 / 160 / 36 / 80
  - Gem cluster 120 / 0 / - / 0 / 7 / 50
  - Gold cluster 120 / 0 / - / 20 / 0 / 50
  - Monolith 20000 / 200 / 5 / 300 / 200 / 150
- A group is a set of cardinally connected camps. Damage wakes the whole group, and it attacks the nearest champion in
  range every round. Camps never heal and never chase. A group falls asleep after 2 rounds with no champion in range.
- **Capture:** stand on a cleared group's tile, uncontested, for 5 rounds (3 with an allied Capture emblem). The reward
  is split among the allies present. The group's respawn timer only starts once its reward is captured.
- **Death:** out for 60 rounds. The killer takes every gem you carry plus 100 gold. If a camp kills you, the gems are
  destroyed.
- **Recall:** 5 turn ends (3 with Home). Damage cancels it. Standing on spawn heals to full, and you can shop there.
- **Shards** cost 30 gold, and 5 of a family make an emblem with a passive:
  - Jungle: 30% less camp damage taken.
  - Monster: 15% more damage dealt to camps.
  - Durability: 10% less damage taken from everything.
  - Capture: capture takes 3 rounds.
  - Treasure: 10% more gold.
- **Fuel:** 100k per turn in C++, 2x on the first play turn, 100x on the first draft turn. A bot that runs out is
  suspended mid-turn and resumes next round.
- **Vision is not shared.** Each champion sees 8 tiles plus its own ward. The team shares **8 bytes** of memory during
  the match, plus 1024 read-only bytes written in the draft.

### A2. The problem, with the evidence we already have

Read these before designing anything. Several split attempts have already failed, and each failure says why.

1. **Top teams farm in parallel; we don't.**
   - `results/fish-farming-study.txt` (branch `vargvale-strikes-back`) compared Fish (top ladder team) with v170:
     - 2+ squads farming at once: 21.7% vs 5.8% of farming rounds.
     - Standing still near camps: 0.9% vs 9.1%.
     - Captures by r300: 13 vs 9.
     - Team emblems by r400: 11 vs 6.
   - Fish's squad rule: "the fewest champions whose expected camp damage stays under ~0.3-0.5 of their health". It
     sends solo wolves from about r240 and solo elks from about r330. It never sends a solo or duo to the enemy side.
     It opens together, then splits after about 3 captures, with solos about 17 tiles from the core.
2. **We are about as fast per farming round; we just farm fewer rounds.** Captured gems per champion-round spent
   hitting or capturing: Fish 0.81, v170 0.82. The gap is time spent walking together, waiting and fighting.
3. **v213's own measurement:** 12.5% of champion turns were in farm.wait, standing by a respawning group (v213
   README, FILL_WIDE).
4. **The study behind v202 SPREAD** (comment at the top of `bots/v213/src/constants.h`):
   - Top-50 teams run 2-3 farming units, with the whole team in one cluster 21-27% of rounds against our 45%.
   - Their lone champions mostly hit wolves (~50%) and elks (24-32%), a median 15 tiles from the main group, and do
     not die more often when alone.
5. **Past split attempts and why they lost:**
   - **BUSY_JOBS** (vargvale lineage v202, Fish-style spare-champion jobs): -23.8% in its Fish-like form, -7.0% with
     safety gates. *Split champions were picked off by raids*: the split side was outnumbered in 51% of rounds against
     17%. Fish's splitting works inside Fish's whole system, and ours would need regroup and fight rules built for it.
   - **v202 SPREAD** (our lineage, now `SPREAD = false` in v213): captures flat, camp gems -13%. The solos traded the
     team's elk captures for cheap solo groups. v203 added SPREAD_EDGE, and it is still off.
   - **SATELLITE** (v159, live in v213): stage 1 went 130 of 280 vs 152 off; stage 2 (middle zone) 153 of 280. It works
     but gains little, because it only takes clusters or a single wolf. **SAT_ELK never fired**: a solo elk costs about
     1,800 health, more than SAT_ELK_TAKE of a 1,200-health satellite.
   - **v192 multifarm** (vargvale, Vesper satellite): not adopted.
   - **Ladder** (vargvale, 2026-11-03): v202 and v203 farmed faster early but died 13-15 times a game against v209's 9,
     which gave the whole-game farming rate back. *Survival is the binding constraint, not farm rate.*
6. **Ladder vs local:** we lose about 80% banked to the top 4 teams (`results/results-v185-v191-top50.txt`).
   Self-play has misled before (memory: score-sweep and ladder-behaviour findings): the Vesper funnel won self-play
   but leaves the frontline without items on the ladder.

**What this means for every brief:** a split gets better only when it adds farm rounds *and* the split champion does
not die more. Every experiment reports deaths, especially the split champion's, next to farm numbers.

### A3. How v213 farms today (code map)

All paths are under `bots/v213/src/`. Read the functions named for your brief, not just this summary.

- **One shared target.** A leader picks a group (`pick_target`, `route.cpp:1651`) and writes it to shared memory. Its
  rate is `route_rate_steps` (`route.cpp:1353`): `values[aggressive][s] / (steps + respawn wait + clears[s] +
  capture_rounds)`.
  - **`clears[s]` uses a fixed TEAM_DAMAGE of 200 a round** (`draft.cpp:100`, `draft.cpp:510`), whoever is present.
    No team rate today depends on how many champions are there. E1 and E2 must change that locally.
- **FIX_FILL / FILL_WIDE:** while the goal is respawning, a filler group is taken if its gems beat what the detour
  costs the goal (`route.cpp:1725-1786`).
- **Zones** are set in the draft: `zone(s)` is SAFE, UNCERTAIN or UNSAFE by walking steps from each spawn
  (ZONE_SIDE 1.5). The phase sets which zones are open: `choose_phase`, `phase.cpp:575`.
- **The live split is SATELLITE** (`constants.h:1531-1590`).
  - One fixed champion: the first of SAT_KINDS = Marcella, Ryth, HoR in the roster. Only on teams of 4+ without
    Cassi & Issac, never on the Prism team.
  - In practice: Marcella on ROSTER_5, Ryth on ROSTER_4 and ROSTER_5_VARG / ROSTER_5_PRISM, HoR on ROSTER_4_PRISM
    (rosters at `draft.cpp:24-38`).
  - Opening gates (`phase.cpp:613`): FARM mode, centre open, no enemy for SAT_QUIET 3 rounds, no ping, 10-round
    cooldown, and `sat_ready()` (alive, not collector or carrier, bag under 20, at least 60% health when in sight).
  - Candidate groups (`split_candidate`, `phase.cpp:533`): `sat_ok` (clusters only, or wolves within
    `strength*health/60 <= 500`, which is a single wolf; `route.cpp:143`), SAT_APART 6 to SAT_RANGE 20 from the main
    target, up on arrival. It must pay at least `SAT_EDGE x main rate / allies_alive`.
  - Picks while out (`sat_pick`, `phase.cpp:116`): value / (walk + own clear + capture). Own damage =
    `strength + 0.25*focus`. Skips groups whose camps would deal more than SAT_TAKE 0.5 of *current* health. The
    current group gets SAT_KEEP x1.25.
  - Ends (`end_split`, `phase.cpp:715`; the checks are in `play_pair`, `phase.cpp:729`): enemy in sight, bag needs an
    escort, team in FIGHT, health under 60%, bag of 20 or more, no group left. The satellite's group goes in byte 5
    bits 0-5 (`target2`), and the main team stays off it (`phase.cpp:978`, `route.cpp:1917`).
- **Dead code in v213:** the V86 Cassi & Issac pair split never runs, because no live roster drafts them. SPREAD,
  SPLIT_SMALL and PRISM_SAT are all off.
- **Reusable pieces in `spread.cpp`** (v202 SPREAD, off but compiled):
  - `spread_clear(s, damage, rounds)`: damage taken clearing a group alone, killing the weakest camp first, each camp
    hitting until it dies. This is the Lanchester duel in E2.
  - `spread_mark` / `spread_danger`: a ring of enemy sightings and ally deaths. E3 can build on it.
  - `spread_estimate`: the leader's estimate of a solo's stats from base stats plus our emblem record.
- **Other state you will need** (`bot.h`):
  - `ready_at[s]`: the round group s is up. Each champion learns it from its own sight (`route.cpp:505-525`) and from
    CAPTURE_NEWS (`announce_capture`), so **champions can disagree**.
  - `enemy_at[]` / `enemy_at_round[]` (last enemy tiles), `last_enemy_round`, `emblems` (our emblem record per
    champion), `emblem_count(me)`, `owned_of(me).passives`, `emblem_lead()`, `allies_alive()`, `gap(a, b)` (cached
    group-to-group steps), `travel(origin, skip)` (steps table to every group).
- **Shared memory is full.** Bytes 0-6 hold mode, target, capture slot, collector, stamp, phase+target2 and claim
  round; byte 7 is the ward mask (`constants.h:1994-2010`). A new idea must either work from facts every champion
  already has (the same answer computed independently), or reuse an existing field. Do not widen shared memory without
  showing what you take bits from.
- **Shop builds** (`constants.h:510-590`): Marcella buys Treasure, Monster, Durability, Tribute, then Jungle. Ryth
  buys Jungle, Monster, Durability. HoR starts with Capture, then Jungle. VESPER_FUNNEL_EMBLEMS 12 sends gold to
  Vesper first.

### A4. Hard constraints

- **Fuel** (from CLAUDE.md, user's priority): report mean fuel per turn and out-of-fuel turns against v213 for every
  variant.
  - All log text goes through the LOG / WHY / NOTE / SUB macros (inside `debug_block`), never a bare
    `note(format(...))`.
  - Measured costs: champion reads (`position()`, `health()`) are about 100 each and go through the engine;
    `base_stats` about 220; a new `distance_field` about 1,550; a printed line 1-1.5k; a guest statement about 10.
  - Hoist reads out of loops, reuse buffers, and never loop over a goal bitboard's tiles per move.
  - Expensive precomputation belongs in the draft (100x fuel) or the first play turn (2x), stored in the 1024 draft
    bytes only if every champion needs it.
- **Determinism without messages:** if your idea assigns champions to groups, each champion must compute the same
  answer from the same facts: roster kinds, map, draft memory, our emblem record, the shared bytes. Champion
  positions, health and `ready_at` are *not* common knowledge. Say how your design handles that.
- **Base rule:** with your flag off, the build plays exactly as v213. Check this once with a short bench: the off
  build against v213 should give identical results.

### A5. Workflow

1. **Make your folder:** `python bots/v133t/bench.py variant v213 v214-<tag> FLAG=VALUE` copies v213 and sets
   constants (it skips `.cambc` and `*.py`). If you copy by hand, delete `.cambc/` in the copy.
2. **Code style:** follow `constants.h`. Each new constant gets a comment block saying which version added it, what it
   does, why (with numbers), and later the results. Put switches near the code they relate to.
3. **Build and smoke-test:** run one game (`cambc run`) and read the bot's debug log for your NOTE lines before
   benchmarking.
4. **Opponents and seeds:**
   - Pool: v213 (mirror), v210, v208, and v185 if you can get it:
     `git archive vargvale-strikes-back bots/v185 | tar -x -C .` (check that `bots/v185` does not exist first).
   - The v213 README also used `proxy-farmer`. It is not in this checkout; skip it unless the user gives you a path.
   - Seeds: `python bots/v133t/bench.py seed <bot> N` makes timing seeds (N = 1, 2). Run seeds 0-2 for **both** v213
     and your variant.
   - Manifest and run: `python bots/v133t/bench.py manifest <OUT> <OPPONENT> <bot>[,<bot>...]` covers every
     `maps/*.map26a` on both sides (the user's rule: all maps in `maps/`, currently 70). Then
     `cambc run-batch b<OUT>.txt --jobs N --json`.
   - The machine has 20 cores. If other agents may be benchmarking at the same time, use `--jobs 5`.
5. **Noise:** one 140-game run swings about ±10% banked. Three pooled seeds are about ±3.5%. Treat smaller effects as
   noise unless a mechanism shows in the logs.
6. **Metrics to report** (variant vs v213, same opponents and seeds):
   - Record; banked (mean and by r300 / r600; see `bots/v114/analyze_gold.py`); captures, and captures by r300;
     camp gems; team emblems by r400.
   - Deaths, split into champion kills and camp deaths (`bots/v114/analyze_camps.py`); kills; **deaths of the split
     champion**.
   - Mean fuel per turn and out-of-fuel turns (`bots/v114/fuel_bench.py`, `fuel_compare.py`; also `fuelExhausted`
     events in the stats JSON).
   - **Split activity:** share of the satellite's turns spent split; split ends counted by reason (the
     `split ends: <why>` NOTE lines in debug logs). For E1/E4 also the farm.wait share: find the WHY stem that
     farm() writes while waiting, since stems are built by concatenation, so grep `turn.cpp` / `combat.cpp`.
   - Logs are cut at 1 MB per team unless you pass `--log-limit 40000000`.
   - `python bots/v133t/bench.py summary DIR...` gives record, banked, K/D, camp deaths and out-of-fuel turns.
7. **Deliverables:**
   - `bots/v214-<tag>/README.md` in the style of `bots/v213/README.md`: what changed, why, the bench table, and what
     was tried and dropped.
   - A short report back with: the table, the decision you recommend, and anything in this plan you found wrong.
8. **Do not** commit, push, or run `cambc submit` / `submissions activate` / `match-groups create`. Platform games
   use the team's quota and live entry, so the user decides (see Part C).

---

## Part B: experiment briefs

Each brief has the same parts: sources, motivation, hypothesis, design, code placement, flags, fuel, risks, test,
success criteria, and open questions for you to resolve.

### E0. Calibration from replays (analysis only, no bot change). Agent: "calibrate"

**Sources.**
- Wu et al., *Hierarchical Macro Strategy Model for MOBA Game AI*, AAAI 2019, https://arxiv.org/abs/1812.07887.
  - A "phase" layer recognises the stage of the game, and an "attention" layer predicts which map region each hero
    should go to.
  - Both are learned from top human games. A 5-AI team won 48% against top-1% human teams.
  - Our use: imitate *when and where* strong teams split, by measurement rather than a learned model.
- Fish study (A2.1): the template for this kind of measurement.

**Motivation.** E1-E3 each need thresholds:
- the health fraction f a solo may spend on a camp;
- the round and build at which solos start;
- how far solos stray, and how often they die alone.

Fish's numbers come from one team on 22 maps. Calibrating from several top teams makes the thresholds less likely to
be overfit.

**Task.**
1. Rebuild the replay decoder from memory note `replay-decoding`: the engine wasm plus PanelChrome decoder from
   `cambc view`, run in Node.
2. Collect replays of top teams. Ladder games we already have are under `replays/` (gitignored); otherwise list
   recent matches with `cambc matches` (download only, no queuing).
3. For each top-team champion-round, label:
   - solo / duo / core (allies within 6 tiles);
   - what it is hitting (camp kind);
   - its health fraction, and its build (emblem count and families, if the decoder exposes items; else gold spent);
   - the round, and its distance to the nearest ally.
4. Report:
   - (a) the distribution of `eff_hp x eff_dps / (camp health x strength)` when a solo engages a wolf, two wolves, or
     an elk (E2's bar; see E2 for the formula);
   - (b) the health fraction at which solos stop hitting and recall;
   - (c) the round and team emblems when the first solo appears;
   - (d) solo death rate per 100 solo rounds against core death rate;
   - (e) what a solo does when an enemy appears: keeps farming, moves away, or joins the core (E3).

**Output.** `results/split-calibration.txt` with the tables and the scripts' location. No bot folder.

**Open questions.** Does the decoder expose items? If not, can gold spent stand in for build?

---

### E1. Split by marginal value. Agent: "marginal". Flag `SAT_MARGINAL`

**Sources.**
- Coalition formation and multi-robot task allocation: Rizk, Awad & Tunstel, *Cooperative heterogeneous multi-robot
  systems: a survey*, ACM Computing Surveys 52(2), 2019
  (https://scholarworks.aub.edu.lb/items/2ca4c068-f8fa-49b0-905b-a84744415147); *Task Allocation Using a Team of
  Robots*, Current Robotics Reports 2022 (https://link.springer.com/article/10.1007/s43154-022-00087-4).
  - Coalition formation assigns an agent to the coalition where its *marginal contribution* (coalition value with it
    minus without it) is highest. It does not split a coalition's value evenly among members.

**Motivation.**
- SAT_EDGE (`phase.cpp:569`, `sat_pick` end) opens a split only if the solo group pays at least `main rate /
  allies_alive`. That assumes each member adds an equal share of the main rate. In practice:
  - Walking and the 5-round capture take as long with 3 champions as with 4. Only the clear gets slower.
  - While the main target respawns (12.5% of turns in farm.wait, A2.3), an extra member adds about nothing.
  - The main team's clear time today ignores team size (`clears[]` at TEAM_DAMAGE 200), so the code cannot even
    express "4 vs 3 present".
- SAT_EDGE's history cuts both ways: without it, the satellite took clusters while the main team, one short, cleared
  elks slower. A marginal test should keep that protection (the marginal value is high during an elk clear) and drop
  the rest (it is low during waits and walks).

**Hypothesis.** Replacing the share test with a marginal one splits more during waits, walks and cluster pick-ups,
and less during big clears. Expected result: more parallel farm rounds, the same or fewer deaths, higher camp gems.

**Design.**
1. **Team clear model.** Add a local estimate of the core's damage per round from the champions who will be at the
   main target.
   - Use our emblem record and base stats as `spread_estimate` does, or the live strength and focus of allies in
     sight. Pick one and justify it on fuel and agreement between champions.
   - `clear_k(s) = group health left / damage_k`, where damage_k is the core's damage without the satellite.
   - Keep TEAM_DAMAGE as the fallback.
2. **Marginal value of the satellite to the core:**
   ```
   rate_core(with)    = V(main) / max(walk + clear_with + capture,    ready_at(main) - now)
   rate_core(without) = V(main) / max(walk + clear_without + capture, ready_at(main) - now)
   marginal           = rate_core(with) - rate_core(without)
   ```
   Use `max(..., time until respawn)`: if the main target is still down when the core arrives, the cycle is bound by
   the respawn and the satellite's damage does not shorten it. Make sure the respawn wait is not counted twice: the
   existing `route_rate_steps` adds wait to steps.
3. **Opening and keeping:** the split opens if `solo rate >= SAT_MARGIN x marginal`. It is kept while
   `solo rate x SAT_KEEP >= marginal`.
   - Marginal can jump when the core starts an elk clear. Decide whether that should recall the satellite (likely
     yes, if it is near) and add hysteresis so it does not flap. Log each switch with NOTE.
4. **Leave everything else in place:** all other gates (quiet, health, bag, cooldown). Change only the value test.

**Code placement.** `split_candidate` (the `sat_best_rate >= SAT_EDGE ...` test) and the end of `sat_pick`. A helper
in `phase.cpp`, e.g. `double Bot::core_marginal(int main, vec2 origin)`.

**Flags.**
- `SAT_MARGINAL` (bool) and `SAT_MARGIN` (double, start at 1.0; sweep 0.75 / 1.0 / 1.5).
- Optional `SAT_MARGINAL_RECALL` for the elk-clear recall in step 3.

**Fuel.** One rate evaluation more per leader check, plus damage estimates. If you use live ally reads, cache them per
turn.

**Risks.**
- The leader and the satellite may compute different marginals because their `ready_at` values differ. Say who
  decides, and what happens on disagreement. Today the leader opens the split, and the satellite's own `sat_pick` can
  end it.
- More splitting means more exposure: watch the satellite's deaths.

**Test.**
- `v214-marginal` vs v213 over the pool, 3 seeds.
- Ablation: SAT_MARGIN sweep. Also check that with SAT_MARGINAL and a huge SAT_MARGIN (so it never opens), the bot
  matches SATELLITE off.

**Success.**
- Banked +3% or more pooled, with satellite deaths per game no higher.
- Split share of the satellite's turns up from the v159 baseline (5-40%).
- farm.wait share down.

**Open questions.** Is `clears[]` used elsewhere with the assumption that it is team-size free? Do not change it
globally in this experiment.

---

### E2. A health × damage bar for solo camps (Lanchester duel). Agent: "solobar". Flags `SOLO_BAR`, `SOLO_BUILD`

**Sources.**
- Stanescu, Barriga & Buro, *Using Lanchester Attrition Laws for Combat Prediction in StarCraft*, AIIDE 2015
  (https://ojs.aaai.org/index.php/AIIDE/article/view/12780).
  - Learns unit strengths for Lanchester models. Their bot used the predictions to decide attack or retreat, and beat
    the simulation-based version.
- *Combat Models for RTS Games* (https://arxiv.org/pdf/1605.05305): Lanchester square-law variants with target
  selection, which predict survivors and duration as well as the winner.
- Fish (A2.1): "the smallest squad whose expected camp damage is at most about half its health". Their rule is a
  Lanchester duel with a health fraction.
- Dota farm priority (https://liquipedia.net/dota2/Farm_Dependency): gold goes to the hero who turns it into the next
  big jump in strength soonest, not evenly.

**Motivation.**
- `sat_ok` limits the satellite to clusters and one wolf, using *base* stats fixed at setup. SAT_ELK's bar
  (SAT_ELK_TAKE 0.75 of health) never passed, and the satellite's growth from items is ignored.
- Top teams' solos hit elks 24-32% of the time (A2.4). They are built for it.
- Fixed emblem-count gates lost before (memory: hard-gate finding). A continuous bar from real stats may not.
- The vargvale lineage's **PT_GATE** (power-to-threat for team camp gates: elk 1.0, golem 0.75) gained +1.3% alone and
  more in the merge. Read `git show vargvale-strikes-back:bots/v202/src/constants.h` (search PT_GATE) and its code for
  a worked version.

**The duel.**
- One champion against one camp, with eff_dps per round and eff_hp. The camp has health H and strength S.
  - The champion kills the camp in `t = H / eff_dps` rounds and takes `S x t` damage.
  - It survives with fraction f of its health left if `eff_hp x eff_dps >= H x S / (1 - f)`.
- A group with several camps is `spread_clear()`: weakest camp first, each hitting until it dies. Use that, not the
  single-camp product.
- Effective stats (check them against the rules and the SDK):
  - eff_dps = strength (basic attack), plus its share of the attack reset (one extra hit every 8 turn ends), plus any
    damage spell the champion has (×1.15 with Monster).
  - eff_hp = current health / ((1 - 0.3 × Jungle) × (1 - 0.1 × Durability)).
- **Use current health, not maximum.** Camps never heal, so a solo can chip a strong camp to its floor, recall,
  heal and come back, as Fish does with golems. Whether the group clear can be split over visits, without the
  capture-delay rules interfering, is part of the work.
- Rough numbers, from the rules and not checked in-game: elk H x S = 144k. Base Ryth is about 1200 × 45 = 54k, so it
  can't solo an elk. With Jungle-Health and Monster-Strength, about 2.4k eff_hp × 90 eff_dps ≈ 220k, which clears an
  elk with health to spare. Confirm with real stats in-game.

**Design.**
1. **SOLO_BAR.** Replace the static `sat_ok` / `sat_elk` filters and the SAT_TAKE check in `sat_pick` with the duel
   above, run on the satellite's **live** stats. The satellite knows its own items and health; the leader knows our
   emblem record of it.
   - A group qualifies if the damage the satellite takes clearing it is at most `SOLO_TAKE x current eff_hp`.
   - Two decision makers need two estimates. The leader opens the split with a conservative estimate (emblem record).
     The satellite decides its own picks with live stats.
   - Allow single-wolf, double-wolf and single-elk groups (anything that passes). Keep monoliths and golems out.
2. **SOLO_BUILD (separate sub-test).** Order the satellite kind's shop so its first two emblems cut the time to its
   first solo elk: Jungle and Monster first.
   - For Marcella that means moving Jungle from 5th; for HoR, moving Monster up.
   - Test it alone and with SOLO_BAR. It interacts with the Vesper funnel: the satellite needs gold. Check how the
     funnel (VESPER_FUNNEL_EMBLEMS) affects the satellite's gold, and report the satellite's emblems by r300.

**Code placement.**
- `route.cpp:143` (setup flags).
- `sat_pick` and `split_candidate` in `phase.cpp`.
- `spread_clear` in `spread.cpp` (usable as is; it is a Bot method).
- `constants.h` builds.

**Flags.**
- `SOLO_BAR` (bool), `SOLO_TAKE` (start at 0.5; sweep 0.35 / 0.5 / 0.65, using E0's distribution if ready).
- `SOLO_BUILD` (bool).

**Fuel.** `spread_clear` per candidate is a few camps × a few statements. Visible camps read their health, so cap it
to the top N candidates by value.

**Risks.**
- A solo on an elk takes about 1,000 health, which increases how often it needs to recall. Count recalls per solo.
- Camp deaths: a camp kill destroys the bag. Report camp deaths per game, and check that FINISH_HEALTH and the
  satellite's 60% end rule still hold.

**Test.**
- `v214-solobar` (SOLO_BAR), `v214-solobuild` (SOLO_BUILD), `v214-solobar-build` (both), each vs v213, 3 seeds.

**Success.**
- Satellite elk and double-wolf captures appear.
- Camp gems up, with camp deaths and satellite deaths no higher than v213.
- Banked +3% or more pooled.

**Open questions.**
- Where is the attack-reset share best counted? The kits differ: Marcella's reset also stuns.
- Does Ryth's passive (1% of bonus health as Strength) show in `strength()`?

---

### E3. Price risk instead of gating on it. Agent: "risk". Flag `RISK_PRICE` (after E2 if possible)

**Sources.**
- Jorgensen, Chen, Milam & Pavone, *The Team Surviving Orienteers Problem: Routing Robots in Uncertain Environments
  with Survival Constraints*, Autonomous Robots 2018 (https://arxiv.org/pdf/1612.03232).
  - Each edge has a survival probability. Paths for a team are chosen to maximise the expected reward collected, with
    each robot's survival chance kept above a threshold.
  - A greedy method gets within a known factor of the best.
- Halite III bots (https://stonet2000.github.io/postmortem/): ships avoid places where an enemy can collide with them,
  and attack only with a large edge (target carrying at least 1.5x their cargo, with 2 friendly ships near).

**Motivation.**
- A2.5: splits lose to raids (outnumbered in 51% of rounds; v202-ladder 13-15 deaths a game).
- Today any enemy in sight ends the split (`play_pair`), and SPREAD's danger marks skip groups outright. Both are
  all-or-nothing. They abandon good work on a harmless sighting, and they know nothing about a dangerous area until
  an enemy is in sight.
- A death costs the bag, 60 rounds of farming and 100 gold to them. That can be priced in gems and compared with what
  the group pays.

**Design.**
1. **Risk map.** Per group s, `p(s)`: the chance a solo there meets an enemy force it cannot beat before it can get
   away. Build it from:
   - enemy sightings (`enemy_at` / `enemy_at_round`, or SPREAD's mark ring), decaying over about 30 rounds;
   - enemies alive (roster life reads);
   - the group's side: draft steps to each spawn, which are in draft memory;
   - whether their known farmers are near.
   Keep it simple, and justify each term with replay or bench evidence (E0's (d) and (e)).
2. **Expected loss.** `loss(s) = p(s) x (bag + gems the walk and clear would add + 60 x own farm rate + carried value)`.
   A group's value to a solo becomes `value - loss`, used inside `sat_pick`.
3. **Response to sightings.** Replace "any enemy in sight ends the split" with a quick Lanchester comparison:
   - (sum of visible enemies' eff_hp) × (sum of their eff_dps) against ours plus allies within reach. This is the
     square law; per-unit products alone are not enough with 2+ units.
   - Outcomes: keep farming (enemy weaker or far), move to a lower-risk group, or rejoin.
   - Reuse the OUTCLASS emblem-gap reading (`OUTCLASS_*`, v204/v205) rather than inventing a second one.
   - Never let a solo engage an enemy champion because of this rule. It only decides whether to keep farming or leave.

**Code placement.**
- `sat_pick` (value).
- `play_pair` (the `near_enemies()` end).
- `spread.cpp` marks, or a new `risk.cpp` (`cmake/CMakeLists.txt` globs `src/*.cpp`, so a new file builds as is).

**Flags.**
- `RISK_PRICE` (bool) and `RISK_DECAY` (rounds).
- `RISK_STAY_EDGE`: how much weaker the visible force must be for the solo to stay.
- Sub-switch `RISK_SIGHT` for the sighting response, so the risk map and the response can be tested apart.

**Fuel.** Risk per group every K rounds, not per turn. The square-law check only on turns with enemies in sight
(those are already the dearest turns, so keep it small).

**Risks.** The danger is solos staying out too long. The success line below has a hard limit on satellite deaths.

**Test.**
- `v214-risk` on top of `v214-solobar` if E2 passed, else on v213.
- Ablation: RISK_SIGHT on/off, 3 seeds.

**Success.**
- Satellite deaths per 100 split rounds lower than the base.
- Split rounds the same or higher, banked the same or higher.
- Kills against us while split, from the death events with context, down.

**Open questions.** How much of an enemy's build can a solo read in one sighting? See the ENEMY_EMBLEM_EVERY reads.

---

### E4. Plan short tours, the same way on every bot. Agent: "tour". Flag `TOUR_PLAN` (after E1 + E2)

**Sources.**
- **Orienteering with release dates:** Li, Archetti & Ljubić, *Reinforcement Learning Approaches for the Orienteering
  Problem with Stochastic and Dynamic Release Dates*, Transportation Science 2024 (https://arxiv.org/pdf/2207.00885).
  - Nodes become available at release times, and the route is valued with look-ahead.
  - Our respawn timers are known release dates.
- **Team orienteering with time windows** (TOPTW; e.g. https://maenvs4vrp.readthedocs.io/en/latest/environments/toptw/toptw.html):
  several agents, each with a route, collecting rewards that are only available in windows.
- **Sequential allocation:** Singh, Krause, Guestrin & Kaiser, *Efficient Informative Sensing using Multiple Robots*,
  JAIR 2009 (https://arxiv.org/pdf/1401.3462v1.pdf); IJCAI 2007 version (https://ijcai.org/proceedings/2007/355).
  - Plan robot 1's best path, remove what it covers, plan robot 2 on the rest, and so on.
  - It wraps any single-robot planner, and its loss stays bounded as robots are added.
  - Because it is a fixed order of greedy steps, each of our champions can run it independently and get the same
    assignment.
- **Halite III mining** (https://stonet2000.github.io/postmortem/, https://gist.github.com/DataWraith/4115f04bbb097986e19536cd0dda9522):
  ships rank targets by gain per turn *including the return to a dropoff*. The return leg is part of the target's
  price.
- **CBBA** (Choi, Brunet & How; overview https://www.emergentmind.com/topics/consensus-based-bundle-algorithm-cbba):
  each agent builds an ordered bundle (a route) and conflicts are settled by bids.
  - With 8 shared bytes, a full auction is out. Use it only if you find bits for a claim mask. Sequential greedy
    needs none.

**Motivation.**
- `pick_target` and `sat_pick` choose one group at a time. FIX_FILL and FILL_WIDE patch waits one filler at a time.
- A tour that goes "wolf A (up now) → elk B (up in 14) → bank at altar C" is invisible to a one-step greedy pick, and
  so is the ordering that wastes no wait.
- Fish "almost never waits: it farms something else and comes back in time".

**Design.**
1. **Single-unit tour for the satellite first.** From its position:
   - Candidates: the top 5 first groups by one-step rate.
   - For each, the top 4 second groups from that group's tile (`gap()` gives cached group-to-group steps).
   - Optionally a third leg, which is a bank (nearest open altar) or a recall.
   - Tour value = gems + gold value collected − expected loss (E3, if present) − wait rounds × own rate.
   - Feasible only if the health budget from E2 lasts: cumulative `spread_clear` damage ≤ budget, else the tour ends
     in a recall.
   - Take the first leg of the best tour, re-plan every few rounds or when the first leg finishes, and keep the
     current tour unless another beats it by a margin.
2. **Core tours (stretch):** run the same planner for the core's target in place of FIX_FILL's single filler. Run it
   only if step 1 pays.
3. **Several solo units (stretch, if E1 opens a second solo on 5-champion teams):** sequential greedy in a fixed order
   (roster order). The core plans first; each solo then plans on the groups not yet taken.
   - Every champion runs the same steps on common facts only, so each knows the others' tours without messages.
   - Champions disagree on `ready_at`. Use only common-knowledge timers (respawns the team captured itself, shared
     via CAPTURE_NEWS) for allocation, and private knowledge only for one's own tour order.

**Code placement.**
- New `tour.cpp` (picked up by the `src/*.cpp` glob in `cmake/CMakeLists.txt`).
- Called from `sat_pick`.
- Group-to-group steps through `gap()`. Precompute the full matrix on the first play turn (2x fuel) if it is not
  already cached.

**Flags.** `TOUR_PLAN` (bool), `TOUR_WIDTH` 5, `TOUR_DEPTH` 2, `TOUR_REPLAN` (rounds), `TOUR_KEEP` (margin).

**Fuel.** 5 × 4 = 20 tour evaluations at about 50-100 statements each fits in a few thousand fuel. Profile with
`bots/v133t/profpatch.py` + `profsum.py`. Keep out-of-fuel turns level with v213; if they rise, re-plan less often.

**Risks.**
- Re-planning churn: the team walking between far groups was a past failure (the comment at `route.cpp:1730`).
- Disagreement between champions on shared allocation (step 3).

**Test.**
- `v214-tour` on the best of E1/E2 (and E3 if kept), with TOUR_DEPTH 1 (equal to the base, which checks the code) vs
  2 vs 3. 3 seeds.

**Success.**
- farm.wait share down.
- Banked and camp gems up over the base by more than noise.
- Fuel level.

---

## Part C: order, merge and hand-back

**Order and parallelism.**
- E0, E1 and E2 are independent and can start at once.
- E3 needs E2's bar (it can start on v213 and rebase later).
- E4 needs E1 and E2.
- All agents share one machine: lower `--jobs` when benching at the same time (A5.4).

**Merge (main session, not a subagent).**
1. Combine the kept flags into `bots/v215` and bench it against v213 and each single winner. Memory note
   `small-map-plan` says winning switches don't always add up when combined.
2. Write `bots/v215/README.md`.
3. Save a memory note for each finding, with numbers, the way earlier findings are kept.

**What goes back to the user.**
- Local results have overstated the ladder before (A2.5, A2.6), and splitting has failed on the ladder for survival
  reasons. A local win is not enough.
- The final candidates need the platform test set (memory `platform-testing`: the 10 teams we lose to most, on their
  maps), compared with the live entry's results on the same set.
- Submitting, activating and queuing are the user's call. Each step uses quota or changes the live bot. Hand back the
  bench tables and the proposed platform test, then wait.

**Out of scope here, but noted.**
- Fish's draft (Cassi & Issac as one pick but two bodies, + Vendetta + HoR) is untested and would give the split
  machinery (the dead V86 pair split) a second farmer.
- Teleport crystals for walk savings (Fish: 20-36 teleports a game on maps where one jump saves 30+ steps).
