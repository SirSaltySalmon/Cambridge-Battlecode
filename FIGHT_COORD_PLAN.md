# Fight and coordination experiment plan (base: bots/v213)

Written 2026-11-03. Same layout as `FARM_SPLIT_PLAN.md`: this file hands one experiment to each subagent.
**Every agent reads Part A (shared context) in full, then its own brief in Part B.** Part C covers the order, the merge
and what goes back to the user.

The question behind it: our champions decide fights one by one, each from its own vision. The team fight score is a
hand-tuned sum of terms that stopped improving several versions ago. Strategy-game and multi-agent AI have well-studied
answers to both problems:
- joint commitments between agents with little communication (robot soccer, agent teamwork theory);
- combat models that predict a fight's outcome over time (StarCraft research);
- target and role assignment that stays consistent between agents;
- per-tile movement evaluation (the dominant Battlecode technique).

Each experiment is a flag on top of v213. **With the flag off, the bot must play exactly as v213.**

This plan runs alongside `FARM_SPLIT_PLAN.md`, whose agents use `bots/v214-*` and `bots/v215`. This plan uses
**`bots/v216-<tag>`** for experiments and **`bots/v217`** for the merge. Both plans may bench on the same 20-core
machine at the same time, so read A6.4.

---

## Part A: shared context (every agent)

### A1. The rules that matter for fights

The full rules are in `CLAUDE.md`. The facts these experiments touch:

- **A turn:**
  - One ordinary move (8 directions), one basic attack and one successful ability cast (either slot).
  - **Ranged champions cannot walk and basic-attack in the same turn**, and the same holds for their ranged damaging
    spells ("excludes walking"). Melee champions can walk and hit.
  - Attack-reset abilities restore the attack allowance.
  - Both ability slots have an 8-turn-end cooldown.
- **Ranges** (Chebyshev distance, diagonals count 1): melee basic attack 1, ranged basic attack 3, most spells 4. Dashes
  move up to 2 steps and may cross holes.
- **Turn order:**
  - Teams alternate who acts first each round.
  - Within a team, higher priority acts first, then draft order. See `priority_of`, `constants.h:1862`: Ren and Ryth 7;
    Cassi, Willow and HoR 6; Issac 5; Vendetta 4; Varg, Marcella and Prism 3; Vesper 2.
  - **A shared-memory write is visible to every champion that acts later in the same round.** The leader stamp in
    `lead_block` (`route.cpp:1820`) depends on this.
- **Stuns** last one round. Marcella is immune.
- **Shields:** Varg 2.5 × Focus and HoR 1.5 × Focus, each lasting 5 turns. Shields are not health for HoR's passive.
- **Death:**
  - The champion is out for 60 rounds.
  - The killer takes every carried gem plus 100 gold (+50 with Hunter). A camp kill destroys the gems.
  - Cassi and Issac die together.
- **Recall:** 5 turn ends (3 with Home). Damage cancels it. Standing on our spawn heals to full at turn end, and enemies
  cannot step onto it.
- **Camps** hit the nearest champion in reach. Ally ties go to whoever acts first (CAMP_TIE_ORDER, measured 1253 of
  1254). Reach: wolf 3, elk and golem 4, monolith 5. Camps never chase.
- **Vision** is 8 tiles, blocked by walls and bushes, and **not shared**. The team shares **8 bytes** of memory during
  the match, plus 1024 read-only bytes written in the draft.
- **Fuel:** 100k per turn in C++, 2x on the first play turn, 100x on the first draft turn. A bot that runs out is
  suspended mid-turn and resumes next round. `play_turn` replays a resumed turn from scratch (`turn.cpp:8`).

**Champion kits** (base health / strength / focus, attack type; F = Focus):

| Champion | Base | Slot 0 | Slot 1 | Passive |
|---|---|---|---|---|
| Ryth | 1200/45/100 M | 1F area stun, radius 1, range 4 (no walk) | attack reset | +1% of bonus health as Strength |
| HoR | 1000/30/100 M | 1.5F shield | dash 2, 1.5F area stun on landing | up to +50% damage by missing health |
| Willow | 700/55/140 R | heal ally 3F, range 4 | attack reset | none |
| Vesper | 1000/45/100 R | attack reset | dash 2 | +33% gold |
| Marcella | 1200/45/60 M | reset, next hit stuns | 2F single, range 4 (no walk) | stun immune; 2 gold per gem tributed |
| Prism | 700/35/80 R | 1.5F + stun, range 4 (no walk) | 1F area, range 4 (no walk) | +1.5 Focus per carried gem |
| Varg | 1000/45/100 M | attack reset | 2.5F shield | basic attacks splash Strength in radius 1 |
| Vendetta | 1000/70/100 M | attack reset | dash 2 | a kill resets her cooldowns |

**Rosters we draft** (`draft.cpp:24-38`):
- ROSTER_5: Ryth, HoR, Willow, Vesper, Marcella. Prism replaces Marcella on gem-rich maps; Varg replaces her on
  Varg maps.
- ROSTER_4: HoR, Vesper, Willow, Ryth. Prism replaces Ryth on gem-rich maps.
- Three-champion maps: HoR, Vendetta, Vesper (VENDETTA_3 is on).
- The Prism team on gem-cluster maps: Prism, HoR, Marcella, Ryth, Willow.

So almost every fight involves HoR, Vesper and Willow, and usually Ryth.

### A2. The problem, with the evidence we already have

