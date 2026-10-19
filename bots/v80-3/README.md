# v83

`v81` + two item changes. Ren: Jungle Health -> Durability Health -> stack Bruiser Focus (was Bruiser Health ->
Durability Health -> Power Focus). Willow: Willow builds Durability Focus -> Power Focus (Durability first, as asked) (`WILLOW_FOCUS` in
`constants.h`) instead of the shared ranged list Power -> Monster -> Durability Focus. Durability (10% less damage
taken) keeps the healer alive; Monster (more damage to camps) did little for her. Cassi keeps `RANGED_FOCUS`.
 Not submitted.

Power -> Durability (first version), 68 games vs v81: 30-38, gems -3.0% (within noise; Willow plays only on 4- and 5-champion maps).

## Merged: Ren build Jungle Health -> Durability Health -> stack Bruiser Focus
(v83 has Bruiser Health -> Durability Health -> stack Power Focus.) Clone of v83 with only `REN_BUILD` and
`FALLBACK_REN` changed, 68 games vs v83: 38-30 (normal 19-15, unlimited 19-15), team gems -0.5%.
Ren over both runs (new / v83): deaths 137 / 144, champion kills 232 / 212, camp damage 1.12M / 1.01M (+11%),
gold earned 78.7k / 72.9k (+8%), champion damage about equal, gems banked 14.5k / 14.4k.
Vs the brawler (68 games each): new Ren build 32-36 (gems -1.2%), v83 33-35 (gems -2.0%). Ren vs brawler (new / v83):
deaths 147 / 154, kills 226 / 254, champion damage 471k / 540k (-13%), camp damage 1.09M / 1.07M, gold 74.9k / 74.4k.
