# v220-peel (bots/v220 + PEEL on)

One switch: `PEEL = true` (v216 F4: a ready stun from Ryth, Prism or Marcella goes on an enemy melee within 2 of our
Vesper, Prism or Willow, before the focus). Everything else is bots/v220.

## Platform A/B (unranked, 2026-11-04)

Platform v219 = bots/v220 (PEEL off), platform v220 = this folder. 15 groups each, one per team that beat us in our last
100 ranked games (42 lost team/map pairs): the maps we lost on, same sides, padded with 00the_line, 02treasure_box,
00packs, 00ambush; `--source-group` the losing ranked group, so the opponent is the bot that beat us. Same 75 games for
both.

| | Record | Gems (ours vs theirs) | K/D | Camp deaths / game |
| --- | --- | --- | --- | --- |
| PEEL off, all 75 | 34-41 | 723 vs 752 | 11.7 / 10.2 | 0.35 |
| PEEL on, all 75 | 35-40 | 719 vs 738 | 12.1 / 9.6 | 0.43 |
| PEEL off, 42 lost maps | 13-29 | 608 vs 862 | 8.8 / 10.6 | 0.57 |
| PEEL on, 42 lost maps | 14-28 | 621 vs 849 | 9.9 / 9.9 | 0.67 |
| PEEL off, 33 pad maps | 21-12 | 869 vs 611 | 15.4 / 9.5 | 0.06 |
| PEEL on, 33 pad maps | 21-12 | 844 vs 596 | 15.0 / 9.2 | 0.12 |

Platform games are deterministic: 54 of 75 were identical (PEEL never fired). In the 21 that differed: one result
flipped (Sir Chud 01starfish, loss to win, 92-1022 -> 1000-16), none the other way; deaths 336 -> 294 (-12.5%), kills
256 -> 287; gem margin -151 -> -114 a game; margin better in 11, worse in 8; camp deaths 13 -> 19. Bot faults 0 both.

Read: PEEL does what it is for (fewer deaths where it fires), and the record moves by one game. 21 changed games is a
small sample with large single-game swings (Stubborn 02treasure_box 884 -> 10, Sir Chud 01starfish 92 -> 1000), so it is
a lean, not a proven gain. The previous live bot (platform v218) lost all 42 of these team/map pairs on ranked; v220's
13-14 wins there come from the v220 changes (or a different match seed), not from PEEL.
