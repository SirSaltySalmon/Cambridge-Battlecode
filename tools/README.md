# tools

Test and analysis scripts (Python 3; on this machine `C:\Users\Admin\AppData\Local\Programs\Python\Python314\python.exe`).

| Script | Use |
| --- | --- |
| `bench.py run BOT OPP [--unlimited] [--release] [--maps a,b] [--jobs N] [--out DIR]` | All 17 maps from both sides (34 games, ~2 min with 11 jobs), then the summary. `--unlimited` = 100M fuel and log limit, `--release` = platform build. |
| `bench.py summary BOT DIR` | Record, gems, kills, deaths, fuel mean / p99 / turns at 100k+ for BOT's side. |
| `queue.sh QUEUEFILE` | Runs `BOT OPP [flags]` lines one after another into `results/results-BOT-vs-OPP-<kind>.txt`, skipping finished ones. Don't edit it while it runs. |
| `table.py BASE BOT...`, `readme_table.py BOT...` | Results tables from those files (v82, v82 unlimited, brawler, v80-5, locker). |
| `farm.py rows DIR...`, `farm.py table KIND BOT...` | Farming efficiency from bench folders, us vs them in the same games: gems collected and camp captures per 100 rounds, gems per capture, kill share, gems lost, banked %. `table` shows each BOT vs v82 and vs v83. |
| `farm_rank.py` | One line per v83 variant: records vs v82/v83 (normal, unlimited) and the farm.py margins per set. |
| `logparse.py REPLAY a\|b [CHAMP] [FROM-TO]` | Decision log as one record per champion turn (debug builds). |
| `flips.py DIR` | Shared mode changes and bounces (changes undone within 3 rounds). |
| `fuelturns.py DIR [LIMIT]` | Fuel per turn matched to the log: which branches go over 100k. |
| `profile_copy.py SRC DEST`, `profile_read.py DIR [MIN]` | Throwaway copy with per-function fuel scopes; run it with `--unlimited`, then read the profile. Delete the copy after. |
| `ladder.py DIR` | Summary of downloaded ladder matches (`<map>-<side>-<id>.json` from `cambc matches summary ID --json`). |
| `ladder_queue.py OPPONENTS GROUPS` | Queues one unranked group (5 maps, rotating through the pool) per "team_id name" line and appends "group_id name" lines to GROUPS for `ladder_fetch.py`. |
| `ladder_fetch.py GROUPS OUT` | Downloads the finished matches of unranked match groups (`cambc match-groups create TEAM --maps ... --json`). |

`cambc run-batch` will not overwrite an existing replay (the row fails silently), so `bench.py` empties its output folder
first; manifest paths are made absolute (relative ones resolve against the manifest's folder).
