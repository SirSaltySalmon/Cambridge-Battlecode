# Cambridge-Battlecode

Our code for CAMBC 2026a (Cambridge Battlecode), from team **"opsec level opera gx"**, October to November 2026.

- Platform: <https://2026a.battlecode.co.uk>
- Organiser: <https://battlecode.co.uk>
- Rules: [GameRules.md](GameRules.md), also copied into [CLAUDE.md](CLAUDE.md). We used the `cambc` CLI, SDK 5.0.1.

Contributors (GitHub): SirSaltySalmon, Jack, amai-ua, CantCode4Nothin.

---

## The game

Each team drafts 3 to 5 champions and sends them to kill camps of monsters. Once a camp is cleared, standing on it for
five rounds captures its gems and gold. Gems only count once you tribute them at an altar, and a champion who dies
loses what it carries. Gold buys stat shards, and five of a kind make an emblem with a passive. The first team to 1,000
tributed gems wins, otherwise whoever has more at round 2,000.

Every champion runs its own copy of the bot, sees only what it can see, and gets 100k fuel (roughly instructions) a
turn. The team shares 8 bytes of memory, plus 1 KB written during the draft.

## What we built

We started in Python, moved to C++ at v63 (17 October) because we kept running out of fuel, and finished at v235. The
final bot is about 18,500 lines of C++ with 270 on/off switches.

- [`bots/v230`](bots/v230/README.md) was our live bot from 11 November. [`bots/v235`](bots/v235/README.md) is the best
  one locally.
- We peaked at rank 29. At the end of the event, we were rank 121, at rating 1,998.

How the bot works:

- **Draft** (`draft.cpp`) picks a roster for the map: usually Vesper Vale as the gold carry, a frontline from Heart of
  Ruin, Ryth, Marcella and Varg, Willow for heals, and Prism on gem-cluster maps. It writes an opening plan into draft
  memory.
- **Farming** (`route.cpp`, `swap.cpp`, `spread.cpp`) scores camps by value, distance and danger, and claims them in
  team memory so two champions don't go for the same one.
- **Fighting** (`combat.cpp`, `kits.cpp`, `hor.cpp`) uses a hand-tuned fight score, focus fire, help pings, and a short
  memory of enemies that just went out of sight.
- **Economy** (`shop.cpp`, `tribute.cpp`) sends most gold to Vesper, plans emblems and decides when to bank.

Tools:

- [`tools/hc`](tools/hc) runs local benches and platform A/B tests against fixed opponents, plus scripts that read our
  logs.
- [`tools/replay`](tools/replay) decodes ladder replays with the engine's own wasm, so we could see what opponents did.
- [`tools/evotune`](tools/evotune) tunes the fight-score numbers with SPSA.
- Each `bots/<version>/README.md` says what changed and how it benched.

---

## Post-mortem

### Timeline

| Dates | What we did |
| --- | --- |
| Oct 14-17 | Python bot, then the port to C++. |
| Oct 18-24 | Farming and fight basics, and the Vesper gold funnel. |
| Oct 24-28 | Movement rewrite, enemy memory, golem gate. Our best stretch. |
| Oct 27-28 | First look at ladder replays. Gains in self-play weren't showing up on the ladder. |
| Oct 28-Nov 4 | Two branches worked in parallel, then a research-based fight plan (v216-v218) that lost. |
| Nov 4-11 | Wards, teleport crystals, survival fixes found in replays (v230), and fight re-tests that helped locally but not on the ladder. |

### What worked

- **C++.** The Python bot ran out of fuel as soon as it did any real planning.
- **Reading engine values once a turn.** `position()` and `health()` cost about 100 fuel each, and helpers were calling
  them inside loops. Caching them halved our out-of-fuel turns.
- **One movement function** (v133, +9.5%). Everything moves through `go()`, so a fix to dashing or kiting applies
  everywhere.
- **Deleting rules** (v135, +30.8%). We audited movement and removed rules that fought each other. Our largest single
  gain came from taking code out.
- **Remembering enemies** for a few rounds after losing sight of them (v140, +10%).
- **Only fighting golems when strong enough** (v142, +5%). Deaths per golem clear fell from 28% to 10%.
- **Waiting for allies before fighting** (v119, v124).
- **Platform tests with opponents pinned to one version.** Games are deterministic, so only the games our change
  touched come out different, which makes small changes readable.
