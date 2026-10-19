# v77

`v76` + the support ping (`FIX_PING`, switch in `constants.h`). Not submitted.

## Design
Shared byte 7 held the ward-post mask, unused since the moving ward (`FIX_SCOUT_WARD`, no kept posts), so the
ping has a whole byte (a `static_assert` keeps it that way):
- bits 0-5: the route slot nearest the champion under attack, + 1 (0 = no ping); bits 6-7: round & 3.
- Send: lost `PING_MIN_LOSS` = 60+ health since our last turn with an enemy champion within `PING_REACH` = 4, and no
  fresh ping up (`update_ping()`, combat.cpp, before the team decision).
- Age: fresh for `PING_FRESH` = 2 rounds; any champion clears an older one.
- Team: a fresh ping within `PING_RANGE` = 12 tiles of the leader puts the team in fight mode (route.cpp).
- Answer: a fight-mode champion with no enemy in sight walks to the pinged slot (`answer_ping()`, `fight.answer`).
Unlike the v70 versions, responders know where to go: the ping carries a place, not only who.

## Results (68 games = 34 normal + 34 unlimited fuel)

| Opponent | Normal | Unlimited | Total | Gems |
| --- | --- | --- | --- | --- |
| v76 | 16-18 | 21-13 | 37-31 | +1.0% |
| v74 | 16-18 | 18-16 | 34-34 | -1.9% |
| brawler (v76 always fighting) | 12-22 | 15-19 | 27-41 | (v76 vs brawler: 31-37) |

Per game: 28 pings sent, 22 leader fight calls, 49 answer turns. Out-of-fuel turns +2.8%, 0 crashes.
Locally neutral, as in the v70 tests; the team expects the gain on the ladder, where opponents hunt lone champions.
