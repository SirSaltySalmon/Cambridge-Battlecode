# v182

Platform v182: the common parent of bots/v185 (Mike's, ae3604b) and bots/v206 / bots/v208 (Jack's), and the start of
the stepwise ladder test bots/v185-local .. bots/v201. Its lineage after v155 (v156 .. v181) has no folders in this
repo; each step's switches and bench numbers sit in `src/constants.h`, block by block:

- v156-v157: monolith deadlock fixes and economy (SHOP_NO_TRIBUTE, GATE_ALL_GROUPS, MONOLITH_GATHER, BANK_ON_WAY,
  MONO_MEMORY; switches bots/v157-monolith also carries).
- v158-v161: the Prism team on gem-cluster maps (PRISM_TEAM_BAR, PRISM_MOVE_WITH, PRISM_PICKERS), monolith sessions
  (MONO_SESSIONS, MONO_EARLY, MONO_LEAVE, MONO_SIDE).
- v165-v171: Vesper gold-only, Prism on our side (PRISM_OUR_SIDE), PRISM_WILLOW_4, PRISM_AS_V170.
- v177: ECON_PHASES / CLOSE_OUT (from the 00collision VOD vs QS2).
- v180: PRISM_GUARD, PRISM_STRICT (tribute only a winning bag).
- v181: KILL_TIME, a fight score that compares both sides.

Why v170 beat it (KILL_TIME's clamp, PRISM_STRICT, the v156-v157 economy) and what v185 undid: bots/v185/README.md.
