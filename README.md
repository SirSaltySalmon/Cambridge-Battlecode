# Cambridge-Battlecode

Bots for CAMBC (cambc2026a). Game rules: `GameRules.md` / `CLAUDE.md`; C++ API: `BotAPICpp.md`. One folder per version
under `bots/`; each main version's README says what it adds, what it was benched against and the results. Bench notes
are in `results/`, the SPSA tuner in `tools/evotune/`.

## Branch final-stretches

Merges jack-claude (Jack's v150-v213 work) and vargvale-strikes-back (v138-v203) over their common base, v137.

- Jack's `bots/v150` (v149 + Optuna fight trial 43) was relabelled **`bots/v150-jack`**: vargvale's `bots/v150` (v149 +
  small-map switches) is the parent of v152 and kept its name.
- 99 local test folders that never shipped were removed. Each one's settings and results are recorded in the README
  of the main version it belongs to (table below); the code is in git history (commit a31dc30 and before).
- Folder numbers are local names; platform version numbers are given where known and do not always match.

## Versions after v137

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

Tools: `bots/zz-facts` dumps map facts (champions, altars, spawn-to-spawn steps) used by `bots/v150/analyze_maps.py`;
`bots/mapdump` is the older map dumper.

## Seed copies

Local games are deterministic, so most benches pool three "fuel-burn seeds": seed 0 is the bot itself, and `-s1` /
`-s2` copies add `{ volatile int sink = 0; for (int i = 0; i < 300; ++i) sink += i; }` before `cambc::end_turn()` in
`src/main.cpp`, which shifts fuel timing and reshuffles every timing-dependent outcome. Those copies are not kept;
recreate them from the main folder with that one line.