The project memory notes live in
`C:\Users\salty.THE-AQUARIUM\.claude\projects\c--Users-salty-THE-AQUARIUM-Desktop-Vinamilk-1--Workbench-Code-Cambridge-Battlecode\memory\`.
Read the ones named here; each has numbers and replay folders.

**1. What fights look like in losses.**
- From `replay-decoding.md`, on 50 ladder games of v144v:
  - We lose fights at equal numbers 46% of the time, and 70% when outnumbered.
  - The first death is alone in 64% of losses (48% of wins).
  - Vesper dies 3.9 times a game in losses against 0.7 in wins. Most of those deaths come from above 60% health within
    5-7 rounds, with allies more than 3 tiles away and enemy melee 1-2 tiles from her.
  - HoR dies 6.4 times a game in losses, half of them alone, mostly on dash dives.
- From `ladder-behaviour-finding.md`: games are decided by r150-300. In losses we die 4.1 times by r300 against 1.4 in
  wins.
- From `join-finding.md`: 46% of our deaths to champions had no ally within 4 tiles. Fights begun with no ally within 4
  killed us 10% of the time, against 6% together.

**2. Our emblem gap predicts deaths better than our own emblems do.** From the v181 note at `constants.h:1821-1824`,
on 21,638 ladder contacts:
- Our death rate within 20 rounds of a contact, by local emblem gap: -16 → 56%, -8 → 41%, -4 → 31%, 0 → 13%,
  +4 → 8%, +16 → 4%.
- As predictors of our death: our own emblems AUC 0.59, the emblem gap 0.76, the health ratio 0.78.
- The study it cites (`bots/v177-light/DEATHS_AND_FIXES.md`) **is not in this checkout or any branch**. Ask the user
  for it before redoing that work.

**3. Results so far: team-level rules won, per-champion rules lost.**

| Kept | Result |
|---|---|
| JOIN_FIRST (v119) | Closing alone waits up to 3 rounds for an ally: +3.4% banked, kills +15-20%, deaths -11-16%. |
| TEAM_CHASE (v117) | Team-level chase cut-off: 129/240. |
| FOCUS (v183) | One shared target in a 4-bit field: +8.6% banked. |
| Enemy weighting (v124) | FIGHT_ENEMY_WEIGH, with the health edge retuned: +21.6%. |
| GHOST (v140) | Chase the enemy we just lost sight of for 5 rounds: +10%. |
| HOR_DIVE_FOLLOW (v146) | A dive lands only with 2 allies within 3: +5.3%. |
| OUTCLASS / OUTCLASS_FIRST (v204-v210) | Leave fights at an emblem gap of -5 or worse. |

| Lost | Result and reason |
|---|---|
| Per-champion chase cut-offs | CHASE_PATIENCE and CHASE_LINE_ALL (v116): the team splits. |
| FIGHT_ALLY_WEIGH (v124) | Off. |
| FIGHT_COMMIT_PING (v124) | -12.1%: far allies stopped farming. |
| PING_GATHER (v119) | Deaths +10%. |
| PING_IDLE_ONLY (v128) | Gathers more, wins fewer fights. |
| TEAM_CHASE_REACT (v118) | Re-engaging sooner lost gems on every run. |
| VESPER_LEASH (v146) | -3.3%: pulling the carry to the team drags the fight onto her. |
| CHASE_LEAD (v138) | -7.9%. |
| EMBLEM_RELATIVE (v115) | 34-46: the team retreats more and gets run down. |
| GRADED_EDGES (v115) | 23-57 with EMBLEM_RELATIVE. |
| KILL_TIME (v181-v185) | Its ±20 clamp outweighed every other term. 43% of scores sat at the cap, and the team retreated too often. |
| EMBLEM_GAP (v181-v182) | Lost on the ladder: against teams ~13 emblems ahead, every fight became a retreat. |

The sources are `chase-finding.md`, `join-finding.md`, `fight-weigh-finding.md`, `lead-finding.md`,
`replay-decoding.md`, and the comments at `constants.h:1741-1846`.

**4. The lessons every brief must respect.**
- **L1. Team-level beats per-champion.** Every per-champion cut-off split the team. Every team-level rule (one shared
  focus, a team chase cut-off, joining before closing) won.
- **L2. Gathering costs farming.** Calling allies to every contact lost (FIGHT_COMMIT_PING, PING_GATHER). A gather must
  be worth the farm rounds it costs.
- **L3. Retreat is not free.** Every "more honest" score that retreated more often lost, because the team got run down
  and gave up farm (EMBLEM_RELATIVE, EMBLEM_GAP, KILL_TIME). A new evaluator must price retreating as well as fighting.
- **L4. Self-play misleads.** Fight-score numbers tuned against our own copy did not hold against a pool
  (`score-sweep-finding.md`). Our local bots are never far apart in emblems, so local benches cannot show emblem-gap
  effects (`constants.h:1837`). The ladder decides.
- **L5. The current score sits at a local optimum.** No single knob step beats the current numbers
  (`score-sweep-finding.md`; evotune and Optuna since). More tuning will not help. A change of structure might.

### A3. How v213 decides fights today (code map)

All paths are under `bots/v213/src/`. Read the functions your brief names, not just this summary.

**The order of a turn** (`Bot::play`, `turn.cpp:48`):
1. `observe`, `track_enemies`, `check_hidden`, `update_emblems`, `update_enemies`.
2. **`update_ping`** (`combat.cpp:2110`): reads and ages the ping byte, and sends a ping when an enemy champion is
   hurting us with no ally beside us.
3. **`sync_decision`** (`route.cpp:2025`): the shared decision, either followed or led.
4. Channel handling (`turn.cpp:106`).
5. Hurt recall or dodge (`turn.cpp:229-322`): self-preservation, which runs before any fight logic.
6. `retreat_to_allies` (PING_RETREAT, `turn.cpp:325`), then **`outclass_step`** (`spread.cpp:363`, before collection
   since v210).
7. Collection and claims (`turn.cpp:334-374`).
8. **Local re-score** (`turn.cpp:396-416`): if the shared mode is FARM or TRIBUTE and *this champion* sees a near enemy,
   it re-scores `score_mode(position, local_prev)` from its own tile and vision and may switch itself to FIGHT. Only a
   built Ren needs an ally beside him for that.
9. **`update_team_chase`** (`combat.cpp:433`), per champion. Its "team" gap memory is kept by each bot separately, so
   different champions drop the chase at different rounds.
10. **`fight()`** (`combat.cpp:138`), or **`retreat()`** (`combat.cpp:649`) for a too-hurt carrier. Then GHOST
    (`turn.cpp:469`), ping answering, banking, shopping, and `farm()`.

**Who leads.**
- `lead_block` (`route.cpp:1820`): the first champion each round that is alive, not channelling, not split, not in a
  hole pocket and with the team settles the shared decision. Everyone else copies mode, slot and claims.
- **The leader scores the fight from the target group's centre with its own vision** (`route.cpp:2155`). A leader that
  sees no enemy writes FARM, whatever an ally is fighting.

**The live fight score** (`score_mode`, `route.cpp:2628`). With v213's flags (KILL_TIME, EMBLEM_GAP, GRADED_EDGES and
FIX_FIGHT_BODIES are all **off**), the score is:
- **Allies counted:** FIGHT_LOCAL, i.e. within 12 tiles of us or of the near enemy closest to us. Allies under the
  recall bar and carriers are dropped (FIGHT_SKIP_LEAVERS).
- **Enemies counted:** weighted by JOIN_WEIGHT on their distance from that front enemy. Channelling enemies and enemies
  under 35% health count 0.
- **Terms:**
  - `bodies`: +OUTNUMBER_BONUS (0) if our weighted count is higher, else -MISSING_BODY_PENALTY (7) × the deficit.
  - `health`: **sign only**, ±HEALTH_EDGE 7.
  - `power`: sign only, ±POWER_EDGE 2, on summed strength + focus.
  - `emblems`: **+EMBLEM_BONUS 3.0 × our fighters' emblem count**. Ours only; the enemy's emblems are not subtracted.
  - Skirmish and Hunter family terms; -OVEREXTEND_PENALTY 20 when a squishy is in front of every tank; a spawn bonus.
  - -DISTANCE_PENALTY 0.1 per tile to the nearest enemy; +5 for a fresh ping near us.
- **Decision:** FIGHT if the score is at least FIGHT_THRESHOLD -14. Staying in needs 15 less (FIGHT_HYSTERESIS) unless
  we are behind on both health and power.
- The tuned numbers are in `evo_constants.h`.
- **What this means:** a mid-game team with 8 emblems among its fighters starts about +24 up, so it takes almost any
  fight. OUTCLASS (`spread.cpp:337`) is the separate brake: each champion's own emblem gap within 8 tiles,
  -5 or worse means leave.

**Inside `fight()`** (`combat.cpp:138-334`), each champion applies its own filters in this order:
1. Chase-ring checks near their spawn (`chase_line`, `combat.cpp:548`).
2. Target pick. A channelling enemy in reach comes first, then the **FOCUS** target (`combat.cpp:225-251`), then the
   weakest in reach, then the weakest overall.
3. Camp exits (CAMP_EXIT_REACT, CAMP_SHARE, CAMP_WAIT_FIGHT, MONO_FIGHT_OUT).
4. **`join_first`** (`combat.cpp:337`).
5. `engage()` (`kits.cpp:10`), which dispatches per kit:
   - `engage_melee` (`kits.cpp:42`) and `engage_ranged` (`:165`);
   - `engage_vesper` (`:777`), with `vesper_kite` and `vesper_follow`;
   - `mage_fight` (`:1071`);
   - `engage_hor` and `hor_dive` (`hor.cpp:284`, `:147`);
   - `willow_backline` (`:1138`).

**FOCUS** is a 4-bit field in the stamp byte (`state.cpp:286-292`).
- It is **never cleared**. The first champion whose near list lacks the current focus overwrites it with its own pick.
- So two champions in two separate fights overwrite each other's focus.

**Movement.** `go()` (`actions.h:50`) is the one mover, with dash policies and a ranked-landing hook. `retreat()` uses
`recall_zone` (enemy dash and spell reach), RETREAT_LEAD and `vesper_retreat`. `escape_rank` ranks escape dashes.

**Memory each champion keeps about enemies** (`bot.h`):
- `enemy_at[]` and `enemy_at_round[]`: last tiles, per enemy ident.
- `enemy_emblems[]` and `enemy_emblem_round[]`.
- `enemy_dead_since[]`.
- `ghost_*`: the champion we last fought.
- Roster liveness can be read for every champion, in or out of sight (`team_alive()` and `seen::roster`).
- About allies: `ally_at[]` and `ally_seen[]` are last sightings; `emblems` is our emblem record per ally.

**Damage helpers** (`state.h`):
- `round_damage(kind, strength, focus)`: basic attacks plus resets, plus spells averaged over 8 rounds.
- `kit_spell_mult`, `kit_resets`.
- `champ_dmg_mult` and `champ_taken_mult` for Skirmish, Collectors and Durability.
- `hor_shield_hp`.
- `ready_moves` (`combat.cpp:398`): reads which dash or stun is ready. An enemy's read can throw, so wrap it.

### A4. Shared memory map (all 8 bytes are in use)

From `constants.h:1994-2035` and the code:

| Byte | Contents | Writer |
|---|---|---|
| 0 | mode (bits 0-1: FARM 0, FIGHT 1, RETREAT 2, TRIBUTE 3), tank or carrier (bits 2-5), aggressive (bit 6), Vesper or Prism built (bit 7) | leader; carrier; Vesper |
| 1 | target slot | leader; `share_loot` |
| 2 | capture slot (255 = none) | leader; claimers |
| 3 | collector (champion + 1) | leader; claimers |
| 4 | stamp (bits 0-3, `round % 15 + 1`) and FOCUS (bits 4-7, enemy roster index + 1) | leader; first fight-target picker |
| 5 | phase (bits 6-7) and target2 (bits 0-5) | leader; split pair or satellite |
| 6 | claim round (`round & 0xFF`). Only compared against CAPTURE_TIMEOUT = 15, so **6 bits would do, freeing bits 6-7** | claimers |
| 7 | **ping**: route slot + 1 (bits 0-5), age `round & 3` (bits 6-7) | `update_ping`, `commit_ping` |

A note on byte 7: `WARD_MASK_BYTE` is also 7, but its writes happen only inside `place_ward`. That never runs in v213,
because FIX_SCOUT_WARD is on and SCOUT_KEEP_POSTS is off (`turn.cpp:153-159`). The ward-mask bit clears at
`combat.cpp:1954` and `:2089` only run when `ward_slot` is set, which only `place_ward` does. So in practice byte 7 is
the ping. (`FARM_SPLIT_PLAN.md` A3 calls it the ward mask. That is wrong in practice. Tell the main session if you
confirm it.)

**Rule:** a new shared field must say exactly which bits it takes and why the old user no longer needs them. Prove it
with a bench where the field is written but not acted on (it should match v213).

### A5. Hard constraints

- **Fuel** (from CLAUDE.md, the user's priority): report mean fuel per turn and out-of-fuel turns against v213 for every
  variant.
  - All log text goes through the LOG / WHY / NOTE / SUB macros (inside `debug_block`), never a bare
    `note(format(...))`.
  - Measured costs (`fuel-cost-model.md`):
    - champion reads (`position()`, `health()`, `is_recalling()`): about 100 each, through the engine;
    - `base_stats`: about 220;
    - a new bitboard: about 210-310, an op into an existing board about 100;
    - a new `distance_field`: about 1,550, a `compute_to` about 3k;
    - a printed line: 1-1.5k; a guest statement: about 10.
  - Use `seen::positions()` and `seen::self_position()` rather than re-reading tiles. Hoist reads out of loops and
    reuse buffers. Never loop over a goal bitboard's tiles per move.
- **Determinism without messages:** decisions the team must agree on come from one writer, or from facts every champion
  shares. Shared facts: roster kinds and liveness, the map, draft memory, the shared bytes, our emblem record.
  Positions, health, `ready_at` and the vision lists are **not** shared. Each brief says how it handles that.
- **Base rule:** with your flag off, the build plays exactly as v213. Check this once: the off build against v213 must
  give identical results on a short bench. Games are deterministic.
- **Do not edit `bots/v213`.** It is the base for both plans.

### A6. Workflow

1. **Make your folder:** `python bots/v133t/bench.py variant v213 v216-<tag> FLAG=VALUE`. This copies v213 and sets
   constants, and skips `.cambc` and `*.py`. If you copy by hand, delete `.cambc/` in the copy. To add a *new*
   constant, edit `bots/v216-<tag>/src/constants.h` yourself; `variant` only rewrites existing ones.
2. **Code style:** follow `constants.h`. Each new constant gets a comment block saying which version added it, what it
   does, why (with numbers), and later the bench results. Put switches near the code they relate to.
3. **Build and smoke-test:** run one debug game (`cambc run`, without `--release`) and read the bot's log for your NOTE
   and WHY lines before benchmarking.
   - Log lines look like `r<round> c<champ> (x,y) <branch>: <why> | <outcome>`, followed by indented `@s hp ... mode
     ... slot ...` audit lines and `- note` lines.
   - The regex is in `bots/v119/analyze_spread.py`. Strip NUL padding from the log (`chr(0)`) before parsing.
4. **Opponents, seeds and load:**
   - Pool: v213 (mirror), v210, v208, and v185 if you can get it:
     `git archive vargvale-strikes-back bots/v185 | tar -x -C .` (check that `bots/v185` does not exist first).
   - Seeds: `python bots/v133t/bench.py seed <bot> N` makes fuel-burn timing seeds (N = 1, 2). Run seeds 0-2 for
     **both** v213 and your variant.
   - Manifest and run: `python bots/v133t/bench.py manifest <OUT> <OPPONENT> <bot>[,<bot>...]` covers all 70
     `maps/*.map26a` on both sides (the user's rule). Then `cambc run-batch b<OUT>.txt --jobs N --json`.
   - The machine has 20 cores. **Both plans' agents may be benchmarking at once: use `--jobs 4`.**
   - Fight analysis needs the decision logs: run benches in debug mode with `--log-limit 40000000` (memory
     `join-finding.md`: the user wants benches run with logs).
5. **Noise:** one 140-game run swings about ±10% banked. Three pooled seeds are about ±3.5%. Treat smaller effects as
   noise unless a mechanism shows in the logs.
6. **Metrics to report** (variant vs v213, same opponents and seeds):
   - Record; banked (mean and by r300 / r600; `bots/v114/analyze_gold.py`); kills, deaths (champion and camp,
     `bots/v114/analyze_camps.py`), K/D.
   - Mean fuel per turn and out-of-fuel turns (`bots/v114/fuel_bench.py`, `fuel_compare.py`; `fuelExhausted` events in
     the stats JSON).
   - `python bots/v133t/bench.py summary DIR...` gives record, banked, K/D, camp deaths and out-of-fuel turns.
   - **Fight metrics from F0's tool** once it exists: disagreement rate, entry spread, lone deaths, contact outcomes,
     per-champion deaths (Vesper and HoR above all).
7. **Deliverables:**
   - `bots/v216-<tag>/README.md` in the style of `bots/v213/README.md`: what changed, why, the bench table, and what
     was tried and dropped.
   - A short report back with: the table, the decision you recommend, and anything in this plan you found wrong.
8. **Do not** commit, push, or run `cambc submit` / `submissions activate` / `match-groups create`. Platform games use
   the team's quota and the live entry, so the user decides (Part C). Listing and downloading existing matches
   (`cambc matches ...`) is fine.

---

## Part B: experiment briefs

Each brief has the same parts: sources, motivation, hypothesis, design, code placement, flags, fuel, risks, test,
success criteria, and open questions for you to resolve.

### F0. Fight measurement tool (analysis only, no bot change). Agent: "fightlog"

**Sources.**
- `bots/v119/analyze_spread.py` (lone deaths and closing turns) and `bots/v116/analyze_chase.py`: our own log parsers.
- Drachen and colleagues define a team fight in game logs and show that fights carry most of the signal about who wins
  a MOBA game (*Match Outcome Prediction in MOBAs with Deep-Learning-based Team Fight Models*, IEEE CoG 2022; summary
  at https://sdu.dk/en/forskning/sdumetaverselab/news/predicting-dota-2-esports-match-outcomes-with-recurrent-neural-networks).
  Their idea we use: cut the game into fight episodes, and measure outcomes per episode, not per game.

**Motivation.** Every other brief needs to show whether the team now acts *together*, not just whether it wins more. Our
current tools count lone deaths and closing turns. Nothing measures disagreement or staggered entry, the two failures
this plan targets.

**Task.** Write `tools/fights/fightstats.py`, which reads debug replays: the `.repl26a` zip's `team-a.stdout` and
`team-b.stdout`, plus the stats JSON.
1. **Contact episodes.** An episode starts when any of our champions has an enemy champion within FIGHT_STEPS (10) and
   none did for the previous 5 rounds. It ends 5 rounds after the last such round. Positions come from the log lines.
   Enemy positions are not in our logs, so use the `@s` audit line's notes, or the replay decoder from F1.
   - Simpler fallback: the episode starts with the first `fight.*`, `retreat.*`, `outclass.*` or `hurt.*` branch near
     a death, or a `ping` NOTE. Document the definition you used.
2. **Per episode:**
   - (a) **Disagreement:** rounds where two or more of our living champions within 12 tiles of each other are in
     conflicting branch families (engage = `fight.hit` / `fight.answer` / `ghost`; hold = `fight.join*` /
     `fight.wait`; leave = `retreat.*` / `outclass.*` / `hurt.*` / `fight.leave`; farm = everything else).
   - (b) **Entry spread:** the rounds between the first and the last of our champions to first land in reach of an
     enemy (from the `| outcome` chunks or the stats damage events).
   - (c) **Bodies at the first hit**, ours against theirs.
   - (d) **Outcome:** kills, deaths, and the gem swing (gems lost and stolen; `gemLoss` events with owner and context)
     inside the episode window.
   - (e) **Score at entry:** the leader's `lead ... -> fight X.X >= bar (...)` text and any `own sight disagrees` line.
3. **Per game:** the episode count, the share of episodes with disagreement above 20% of their rounds, the median entry
   spread, lone deaths (no ally within 4 the round before), and deaths per champion kind.
4. **Run it** on a debug bench of v213 against the pool, seed 0, and write `results/fight-baseline.txt`. Also list the
   10 episodes with the worst disagreement, as map plus rounds, so other agents can replay them.

**Output.** The tool, its usage line at the top of the file, and `results/fight-baseline.txt`. No bot folder.

**Open questions.**
- Does the stats JSON carry per-hit damage events with attacker and target? If so, entry and outcome are exact.
- Can enemy positions be read without the replay decoder? If not, share a decoder module with F1.

---

### F1. Learn the fight model from ladder replays (analysis only). Agent: "calibrate"

**Sources.**
- Stanescu, Barriga & Buro, *Using Lanchester Attrition Laws for Combat Prediction in StarCraft*, AIIDE 2015,
  https://ojs.aaai.org/index.php/AIIDE/article/view/12780. Learns the Lanchester attrition coefficients from battles,
  predicts survivors as well as the winner, and decides attack or retreat with the predictions.
- Uriarte & Ontañón, *Combat Models for RTS Games*, IEEE TCIAIG 2018, https://arxiv.org/pdf/1605.05305.
  - Compares TS-Lanchester², Sustained-DPF and Decreasing-DPF models, with damage-per-frame matrices and
    target-selection orders **learned from replays** (Borda count over first kills).
  - It also reviews LTD and LTD2 = Σ √HP × DPF (Kovarsky & Buro), which count several damaged units as better than one
    full one.
  - Its TS-Lanchester² with static rates predicted winners best.
- Stanescu et al., *Predicting Army Combat Outcomes in StarCraft*, AIIDE 2013,
  https://skatgame.net/mburo/ps/aiide13-combat-outcomes.pdf: a rating-style (Elo/TrueSkill-like) learned model.
- Yang et al., *Real-time eSports Match Result Prediction*, 2016
  (https://cdnjs.deepai.org/publication/real-time-esports-match-result-prediction): logistic regression on Dota 2
  replay features, a strong simple baseline.
- Our v181 ladder contact study (A2.2), and `replay-decoding.md` for the decoder recipe.

**Motivation.**
- The live score's weights were tuned by self-play and evolutionary search (L4, L5).
- F2 and F3 need two things: an evaluator whose parameters come from **real opponents**, and a value for a death in gems.
- The v181 numbers (emblem gap AUC 0.76, health ratio 0.78) show that ladder data separates fights well, but nobody has
  fit a combined model on it.

**Task.**
1. **Rebuild the decoder** from `replay-decoding.md`:
   - The engine wasm and PanelChrome decoder come from `cambc view <file> --port P` (assets at
     `http://127.0.0.1:P/assets/`). Pick the wasm by the match's engineVersion.
   - In Node: `Game_FromReplayFileBytes`, `Game_Visualiser_Timeline_ReplayNext`, `Game_Visualiser_ReadTimelineState`.
     The decoded champions carry index, team, type, position, health, alive, gems, gold, kills and stunSource.
   - Save it as `tools/fights/replay.mjs` and share it with F0.
2. **Get ladder games.** No ladder replays are in this checkout. List our recent matches with `cambc matches ...` and
   download them with `cambc matches download ID -o x.zip`. Download only: no queuing and no new match groups. Aim for
   200+ games, weighted toward the 10 teams we lose to most (memory `platform-testing.md`).
3. **Build a contact dataset** (both teams, so we learn from the opponent's fights too):
   - Contacts as in F0.
   - Per side at contact start:
     - bodies within 4 / 8 / 12 tiles;
     - summed and per-unit health, and max health;
     - strength and focus if the decoder exposes them, else gold spent or item counts as a proxy;
     - each unit's distance to the nearest enemy, as an **arrival-time** proxy (steps minus range);
     - bags carried; spawn distance.
   - Labels: deaths per side within 10 and 20 rounds, and the gem swing.
4. **Fit and compare** with logistic regression (and one tree model as a check):
   - (a) The current score's terms, reconstructed: body deficit, health sign, power sign, our emblem count.
   - (b) A Lanchester square-law feature: log(H_ours × D_ours / H_theirs × D_theirs).
   - (c) (b) with each unit's damage counted only from its arrival round, plus burst and stun counts if the decoder
     shows casts (`stunSource`).
   - (d) The emblem gap alone.
   - Report AUC on held-out games and calibration plots as tables.
5. **Value of a death,** to feed F3:
   - The average gems the team captures per champion-round alive. A death's farm cost is 60 × this.
   - The gems a killer gains (bags).
   - Report by game phase (before r300, r300-r600, later).

**Output.** `results/fight-model.txt` with the tables, the fitted coefficients in a form F3 can paste into
`constants.h`, and the scripts under `tools/fights/`.

**Open questions.**
- Does the decoder expose items or stats? If not, is gold spent a good enough proxy? Check it against games where both
  sides' builds are known from our own logs.
- Does `stunSource` let you recover cast timing?

---

### F2. Engagement contract: one team decision per contact. Agent: "contract". Flag `CONTRACT`

**Sources.**
- **Joint intentions.** Cohen & Levesque, *Teamwork*, Noûs 25(4), 1991 (https://www.sri.com/publication/teamwork/).
  Precursor: Levesque, Cohen & Nunes, *On Acting Together*, AAAI 1990.
  - A team holding a **joint persistent goal** keeps it until a member comes to believe it is achieved, unachievable or
    irrelevant.
  - That member must then **make this mutually known** rather than drop out silently.
- **STEAM.** Tambe, *Towards Flexible Teamwork*, JAIR 7, 1997 (https://jair.org/index.php/jair/article/view/10193,
  https://arxiv.org/abs/cs/9709101).
  - Builds joint intentions into a working team model for RoboCup and simulated pilots.
  - Weighs the cost of telling the team against the cost of **miscoordinated termination** (one member quits while the
    rest go on).
  - The RoboCup abstract (https://u.cs.biu.ac.il/~kaminkg/publications/papers/robocup98.pdf) warns that requiring
    mutual belief every time makes the team rigid. Keep the protocol light.
- **Locker-room agreement.** Stone & Veloso, RoboCup simulation teams
  (https://link.springer.com/content/pdf/10.1007/3-540-45327-X_42.pdf): pre-agreed protocols and roles let agents
  predict each other with little communication. For us, the protocol is the shared code plus the draft memory.
- **Desire arbitration with hysteresis.** Valve's Dota 2 bot scripting (https://developer.valvesoftware.com/wiki/Dota_Bot_Scripting)
  has team-level and bot-level evaluation layers, and modes chosen by desire values.
- **Phase → attention → execution.** Wu et al., *Hierarchical Macro Strategy Model for MOBA Game AI*, AAAI 2019
  (https://ojs.aaai.org/index.php/AAAI/article/view/3915): a team-level "where to go" layer above per-hero execution.

**Motivation.**
- **Engagement has no owner.** The leader writes a fight mode from what it sees, often nothing. Then each champion
  near an enemy can opt into FIGHT alone (`turn.cpp:396`), or leave through its own filters (OUTCLASS, `join_first`,
  the chase line, per-bot TEAM_CHASE).
- So a contact is decided by up to five agents with five views. That fits the losses in A2.1: lone first deaths, Vesper
  caught with allies more than 3 tiles away, HoR diving alone.
- **Shared rules have won and per-champion rules have lost** (L1). FOCUS shows that even 4 shared bits help.

**Hypothesis.** One shared engagement state per contact, written by a champion that can see the contact and followed
by everyone, will:
- cut disagreement rounds and entry spread (F0);
- cut lone deaths;
- with the same evaluator, win more fights at equal bodies.

Expected: kills up, deaths down, banked up, farm rounds level (L2).

**Design.**
1. **Split authority by who can see what.**
   - **Target and phase:** the leader, unchanged.
   - **Engagement:** the **contact settler**, the first champion to act in a round that has a near enemy
     (`near_enemies()` non-empty), is alive and is not channelling.
   - A leader that sees no enemy must not overwrite a fresh engagement.
2. **The contract's fields** (recommended layout; justify any other):
   - **State,** in byte 6 bits 6-7 (freed by cutting the claim round to `round & 63`; see A4): NONE 0, GATHER 1,
     ENGAGE 2, DISENGAGE 3.
     - Check every claim-round reader (`route.cpp:1903` ages it with `& 0xFF`) and change it to `& 0x3F`.
     - Show that a wrapped 6-bit age cannot keep a stale claim alive. A claim is dropped at age 15, and every leader
       turn re-checks.
   - **Anchor and age,** in byte 7: the route slot nearest the contact + 1 (bits 0-5) and `round & 3` (bits 6-7), the
     same format as today's ping. **The contract absorbs the ping.** A ping becomes a GATHER proposal (step 4).
   - **Focus,** in byte 4 bits 4-7, unchanged. The settler writes it, and NONE clears it.
   - **Alternative,** if byte 6 must stay whole: use mode bits 0-1 with FIGHT as ENGAGE and RETREAT as DISENGAGE, and
     make GATHER "fresh anchor while mode is FARM". The leader then writes only FARM or TRIBUTE while a contract is
     fresh. This is more coupling; report which you chose and why.
3. **The settler's turn.**
   - The settler evaluates the contact with `score_mode(own position, current)`. F3 can swap in a better evaluator
     later.
   - It writes the state, the anchor, the age (`now & 3`) and the focus.
   - **Hysteresis comes from the contract state**, not from each champion's `local_prev`:
     - entering ENGAGE needs `score >= bar`;
     - staying in ENGAGE needs `score >= bar - FIGHT_HYSTERESIS`;
     - leaving DISENGAGE back to ENGAGE needs `bar + CONTRACT_REENTER`.
   - **GATHER** is chosen when the score fails now but would pass with allies who are healthy, can come (as
     `can_answer`), and are within CONTRACT_ARRIVE rounds (12, the ping's range).
     - Count those allies from `ally_at` / `ally_seen` (last sightings, fresh within 2 rounds), not from current vision.
     - If F3 is merged, it gives this test directly: the evaluation with arrivals.
4. **Everyone else's turn.**
   - **Contact holders acting later in the round** read the contract and **do not re-score** (remove the
     `turn.cpp:396` opt-in under the flag):
     - ENGAGE → `fight()`;
     - DISENGAGE → `retreat()`;
     - GATHER → hold out of enemy reach and wait. This is `join_first` without its own timer; the contract's age bounds
       the wait.
   - **One veto, toward safety only** (joint intentions: make the end mutually known). A later contact holder whose own
     score is under the *stay* bar by more than CONTRACT_VETO may write DISENGAGE. Nobody may write ENGAGE over a
     DISENGAGE in the same round.
     - The veto must be tested both on and off. L3 says retreat cascades are costly.
   - **Self-preservation is unchanged.** The hurt recall and dodge (step 5 of A3), the hurt-carrier rule, and the
     channel rules run before the contract and override it for that champion only.
   - **Champions with no near enemy:** with a fresh ENGAGE or GATHER whose anchor is within CONTRACT_ARRIVE, and
     `can_answer` true, walk to the anchor (today's `answer_ping`); else farm on.
     - Exclusions: the collector, a busy satellite or split pair (`pair_split()`, `spread_out()`; see
       FARM_SPLIT_PLAN), and anyone channelling.
     - With DISENGAGE, keep farming but avoid the anchor's area (reuse OUTCLASS's danger board, `spread.cpp:380`).
5. **Expiry.** A contract whose age is more than CONTRACT_FRESH rounds old (start with 2, to match PING_FRESH) reads as
   NONE. The settler refreshes it every round it sees the contact. When nobody sees the contact any more, it lapses.
   GHOST still runs its own 5-round chase.
6. **OUTCLASS and TEAM_CHASE become inputs to the settler,** not separate exits:
   - The settler's local emblem gap is a veto toward DISENGAGE.
   - The settler's team-chase drop means DISENGAGE, then NONE.
   - Each other champion's own `outclass_step` stays only as self-preservation when that champion is the one alone in
     reach. Decide this and log it.
7. **Log** every state change with NOTE: who wrote it, from which state, why, and the score.

**Code placement.**
- A new `contract.cpp`; `cmake/CMakeLists.txt` globs `src/*.cpp`, so a new file builds as is.
- Read and write helpers in `state.cpp` / `state.h`, next to `read_focus` and `write_focus`.
- The settler hook belongs after `sync_decision` and before step 5 in `Bot::play`.
- The follow rules wrap `turn.cpp:376-467`.
- `update_ping` becomes contract propose and age, with PING_* kept for the flag-off path.
- `sync_decision` (`route.cpp:2155-2176`) must not overwrite a fresh contract with the leader's blind FARM.

**Flags.** `CONTRACT` (bool), `CONTRACT_FRESH` 2, `CONTRACT_ARRIVE` 12, `CONTRACT_REENTER` (start at 5),
`CONTRACT_VETO` (score points; 0 = no veto), `CONTRACT_GATHER` (bool, so GATHER can be tested apart).

**Fuel.**
- Followers stop calling `score_mode` on contact turns, which should *save* fuel.
- The GATHER test reads last sightings and needs no engine reads.
- Report fuel on contact turns separately (F0 can tag them).

**Risks.**
- **GATHER pulls farmers off camps** (L2, FIGHT_COMMIT_PING -12.1%). Gate it on winnability, and test with
  CONTRACT_GATHER off.
- **The settler's view is one champion's.** Rotating settlers each round (whoever acts first with contact) may flap.
  Hysteresis on the contract state is the guard. Count state flips per episode.
- **Turn order.** Ryth (7) and HoR (6) act early, so they settle most contacts. HoR's dive logic (`hor_dive`) must
  respect a DISENGAGE written this round.
- **Two contacts at once.** There is one contract. The anchor tells a second, far contact apart: a champion with its
  own contact more than CONTRACT_ARRIVE from the anchor plays as today. Log how often this happens.

**Test.**
- Run `v216-contract` vs v213 over the pool, 3 seeds, in debug with logs.
- Ablations: CONTRACT_GATHER off; CONTRACT_VETO 0 vs on; the claim-byte change alone with the contract written but not
  acted on (must match v213).
- Report F0's metrics next to the usual ones.

**Success.**
- Disagreement rounds per episode and entry spread clearly down (F0).
- Lone deaths and deaths of Vesper and HoR down.
- Banked +3% or more pooled, with fuel level or lower.

**Open questions.**
- Is "first to act with contact" stable enough, or should the settler stay the same champion while it keeps contact?
  The stamp-style "settled this round" test is `age == now & 3` with a nonzero anchor.
- Should the satellite (FARM_SPLIT_PLAN E3) read the contract?

---

### F3. Fight simulation with arrival times, burst and the price of retreat. Agent: "fightsim". Flag `FIGHT_SIM`

**Sources.**
- **Decreasing-DPF and TS-Lanchester²:** Uriarte & Ontañón, *Combat Models for RTS Games*,
  https://arxiv.org/pdf/1605.05305.
  - The Decreasing model (their Algorithm 1) kills one unit at a time, chosen by a target-selection policy. Each death
    lowers its side's damage. It predicts which units survive and how long the fight takes.
  - Its parameters can be static (from unit stats) or learned. Static rates did best at predicting winners.
- **Lanchester for attack/retreat:** Stanescu, Barriga & Buro, AIIDE 2015,
  https://ojs.aaai.org/index.php/AIIDE/article/view/12780.
- **Script search:** Churchill & Buro, *Portfolio Greedy Search and Simulation for Large-Scale Combat in StarCraft*,
  CIG 2013, https://www.cs.mun.ca/~dchurchill/publications/pdf/combat13.pdf.
  - Search over a few scripts per side instead of over individual actions. Here: engage now, gather then engage, and
    retreat.
  - Scripted targeting policies named there include Attack-Weakest and No-OverKill-Attack-Value.
  - A 2017 follow-up found that PGS works only as well as its forward model.
- **Battlecode 2024:** the *Cout for Clout* postmortem
  (https://battlecode.org/assets/files/postmortem-2024-cout-for-clout.pdf) moved toward "a combat location (inspired by
  Lanchester's laws)" and won by being aggressive at the right moments.

**Motivation.**
- **The live score has no time in it** (A3). Arrival order is patched by FIGHT_LOCAL's 12-tile radius, JOIN_WEIGHT and
  JOIN_FIRST. Spells count as a long-run average (`round_damage`), not as "ready now" burst. Stuns, kill order, bags
  at risk and the value of a death are not in it.
- Health and power are signs only, so a 1-health edge counts the same as 1000.
- **KILL_TIME (v181) was a square-law term** (log of H×D over H×D) and lost for two reasons: its ±20 clamp swamped the
  other terms, and it made the team retreat too often (L3).
- **The fix is not a better attrition number alone.** Simulate both options, fight and retreat, and compare their
  expected values.

**Hypothesis.** An evaluator that simulates the next ~8 rounds, with arrivals, ready burst and stuns, and that prices
retreating too, will separate winnable fights from losing ones better than the term sum. It should beat the current
score in F1's AUC and on the bench without retreating more.

**Design.**
1. **`FightEval Bot::fight_sim(vec2 origin, int state)`**, returning `{ev_fight, ev_retreat, our_deaths, their_deaths,
   rounds}`.
2. **Units:**
   - **Ours:** us, plus allies in sight within FIGHT_LOCAL_RANGE that are not channelling (as `fight_allies`).
     Optionally (SIM_UNSEEN_ALLIES) allies last seen within 2 rounds (`ally_at`).
   - **Theirs:** `near_enemies()`, plus **living enemies not in sight** (SIM_UNSEEN) at their last tile
     (`enemy_at` / `enemy_at_round`), aged by walking one tile per round. Weight them by a discount from F1, or start
     at 0.5.
3. **Per unit:**
   - `hp` = (health + shield) / taken multiplier (`hor_shield_hp`, `champ_taken_mult`).
   - `dps` = strength × (1 + resets/8) × dealt multiplier. Varg's splash counts adjacent enemies, and HoR's damage uses
     his missing health.
   - `burst`: the damage of a ready damage spell (`spell_damage` on the ready slot for us; for enemies, inside a
     try/catch, or assume ready if not seen casting in the last 8 rounds).
   - `stun`: a ready stun and its range.
   - `arrive` = max(0, distance to nearest opposing unit − reach) rounds, minus 2 with a ready dash. For ranged units,
     a round spent walking is a round not attacking.
   - `value` if it dies = its bag + GOLD_W × (100, +50 with Hunter) + RESPAWN_W × 60 × farm_rate (F1; start with
     0.3 gems a round, then use F1's number). Prism's bag also raises her Focus.
4. **The loop,** for rounds t = 0..SIM_HORIZON (8, one cooldown):
   - Units with `arrive <= t` attack their side's current target. Burst lands on a unit's first active round.
   - A ready stun takes away its target's damage for one round, aimed at the enemy with the highest dps in range.
   - Awake camps in reach add their strength to the nearest unit (reuse `camp_heat`'s list).
   - When a unit's hp reaches 0, remove it and retarget.
   - **Our targeting:** the FOCUS target, else the enemy with the highest dps per remaining hp. **Theirs,
     pessimistically:** our lowest effective hp in reach of their front units. These policies are parameters (F4 may
     change them).
   - At most 12 units × 8 rounds = 96 updates, a few thousand fuel. No engine reads inside the loop: read every input
     once, beforehand.
5. **The price of retreat** (L3), in `ev_retreat`:
   - Simulate the same fight in which our units walk away from t = 0. An enemy whose `catch_short(enemy, ours) == 0`
     (`combat.cpp:429`: a dash or stun ready, or we are channelling) gets SIM_CATCH_ROUNDS of free hits on our
     slowest and lowest unit.
   - Then subtract what we give up: the reward being contested (the claim or capture value, if the contact is at our
     target group) and the farm rounds spent leaving.
6. **Decision:** `score = SIM_SCALE × (ev_fight − ev_retreat)`. This feeds the existing bar and hysteresis machinery,
   with its own SIM_BAR and SIM_STAY. Do not clamp: KILL_TIME's clamp is what saturated (A2.3). If F2 is present,
   GATHER is chosen when a fight with `arrive` reduced for allies who can come wins but the fight now loses.
7. **Wire it in:** under FIGHT_SIM, `score_mode` returns the sim's choice. Keep the old body for flag off. Log the sim's
   inputs and outputs in one NOTE per settled contact.

**Code placement.**
- A new `fightsim.cpp`, plus a struct in `bot.h`.
- `score_mode` (`route.cpp:2628`) calls it.
- Reuse `round_damage`, `kit_resets`, `champ_dmg_mult`, `hor_shield_hp`, `ready_moves`, `catch_short` and `camp_heat`.

**Flags.** `FIGHT_SIM` (bool), `SIM_HORIZON` 8, `SIM_UNSEEN` (bool), `SIM_UNSEEN_W` 0.5, `SIM_RETREAT` (bool, prices
retreat), `SIM_CATCH_ROUNDS` 2, `SIM_SCALE`, `SIM_BAR`, `SIM_STAY`, and `GOLD_W` / `RESPAWN_W` / `FARM_RATE`.

**Fuel.**
- The sim itself is cheap. The cost is the reads: `abilities()` per unit is about 100 or more, and `catch_short` reads
  ready moves.
- Cache per round and vision generation (as `score_base` does).
- With F2, only the settler runs it, once a round. Without F2, every champion with contact runs it. Measure both.

**Risks.**
- **Over-retreating** (L3). Watch the retreat share of contact rounds next to banked.
- **Enemy cooldowns are unknown.** The assumption "ready unless seen casting" may make every enemy look like a burst
  threat. Compare it with "never ready".
- **A sim that is too optimistic about arrivals** makes the team walk into fights one by one. Check entry spread with
  F0.

**Test.**
1. **Offline first:** in debug games, log the sim's prediction at each settled contact. Use F0 to score it against the
   actual outcome; report AUC and calibration against the current score's FIGHT/RETREAT choice.
2. **Then the bench:** `v216-sim` vs v213, 3 seeds.
3. **Ablations:** SIM_RETREAT off (to confirm L3 on our own data), SIM_UNSEEN off, burst off.
4. If F2 lands first, also run `v216-contract-sim` (both on).

**Success.**
- The sim predicts contact outcomes better than the current score (F0 and F1 metrics).
- Kills per contact up, deaths per contact down, banked +3% or more pooled, retreat share not up.
- Fuel level.

**Open questions.**
- Can an enemy's ability readiness be read at all? See `ready_moves`' try/catch.
- How is Willow's heal best counted: as negative damage on our lowest unit?
- Is `farm_rate` better as a per-phase constant or measured live (team gems per round so far)?

---

### F4. Team target choice, kept steady, plus peel. Agent: "focus". Flags `FOCUS_VALUE`, `FOCUS_KEEP`, `PEEL`

**Sources.**
- **Targeting scripts:** Churchill & Buro, CIG 2013 (link in F3): Attack-Weakest, Attack-Value, and
  No-OverKill-Attack-Value. Uriarte & Ontañón (https://arxiv.org/pdf/1605.05305) *learn* the kill order from replays
  (Borda count over first kills).
- **Dynamic consistency:** MacAlpine, Price & Stone, *SCRAM: Scalable Collision-avoiding Role Assignment with
  Minimal-Makespan*, AAAI 2015 (https://www.cs.utexas.edu/~AustinVilla/papers/AAAI15-MacAlpine.pdf; definitions in
  https://cs.utexas.edu/~AustinVilla/details/papers/ARMS14-MacAlpine.pdf).
  - An assignment function is *dynamically consistent* if it keeps the same mapping as agents carry it out, so they do
    not "thrash between roles".
  - Greedy nearest-first is not consistent.
- **Peel** (League of Legends): protecting an ally during a fight, often with crowd control aimed at whoever is on your
  carry rather than at the enemy you would kill (https://wiki.leagueoflegends.com/en-us/Peeling). Guides on team
  composition list engage, damage and peel as the three things a teamfight needs
  (https://mobalytics.gg/blog/how-to-peel-in-league-of-legends/).
- **Battlecode 2024 micro rules** (F3 link): move toward lower-health enemies, toward allies that were stunned, and back
  when an ally behind you has more health.

**Motivation.**
- FOCUS won +8.6% with a crude choice: the first champion to pick a target writes the lowest-health enemy in its reach
  or sight. The focus is never cleared, and champions in separate fights overwrite it (A3).
- Lowest health is not the best kill. A full-health Vesper dies to our burst faster than a half-health HoR with a
  shield. Killing the enemy that deals the most damage per health point stops the most incoming damage.
- Our carry dies to melee divers (A2.1: Vesper, melee at 1-2 tiles). VESPER_LEASH (bringing her to the team) lost
  -3.3%. The other way round, sending the stun to the diver, is untested.

**Design.**
1. **FOCUS_VALUE.** The champion that writes the focus (the F2 settler if present, else today's writer) picks the near
   enemy that maximises
   `(value_if_killed + threat × FOCUS_THREAT_W) / rounds_to_kill`, where:
   - `rounds_to_kill` uses our allies' damage that can reach it, with arrivals (from F3, or distance minus reach);
   - `threat` is its damage per round;
   - a channelling enemy in reach gets a bonus (its bag is the prize, as today).
2. **FOCUS_KEEP** (dynamic consistency).
   - Keep the current focus while it is alive and in any champion's near list, unless another enemy scores
     FOCUS_KEEP_MARGIN (1.3x) better.
   - **Clear it** when the contract ends (F2) or no one has seen it for 2 rounds.
   - A champion that cannot see the focus picks its own target locally **without overwriting** the shared one, unless
     it is the settler. This stops two fights overwriting each other.
3. **PEEL.** A champion with a ready stun or hit (Ryth's area stun, Marcella's enhanced reset, Prism's, HoR's landing)
   uses it on any enemy melee within 2 tiles of our Vesper, Prism or Willow before the focus.
   - Ryth and Marcella act early (priority 7 and 3; Vesper is 2), so the stun lands before Vesper's turn.
   - Log each peel, and whether the carry took a hit afterwards.

**Code placement.**
- Target pick: `combat.cpp:213-251` in `fight()`.
- The focus read and write helpers: `state.cpp:286`.
- Kit casts: `ryth_stun` (`kits.cpp:628`), `marcella_cast` (`:651`), `mage_cast` (`:891`) for Prism, and `hor_dive`
  (`hor.cpp:147`).

**Flags.** `FOCUS_VALUE`, `FOCUS_THREAT_W`, `FOCUS_KEEP`, `FOCUS_KEEP_MARGIN` 1.3, `PEEL`, `PEEL_RANGE` 2.

**Fuel.** The near list holds 1-5 enemies, so the scoring is tiny. The reads are already made for the fight score.

**Risks.**
- A high-value target at the back drags our melee through their front line. Count only allies who can reach it within
  2 rounds.
- A peel stun spent defensively is not available for a kill. Compare kills with PEEL on and off.

**Test.** `v216-focus` (FOCUS_VALUE + FOCUS_KEEP) and `v216-peel` (PEEL alone), each vs v213, 3 seeds. Then both on
the F2 build if F2 passes.

**Success.**
- Focus changes per episode down (log them).
- Kills per contact up, Vesper deaths down (PEEL), banked +2% or more pooled.

**Open questions.**
- Does Ryth's area stun at range 4 reach a diver who is next to Vesper from where Ryth usually stands?
- Should HoR's dive land on the diver (peel) or on the backline (engage)? Note HOR_DIVE_FOLLOW's 2-ally rule.

---

### F5. Per-tile fight movement for ranged champions. Agent: "micro". Flags `MICRO_VESPER`, `MICRO_RANGED`

**Sources.**
- **XSquare's micro, through the 2024 postmortem** (*Cout for Clout*, F3 link): "look at any direction a robot could
  go, and for any direction, have a heuristic to weigh how good a square was".
  - The team that switched from if-statements plus a combat BFS to it calls it "this simple idea and clean framework",
    a "crucial advantage" that took them to the top of the leaderboard.
  - Their additions: bucket behaviour by cooldowns and ranges; move toward lower-health allies and enemies; step back
    when the ally behind has more health; don't clump.
- **Kiting with influence maps:** Uriarte & Ontañón, *Kiting in RTS Games Using Influence Maps*, AIIDE 2012
  (https://ojs.aaai.org/index.php/AIIDE/article/view/12544). Attack-and-flee movement from a threat map.
- **Utility considerations:** Dave Mark's Infinite Axis Utility System (https://gameai.com/iaus.php; overview at
  https://en.wikipedia.org/wiki/Utility_system). Each factor is normalised to 0-1 through a response curve, and the
  factors are combined, so new ones can be added without rewriting branches.

**Motivation.**
- Vesper's fight movement is a stack of separately tuned rules: `engage_vesper`, `vesper_kite`, `vesper_follow`,
  `vesper_retreat`, `vesper_escape`, GO_KITE, CHASE_DASH_VESPER.
- Several of them won alone (GO_KITE and the urgent getaway dash +9.5%; VESPER_ESCAPE carries the v137 merge), and
  each new one has to work around the others.
- She still dies 3.9 times a game in losses, mostly from above 60% health (A2.1). That is a positioning failure before
  it is an escape failure.
- One comparator over a few candidate tiles is the Battlecode answer to exactly this kind of rule stack.

**Design.**
1. **Candidates:** stand still, the 8 neighbours, and with a ready dash the dash landings (`go()`'s landing logic gives
   them).
2. **Boards built once per turn** (each about 100-300 fuel):
   - `enemy_reach` (`kits.cpp:1062`): enemy basic reach next turn;
   - enemy spell reach (`recall_zone`'s reach);
   - awake camp reach (`heat_reach`);
   - tiles from which the focus, or any enemy, is within 3;
   - "covered" tiles: a sturdy ally is closer to every enemy that reaches the tile.
3. **Features per tile:**
   - the number of enemies that can hit it next turn, and their summed damage;
   - whether it hits the focus, or any enemy;
   - covered or not;
   - the distance to the nearest sturdy ally;
   - inside camp reach or not;
   - for a ranged champion, whether moving costs this turn's shot.
4. **The comparator depends on the state** (F2's contract, or the shared mode without F2):
   - **ENGAGE:** take no hits it cannot afford (damage taken under DMG_BAR × health); then hit the focus; then covered;
     then fewest enemies reaching it; then nearest the focus.
   - **GATHER / hold:** out of reach first, then nearest the anchor or the allies.
   - **DISENGAGE** stays with the existing `retreat()` in v1.
   - Write it as an ordered comparator (Battlecode style) or as an IAUS product, and say which and why.
5. **Scope.** Vesper first (MICRO_VESPER replaces `engage_vesper`'s move choice; firing stays as is). Then the other
   ranged fighters: Willow's backline, Prism and Stellar's `mage_fight` (MICRO_RANGED). Melee champions are out of
   scope.

**Code placement.** A new `micro.cpp`, called from `engage_vesper` (`kits.cpp:777`) and `mage_fight` (`kits.cpp:1071`).
Moves go through `go()` with a single-tile goal, or through its `rank` hook for dash landings.

**Flags.** `MICRO_VESPER`, `MICRO_RANGED`, `MICRO_DMG_BAR` (share of health).

**Fuel.** Five or six boards plus 9-11 tiles × a few `is_set` calls is about 2-4k. That replaces several `go()`
searches (about 3k each), so it may come out cheaper. Measure it.

**Risks.**
- This replaces tuned winners. Compare against v213 on Vesper's deaths, kills and damage dealt, not just banked.
- The dash landings must keep the hole and pocket rules (DASH_REACH, the hop rules in `go()`).

**Test.** `v216-micro-vesper` vs v213, 3 seeds. Then `MICRO_RANGED`. Then on top of the best of F2 and F3.

**Success.**
- Vesper deaths per game down at the same or better damage dealt.
- Banked +2% or more pooled; fuel level.

**Open questions.**
- Does `go()` expose the dash landings it would consider, or must they be enumerated here?
- Should firing from the current tile always beat moving when the focus is in range and no hit lands next turn?

---

## Part C: order, merge and hand-back

**Order and parallelism.**
- **F0 and F1 start at once** (analysis only). F0 is small; finish it first, since every other brief reports its
  metrics. F1 shares the replay decoder with F0.
- **F2 and F3 start at once on v213**, as independent flags.
  - F3 uses F1's coefficients when they arrive. Start with static values and rerun the best variant with F1's.
  - F2 uses the current score until F3 is ready.
- **F4** can start on v213 (FOCUS_VALUE, FOCUS_KEEP and PEEL do not need F2). Its "clear on contract end" part waits
  for F2.
- **F5** starts after F2 if possible, since it needs the contract state; on v213 it can use the shared mode instead.
- **Machine load:** both plans' agents share 20 cores, so every agent uses `--jobs 4` (A6.4).

**Merge** (the main session, not a subagent).
1. Combine the kept flags into `bots/v217`. The likely stack is F2 + F3 (+ F4, F5). Bench it against v213 and against
   each single winner. Memory `small-map-plan.md` notes that winning switches do not always add up.
2. If `FARM_SPLIT_PLAN.md` has produced `bots/v215` by then, also bench v217's flags on top of v215. F2's contract has
   to respect the split satellite (`pair_split()` / `spread_out()` exclusions).
3. Write `bots/v217/README.md`, and save a memory note for each finding with its numbers, as earlier findings were kept.

**What goes back to the user.**
- **Fight changes are the most ladder-sensitive kind** (L4). Local opponents are never far apart in emblems, and two
  fight-score changes (EMBLEM_GAP, KILL_TIME) won or broke even locally, then lost on the ladder. A local win is not
  enough.
- The final candidates need the platform test set (memory `platform-testing.md`: the 10 teams we lose to most, on their
  maps, sides a,b,a,b,a), compared with the live entry's results on the same set.
- Submitting, activating and queuing are the user's call. Each step uses quota or changes the live bot. Hand back the
  bench tables and the proposed platform test, then wait.

**Out of scope here, but noted.**
- Learning a full policy (OpenAI Five's "team spirit" reward blend, https://arxiv.org/abs/1912.06680) is beyond our
  fuel and tooling. Its lesson, that each agent should judge actions by the team's outcome, is what F2 and F3 encode by
  hand.
- A deeper script search (several scripts per side and best responses, Portfolio Greedy Search) could follow F3 if
  fuel allows. Only the settler would run it, once a round.
