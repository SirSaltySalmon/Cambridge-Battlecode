# Cambridge Battlecode (cambc2026a) — team notes for Claude Code

## Project

- Python bot for Cambridge Battlecode, event cambc2026a. Team: Tuan Minh (me) + Nic + a teammate who does logging.
- Repo: `D:\Cambridge-Battlecode-1`, one folder per bot under `bots\` (each has `src\bot.py, draft.py, main.py, randomness.py`).
- SDK: `cambc` 2.1.6. Rules: `docs\GameRules.md`, API: `docs\BotAPIPython.md` (read these before answering rule questions).
- I'm still learning Git/GitHub/VS Code: when a change should go to GitHub, include the steps (branch, commit, push, PR).
- I use PowerShell in the VS Code Terminal (not the Python Console).

## Team rules

- Identity: Vesper Vale-centred team (gold generation) + situational decision making. Don't propose dropping Vesper.
- Builds / shop / emblems are Nic's job. Logging is my teammate's job — don't touch logging code.
- Keep changes small and cheap on fuel (Python: 2.5M fuel per turn = 25x the C++ 100k; running out suspends the bot mid-turn and it resumes next round). No per-turn full-map searches; cache per round.
- One change at a time where possible, so tests show what helped.

## Testing

- Games are deterministic: same bots + map + side = same result. Side matters a lot, so always play every map from BOTH sides. ±2 wins out of 34 is noise.
- Before a test, check that the two bots differ ONLY in the change being tested (including `draft.py`): `git diff --no-index bots\OLD\src bots\NEW\src`
- Local test loop (run from repo root, all 17 maps, both sides). Note: Windows PowerShell's `Tee-Object` writes results.txt as UTF-16.

```powershell
Remove-Item results.txt -ErrorAction SilentlyContinue
$maps = Get-ChildItem maps\*.map26a | Where-Object { $_.BaseName -notmatch '-[0-9a-f]{8}-' }
foreach ($m in $maps) {
  $n = $m.BaseName
  "== $n ==" | Tee-Object -FilePath results.txt -Append
  cambc run NEW OLD --map $n 2>$null | Select-String "wins|  A  |  B  " | Tee-Object -FilePath results.txt -Append
  cambc run OLD NEW --map $n 2>$null | Select-String "wins|  A  |  B  " | Tee-Object -FilePath results.txt -Append
}
"ALL DONE" | Tee-Object -FilePath results.txt -Append
```

- Result row columns (inferred from win lines, not documented): side, bot, gems tributed, kills, deaths, gold earned, (unknown, always 0), draft.
- Maps: ambush, collision, contest, corners, eight, elkring, expanse, fragment, interlock, lanes, monolith, packs, pilgrimage, rows, rushbait-v2, stampede, swirl (duplicates with a long code in the name are identical copies).
- 5-pick maps: ambush, contest, corners, eight, elkring, lanes, monolith, pilgrimage, stampede. 4-pick: interlock, rows, rushbait-v2, swirl. 3-pick: collision, expanse, fragment, packs.
- Server match JSON exports (match-*.json) can be analysed. Metric ids (inferred, cross-checked): 0 gold earned, 1 spent, 2 gold held, 3 gems held, 4 gems collected, 5 gems submitted, 7 kills, 8 deaths, 9 assists, 10 monster dmg done, 11 monster dmg taken, 12 champion dmg done, 13 champion dmg taken, 14 healing, 16 shielding. Event kinds: 1 death, 2 kill, 3 assist, 5 shop buy, 9 tribute (value = gems), 10 capture (value = gems), 12 emblem. Raw .repl26a replays are binary and not readable.

## Bot versions

- v35 = Nic's bot. v35-steady = v35 + fight/retreat hysteresis (FIGHT_HYSTERESIS 10, MIN_HOLD 3).
- v49 = v35-steady + Varg attack reset + teammate's logging + Ryth in the 5-pick lineup. CURRENT BASE.
  - OPEN QUESTION: in the v55-farm vs v49 test, v49 drafted Vendetta (not Ryth) on every 5-pick map and v55-farm drafted Ryth. Check which draft.py is in which folder.
- v55-farm = v49 + farming changes. RESULT vs v49: 7–27 (v55-farm 6–11 as side A, 1–16 as side B). REJECTED as a bundle.
  - tank first (others wait outside a sleeping camp for the healthiest melee, TANK_WAIT 5)
  - tank swap below 50% when a healthier melee is near (SWAP_HEALTH 0.5)
  - Cassi/Ren stun awake camps
  - finish before recall: a champion under 35% keeps going if the camp dies first and no enemy is in sight
  - quick banking: bags of 10+ gems banked at once if an altar is within 10 tiles (Vesper included)
  - recall while standing on a capture (verified: reward still lands) when hurt or with gold and no altar near
  - big groups (strength >= 60) only started with 3 living allies near
  - safe wrapper for cambc.vision.champions (engine "No visible entity" errors)
  - Totals over 34 games: gems 17,887 vs 30,054, gold 126k vs 183k, deaths 754 vs 538. On 3/4-pick maps (identical drafts) still 4–12 with deaths 289 vs 146, so the farming logic itself hurt, not only the draft.
  - Next: re-test the changes ONE AT A TIME on top of v49.
- Tried and dropped: "check before starting a camp" (clear prediction) — far too cautious, skipped 2/3 of camps (11–23). Monolith rule (all champions >= N emblems) — on hold; monolith is a late-game (round 1000+) contest-map goal, one trip, one banked monolith (200 gems) wins contest on the gem tiebreak.

## Findings so far

- Server: all our losses are vs melee-heavy drafts (Varg + Vendetta + Cassi&Issac, often Ryth). In losses we collect half the gems they do (4,414 vs 8,960 over 8 losses) and die ~2x as often; even perfect banking would flip 0 of 8. So farming speed + not dying matters most; banking second.
- Our non-Vesper champions often end with 0 emblems (building Vesper collects all loot). Considered sharing loot only vs brawler drafts — not decided (goes against our identity).
- Top teams: bank small bags right after each capture (99% of collected gems banked vs our ~67%), rarely die, stack Power emblems, never chase, rotate 4–5 camp groups, can split for solo camps.
- Local: on contest neither v49 nor v55-farm tributes any gems (0–0 in both games); the result is decided by the gold tiebreak and follows side A.

## Next ideas (not started)

- Ranged champions idle too long holding back from camps; farm a nearby small camp while the target respawns; fewer shop trips; never chase retreating enemies; altar locks; wards in bushes.