- **Fixes found by watching replays** (v230, +6% gems).

### What didn't

- **Tuning against ourselves.** Fight numbers tuned in self-play beat our own copy and lost to older versions (74/240).
- **Sending all the gold to Vesper.** It won self-play 42-28, because both sides did it. On the ladder opponents itemised
  every champion, so our frontline fought without items and the economy died whenever Vesper did.
- **The research fight plan** (v216-v218, 34-52). We added four paper-based systems on top of the existing rules at
  once. Some were buggy (the engagement contract quietly turned off the retreat ping), and others overrode rules they
  had never been tested against.
- **The Prism team** only helped on nine gem-cluster maps.
- **Farming solo, spreading out, Vendetta, Greed, Cassi & Issac** all lost or came out level. Lone champions die.
- **Better fight coordination** (v231, v235): 29-21 locally with 14% fewer deaths, no change on the ladder.
- **About 99 per-map switches.** A few helped on a handful of maps. Most did nothing once the map pool changed.

### Why we fell short

We lost on economy, early, not on fights.

- By round 150 in games we lost, we had earned 398 gold to their 580 and banked 19 gems to their 53. Kills and deaths
  against teams near our rank were about even.
- Vesper had 872 gold by round 300 and everyone else under 140. Opponents spread 200-440 across every champion.
- We banked about 30% of what we picked up early, they banked 65-70%. Our carried gems got stolen.
- We farmed near our own altar until the team felt strong enough, and lost the middle: 54 gems to their 98 by round
  300 in losses.
- 15-17% of our turns were spent waiting for a camp to respawn, and 15-25% walking between camps as a group. Unopposed,
  we needed 1,129 rounds to reach 1,000 gems. Top teams won real games in a median of 738.
- The top five teams caught us in packs. 78% of our early deaths were outnumbered, often with an ally 6-14 tiles away
  doing nothing.

We spent most of our time on fights because they're easy to see in logs and easy to test locally. The real gap was
farming speed and how we spent gold, and that only shows up against teams that play differently from us.

### What we learnt

**The biggest thing we'd change: look up existing solutions before writing our own, rather than reinventing the
wheel or asking AI agents to invent something.**

**1. Research first.** Most of our hard problems already have names and papers. We wrote our own versions anyway.

