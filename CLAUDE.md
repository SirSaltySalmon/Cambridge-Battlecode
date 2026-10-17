# Cambridge Battlecode (cambc2026a) — team notes for Claude Code

## Project

- Python bot for Cambridge Battlecode, event cambc2026a. Team: Mike (me) + Nic + Salmon (repo owner, GitHub SirSaltySalmon) + a teammate who does logging.
- Repo: `D:\Cambridge-Battlecode-1` (clone of github.com/SirSaltySalmon/Cambridge-Battlecode). One folder per bot under `bots\` (each has `src\bot.py, draft.py, main.py, randomness.py`).
- My branch: `mike-and-claude-hot-yaoi-sex` (planned rename: `mike-claude`). Changes reach `main` only through a pull request that Salmon/Nic agree to.
- SDK: `cambc` CLI 2.2.5 (standalone install, no Python/pip on this PC). The project's `sdk/` is from 2.2.2: ignore the warning, do NOT run `cambc init` unless the team agrees (it rewrites `sdk/` in the repo). 2.2.3 and 2.2.5 give identical results.
- Maps are not in Git: `cambc maps pull` downloads them into `maps\`.
- Rules: `docs\GameRules.md`, API: `docs\BotAPIPython.md` (read these before answering rule questions).
- I'm still learning Git/GitHub/VS Code: when a change should go to GitHub, include the steps (branch, commit, push, PR).
- I use PowerShell in the VS Code terminal. Git's editor may be Vim (`:wq` to save and close).

## Team rules

- Identity: Vesper Vale-centred team (gold generation) + situational decision making. Don't propose dropping Vesper.
- Builds / shop / emblems are Nic's job. Logging is my teammate's job — don't touch logging code.
- Keep changes small and cheap on fuel (Python: 2.5M fuel per turn = 25x the C++ 100k; running out suspends the bot mid-turn and it resumes next round). No per-turn full-map searches; cache per round.
- One change at a time, so tests show what helped.
- `draft.py` uses Windows line endings (CRLF); keep them when editing.

## Git notes

- `__pycache__/` folders keep getting committed and block pulls. Never commit them (`git add bots/<bot>/src/*.py`); `__pycache__/` should go in `.gitignore`.
- Pull blocked by local changes: `git stash push -u -m "..."`, `git pull origin main`, `git stash pop`.
- `bots/v52/v51/` on main looks like a misplaced copy of v51.

## Testing

- Games are deterministic: same bots + map + side = same result. Side matters a lot (side A wins about 2/3 of games), so always play every map from BOTH sides.
- Noise: treat ±3–4 wins out of 34 as noise. Many games are identical from both sides when a change never triggers. Also compare total gems and deaths, not just wins.
- Before a test, check that the two bots differ ONLY in the change being tested: `git diff --no-index --stat bots\OLD\src bots\NEW\src`
- Run one game without hiding errors first: `cambc run NEW OLD --map packs` (an empty results file means every run failed, e.g. a bot folder is missing).
- Test loop (all 17 maps, both sides, UTF-8 output, prints a TOTAL line):

```powershell
$old  = "OLD"
$maps = Get-ChildItem maps\*.map26a | Where-Object { $_.BaseName -notmatch '-[0-9a-f]{8}-' }
foreach ($new in @("NEW")) {
  $out = "results-$new-vs-$old.txt"
  Remove-Item $out -ErrorAction SilentlyContinue
  $w = 0; $l = 0
  foreach ($m in $maps) {
    $n = $m.BaseName
    foreach ($game in @(@($new, $old, "A"), @($old, $new, "B"))) {
      $lines = cambc run $game[0] $game[1] --map $n 2>$null | Select-String "wins|  A  |  B  "
      "== $n | $new as side $($game[2]) ==" | Out-File $out -Append -Encoding utf8
      $lines | ForEach-Object { $_.Line } | Out-File $out -Append -Encoding utf8
      $win = ($lines | Where-Object { $_.Line -match "wins" } | Select-Object -First 1).Line
      if ($win -match [regex]::Escape($new)) { $w++ } elseif ($win) { $l++ }
      Write-Host "$new | $n | side $($game[2]): $win"
    }
  }
  $total = "TOTAL $new vs ${old}: $w wins, $l losses"
  $total | Out-File $out -Append -Encoding utf8
  Write-Host $total -ForegroundColor Yellow
}
```

- Result row columns: side, bot, gems tributed, kills, deaths, gold earned, crashes, draft. Win line: `Team A (bot) wins: <reason>`.
- Bot logs: a `.repl26a` replay is a ZIP containing `team-a.stdout` / `team-b.stdout` (everything the bots print, including the per-turn decision log), plus `replay.repl26a`, `.debug` and `.fuel`. Analyse logs from there instead of adding prints to the bot.
- Maps: ambush, collision, contest, corners, eight, elkring, expanse, fragment, interlock, lanes, monolith, packs, pilgrimage, rows, rushbait-v2, stampede, swirl (duplicates with a long code in the name are identical copies).
- 5-pick maps: ambush, contest, corners, eight, elkring, lanes, monolith, pilgrimage, stampede. 4-pick: interlock, rows, rushbait-v2, swirl. 3-pick: collision, expanse, fragment, packs.
- Fuel per turn: the replay's `team-a.fuel` / `team-b.fuel` = 8-byte magic + 5 header bytes, then one little-endian u64 per entry. Entries 0–12 are config/draft, then one entry per bot turn in order (bots per round = picks + 1, since Cassi & Issac are two bots). 2,500,000 = the turn ran out of fuel. No Python on this PC, so decode with perl: `perl -e 'local $/; open F,"<:raw",$ARGV[0]; $d=<F>; @v=unpack("Q<*", substr($d,13)); ...'`.
- Pure fuel optimisations (no intended behaviour change) can be proven exact: run old and new with `--fuel 100000000 --log-limit 100000000` (nothing ever runs out) and compare `team-a.stdout` / `team-b.stdout` byte for byte; identical logs = identical decisions.
- Server match JSON exports (match-*.json) can be analysed. Metric ids (inferred, cross-checked): 0 gold earned, 1 spent, 2 gold held, 3 gems held, 4 gems collected, 5 gems submitted, 7 kills, 8 deaths, 9 assists, 10 monster dmg done, 11 monster dmg taken, 12 champion dmg done, 13 champion dmg taken, 14 healing, 16 shielding. Event kinds: 1 death, 2 kill, 3 assist, 5 shop buy, 9 tribute (value = gems), 10 capture (value = gems), 12 emblem.

## API facts (verified in games, not just from docs)

- `cambc.champions(Team.ALLY)` returns ALL allies, including out of vision (the SDK docstring saying "visible" is wrong). `is_alive()` works for every ally; `champion_name()`, `position()`, `health()` raise "Unit is not in vision" for allies out of sight. So `team_alive()` in bot.py is correct.
- `cambc.vision.champions(...)` / `cambc.vision.camps()` return only what this champion sees.
- Camps have `is_awake()` / `is_asleep()` (no need to guess from health < max).
- `self.emblems` in bot.py has one key per ally ever seen = the full team size (all start together on spawn).
- Fuel costs measured in game (Python, 2.5M per turn, 5M on the first play turn): empty function call ~1–2k; `me.position()` / `health()` / `champion_name()` ~2.5–3.5k; `item_count()` ~7k; `cambc.champions(ALLY)` ~55k; `vision.champions(ALLY)` ~58k; `vision.camps()` 13k+; `game_map.camp_groups()` ~0.4M; full-map `DistanceField.compute` ~660k, `compute_to` a far goal ~300k; one `print()` ~18k; any per-tile Python loop ~5k per tile (a `sorted(key=...)` over 300 tiles cost ~3M). The `cambc.is_draft()` call on the first turn costs ~0.4M (SDK cache load).

## Bot versions and results

- v55 = v49-salmon. v58 = v55 + Nic/Salmon shop changes (Varg own build, Issac Durability Health, Cassi/Willow Power Focus first, Health fallbacks). v58 vs v55: 20–14, gems 26,159 vs 22,727, deaths 645 vs 744.
- v58-monolith = v58 + monolith gate fix in bot.py: dead allies count as missing (team size = `len(self.emblems)`), no monolith before round 1000 (`MONOLITH_ROUND`), start only with the whole team within 6 tiles at 80%+ health and no enemy in sight, keep going while 3+ allies stay. Results: vs v58 19–15 (gems 25,368 vs 23,365, deaths 664 vs 743); vs v55 20–14 (same as v58). Safe, slight improvement. Likely effect: stops v58's early monolith fights (v58 allows the monolith from 12 emblems, at any round, as on-the-way loot) — hypothesis, not yet verified.
- v58-monolith-low (monolith only when no other camp is up within 10 rounds): 17–16 vs v58. Not better than v58-monolith.
- v58-dmg5 = v58-monolith + `DAMAGE_COST = 0.05` in draft.py (was 0.01). vs v58-monolith: 21–13, gems 26,351 vs 22,435, deaths 598 vs 703, hurt recalls while farming −20%. BEST SO FAR; needs confirming vs v55. (0.03: 16–18, no effect.)
- v63-fuel = v63 + fuel optimisations only (proven exact with the unlimited-fuel log comparison on ambush, contest, rows, expanse): new `seen.py` caches vision lists per turn (cleared after any own action), static map boards built once, `observe()` uses one mask over all route spawn tiles, `pick_target` reads distances from each route group's own distance field (built from leftover fuel at turn end) instead of a full-map search, followers skip the search when they see no loot, cheaper emblem counting and ward-spot checks. vs v63: 23–11, gems 28,444 vs 20,443, deaths 566 vs 766. Out-of-fuel turns 3–15% → 0.1–0.6% (lanes 10% → 2%, the rest in fights); average fuel 1.7M → 1.2M per turn.
- v63-fuelB = v63-fuel + exact setup fixes (ward-spot search ring by ring instead of a Python sort key, enemy-spawn distance field built on first use). Setup was ~7M, so champions only acted from round 3; now from round 2. Logs identical to v63 with unlimited fuel. BEST VERSION (fuel-wise) — candidate for v64.
- v63-walk = v63-fuelB + cached terrain field for walks to fixed goals more than 10 steps away (steps round visible units greedily). vs v63-fuelB: 18–16 (noise), gems 23,546 vs 22,669, deaths 585 vs 654, saves only ~0.1M per turn. Not adopted.
- Rejected: v55/v58-finish (below 35% HP keep hitting a started camp if it dies first): 17–17 on v55 (−1,385 gems), 13–21 on v58 (more deaths). v55-farm bundle (7–27). "Check before starting a camp" (too cautious, 11–23).

## Findings so far

- Hurt recalls while farming: ~9 per game per team (~5–7% of farm time); about as many again during fights.
- Contest: whichever team plays side B dies ~120 times (both versions) — a shared-code problem worth investigating. Monolith (200 gems) decides contest on gems.
- Camp value per HP (from the rules): gem cluster 48 gems/1000 HP and no damage; wolf 16.3; elk 15.0 but ~3.3x the damage per gem of a wolf; monolith 10 and ~16x.
- Server: all our losses are vs melee-heavy drafts (Varg + Vendetta + Cassi&Issac, often Ryth). In losses we collect half the gems they do and die ~2x as often. Farming speed + not dying matters most; banking second.
- Our non-Vesper champions often end with 0 emblems (Vesper collects all loot while building).
- Top teams: bank small bags right after each capture (99% banked vs our ~67%), rarely die, stack Power emblems, never chase, rotate 4–5 camp groups, can split for solo camps.

## Next ideas

- Confirm v58-dmg5 vs v55; try `DAMAGE_COST` 0.08.
- Verify why v58-monolith dies less (log v58's monolith targeting before round 1000).
- Tank swap below 50% HP (step back so another melee is nearest) — never tested alone.
- Quick banking (bags of 10+ near an altar) — never tested alone.
- Cassi stun on the monolith (camps can be stunned); investigate contest side-B deaths.
- Fuel: the decision log costs ~216k per turn (~18% of v63-fuelB's use). Ask the logging teammate about wrapping `log_turn` in `cambc.debug_block` (free in local runs, skipped on the platform) or a `LOG` on/off flag.
- Fuel: champions could act from round 1 if setup loses ~1.4M more (read each group's camps once for values/clears, centres from `spawn_lists`, the log-only spawn-gap pairs ~0.6M, `split_altars` ~0.4M).
- Ranged champions idle too long holding back from camps; fewer shop trips; never chase retreating enemies; altar locks; wards in bushes.
