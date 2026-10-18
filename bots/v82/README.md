# v82

`v81` + fixes for v81's gem trade-off (switches in `constants.h`). Not submitted. Final setting:
`PROTECT_SPLIT = false`, `DODGE_TO_ALLY = true`, `RETREAT_RELEASE = true`.

- `PROTECT_SPLIT` (off): carrier protection from `PROTECT_GEMS` = 30 again, built Ren exempt. Alone vs v81: 27-41,
  gems -9.5%, gems lost +16%; champions left fights to bank and lost them. The 60-gem `CARRY_GEMS` stays.
- `DODGE_TO_ALLY`: under recall health and in reach, prefer tiles out of reach within 3 tiles of the nearest
  visible ally. Alone vs v81: 34-34, gems +7.8%, gems lost -14%.
- `RETREAT_RELEASE`: a collector retreating toward help gives its claim back. Alone vs v81: 35-33, gems +1.9%.

## Results (68 games = 34 normal + 34 unlimited fuel)

| Opponent | v82 | gems | v81 on the same test |
| --- | --- | --- | --- |
| v81 | 35-33 | +6.9% (gems lost -11%) | - |
| v80 | 34-34 | -4.9% | 42-26 (+12.0%) |
| v76 | 38-30 | -0.8% | 42-26 (+4.1%) |
| brawler | 28-40 | -10.5% | 33-35 (-6.2%) |

v82 edges v81 head to head and loses fewer gems there, but does worse than v81 against every other opponent.
Fuel (unlimited, all maps): mean 46.4k, p99 95.4k, 0.64% of turns over 100k (v81: 46.0k / 93.5k / 0.53%); per-branch
costs are unchanged, v82 just plays more fight and retreat turns.
Note: running two evaluations in parallel on the same bot folder breaks the build (cmake); run them one at a time.