| Problem | What we wrote | What already existed |
| --- | --- | --- |
| Who farms which camp, without talking | A target score, then claims, swaps, gates and ~100 switches on top | Multi-robot task allocation, e.g. sequential greedy assignment that every bot computes the same way ([Singh et al., JAIR 2009](https://arxiv.org/pdf/1401.3462v1.pdf)) |
| Routing between camps that respawn | Go to the best camp, wait if it's down | Orienteering with release dates ([arXiv 2207.00885](https://arxiv.org/pdf/2207.00885)) |
| Farming without dying | Hard gates on enemies and emblem counts | Team Surviving Orienteers ([arXiv 1612.03232](https://arxiv.org/pdf/1612.03232)) |
| Fight or run | Ten hand-tuned weights | Lanchester combat models ([AIIDE 2015](https://ojs.aaai.org/index.php/AIIDE/article/view/12780), [Uriarte & Ontañón](https://arxiv.org/pdf/1605.05305)) |
| Where the team should go | The best camp near our own altar | Macro strategy for MOBAs ([Wu et al., AAAI 2019](https://arxiv.org/abs/1812.07887)) |
| Kiting and spacing | Separate escape, chase and kite rules | Influence maps ([AIIDE 2012](https://ojs.aaai.org/index.php/AIIDE/article/view/12544)) |
| Enemies out of sight | Our ghost memory (this one worked) | Particle models for RTS games ([AIIDE 2011](https://ojs.aaai.org/index.php/AIIDE/article/view/12424)) |

We only read these in November, after 200 versions of our own rules, and bolting them on in v216 failed. They would
have worked as the base of the bot, not as a layer on top. Where we did reuse existing work, like SPSA and Optuna for
tuning or the engine's wasm for replays, it paid off quickly. The papers are collected in
[FARM_SPLIT_PLAN.md](FARM_SPLIT_PLAN.md), [FIGHT_COORD_PLAN.md](FIGHT_COORD_PLAN.md) and [WARD_PLAN.md](WARD_PLAN.md).

The same goes for AI agents. Ask one to "fix this" or "farm faster" and it adds another rule to whatever code it's
looking at, which is a big part of how we got to 270 switches. Agents are good at implementing a known algorithm from
a paper and a clear spec. They don't replace finding out what the best known method is.

**2. Build general systems, not fixes.** Our best results came from general pieces: one movement function, one cached
view of the board, one memory of enemies, and an audit that removed conflicting rules. Our worst came from patches for
one map, one camp type or one champion. By the end `route.cpp` was 3,600 lines with functions near 300 lines, and new
ideas kept losing because they ran into something tuned 50 versions earlier. One farming model (what each champion is
worth on each camp, whether it can solo it, short routes, risk as expected gem loss) would have let us fix the real
problems in one place. [FARM_SPLIT_PLAN.md](FARM_SPLIT_PLAN.md) describes it. We never built it.

**3. Test against real opponents.** Self-play and the ladder disagreed on almost every change after v145. A mirror
match can't show a mistake both copies make. A pool of older versions was better, and a fixed set of platform opponents
better still.

**4. Know your noise.** A 256-game local bench moves ±4% between builds that play the same, and 100 platform games move
by 4-5 wins. We often kept or dropped changes on smaller differences than that. Deaths and gem curves are steadier than
win counts.

**5. Choose the metric carefully.** Banked gems made Prism look bad, because Prism holds gems on purpose. Lead with
wins, then gems collected, gems lost and K/D.

**6. A loss might be a bug.** Several "bad ideas" turned out to be bad code: a shared-memory byte with two readers, a
rule that never fired, a cache that reset every turn. Read the logs before dropping an idea.

**7. Look at replays early.** Everything in "Why we fell short" came from replays we decoded in late October. With that
tool in week one, we'd have spent November on farming instead of fights.

**8. Design for fuel.** With no LTO, a call into another file is never inlined. An engine read costs about 100 fuel, a
bitboard 250, a distance field 1,550. Cheap systems left room for wards and crystals later. The fight simulator was
too expensive from the start.

**9. One change at a time, in code you can read.** Stacking four fight changes in v218 meant we couldn't tell which one
lost. Copying whole folders per version kept baselines safe, but left about 360 folders and painful merges after two
branches drifted apart for two weeks. [CLAUDE.md](CLAUDE.md) has the rules we'd start with next time.

### If we did it again

1. Look up the published method for each part of the bot first, and have agents implement it from the paper.
2. Build the replay decoder and a fixed set of real opponents in week one.
3. Make farming one assignment model, with fights as a short override.
4. Spread gold across the frontline, bank early, and take the middle before round 300.
5. Keep the code small: refactor before adding, and delete switches that lose.
6. Question local self-play and test with real opponents as often as we can.

---

## Repository layout

- `bots/<version>/`: one bot version, copied forward whole. Its `README.md` is the record of the change, bench and fuel.
- `src/bot.h` declares the single `Bot` class; each `.cpp` implements one part of it. `src/constants.h` holds switches,
  `src/evo_constants.h` the tunable fight-score numbers.
- `results/`: bench notes. `tools/`: bench, replay, tuning and patch tools. `maps/`: the map pool.
- Plans: [FIGHT_COORD_PLAN.md](FIGHT_COORD_PLAN.md), [FARM_SPLIT_PLAN.md](FARM_SPLIT_PLAN.md),
  [WARD_PLAN.md](WARD_PLAN.md). C++ API: `BotAPICpp.md` in the SDK.
- Tools: `bots/zz-facts` dumps map facts (champions, altars, spawn-to-spawn steps) used by `bots/v150/analyze_maps.py`;
  `bots/mapdump` is the older map dumper.

### Branch final-stretches

Merges jack-claude (Jack's v150-v213 work) and vargvale-strikes-back (v138-v203) over their common base, v137.

- Jack's `bots/v150` (v149 + Optuna fight trial 43) was relabelled **`bots/v150-jack`**: vargvale's `bots/v150` (v149 +
  small-map switches) is the parent of v152 and kept its name.
- 99 local test folders that never shipped were removed. Each one's settings and results are recorded in the README
  of the main version it belongs to (table below); the code is in git history (commit a31dc30 and before).
- Folder numbers are local names; platform version numbers are given where known and do not always match.

### Versions after v137

| Folder | What it is | Removed test folders recorded here |
|---|---|---|
| v138 | v137 + diagonal chase, CATCH_ACROSS, getaway | |
| v140 - v143 | residual vision (ghost), home-aware spawn rules, golem gate, Willow regroup | |
| v144 | v144v: CONTEST_VALUE on, MIDDLE_FIRST off. **Platform v143** | v144-s1, v144-s2 |
| v145 - v147 | AWAKE_READ, HOR_DIVE_FOLLOW, RETREAT_LEAD off | v147-s1, v147-s2 |
| v148-normal | v145 + team farming swaps + measured camp tie rule | v148-144 (-s1, -s2), v148-145, v148-147 (Varg farm plan), v148-evo |
| v149 | v148-normal + FIGHT_THRESHOLD -5 | |
| v150 | v149 + small-map switches (all off = v149) | the v150-* switch sweep (80 folders with seed copies), zz-lock |
| v150-jack | Jack: v149 + Optuna fight trial 43 (was bots/v150 on jack-claude) | |
| v151-jack | Jack: v150-jack + Optuna economy trial 77 | |
| v151-which-is-v149tuned | v151: v149-cap (capped emblem-ratio term) + evotune numbers. **Platform v151** | v149-cap, v149-ratio |
| v151-mapfix | v151 + four map fixes. **Platform v154** (ladder A/B) | |
| v152 | v150 with HOR_DIVE_FOLLOW, locks, Vendetta-3 on (= v150-c1). **Platform v152** | |
| v155 | v152 + v151-tc builds + mapfix + v150-jack fight numbers (was v152-jf) | v151-tc, v152-tm, v152-jfe, v152-cap, v152-evo, v152-evof |
| v157-monolith | Jack: v155 + monolith deadlock fixes and sessions (local v161) | |
| v170 | v155 + the Prism team on gem-cluster maps. **Platform v170** | |
| v182 | **Platform v182**: common parent of v185 and v206 (v156-v181 steps documented in its `src/constants.h`) | |
| v185 | Mike: v182 + KILL_TIME off + v170's Prism cash-in and economy + focus fire, join fix, draft map block | |
| v185-local - v201 | 2026-11-02 stepwise ladder test from v182 (20 games each), not adopted; v189 was the best step | |
| v202 | v170 + farming changes from the Fish replays + pre-armed recall reading. **Platform v210** | |
| v203 | v202 + multiclear (BUSY_JOBS). **Platform v211**, not adopted | |
| v206 | Jack: v183 (v182 + PRISM_SAT) + OUTCLASS (leave out-emblemed fights) | |
| v208 | Jack: v206 + Mike's v185, 3-way merge over v182. v210 builds on it as "v209" (v210 differs only by OUTCLASS_FIRST) | |
| v210 | v209 + OUTCLASS_FIRST. **Platform v212** | |
| v213 | v210 + FILL_WIDE. **Platform v213** | |
| v220 | v213 + wards, teleport crystals, focus by value, shop and gate changes. **Platform v219** | |
| v230 | v220 + survival and cluster-map fixes (2026-11-10 hill climb). **Platform v230, live on 2026-11-11** | |
| v231 | v230 + RALLY + WILLOW_SHOOT (2026-11-11 fight-plan re-test). Platform v231: level with v230 | |
| v235 | v231 + RALLY_HIT. Platform v233: level with v230 (25-75 against 24-76), best locally. Newest candidate | |

### Seed copies

Local games are deterministic, so most benches pool three "fuel-burn seeds": seed 0 is the bot itself, and `-s1` /
`-s2` copies add `{ volatile int sink = 0; for (int i = 0; i < 300; ++i) sink += i; }` before `cambc::end_turn()` in
`src/main.cpp`, which shifts fuel timing and reshuffles every timing-dependent outcome. Those copies are not kept;
recreate them from the main folder with that one line.
