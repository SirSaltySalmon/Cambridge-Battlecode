# v218: the fight plan's fixes merged (contract + g4 veto + focus + Vesper micro) on v213

Base: bots/v213. Components were built separately (folders bots/v218-contract, bots/v218-sim, bots/v218-micro, each
with its README) and merged here with bots/v216-focus-v's FOCUS_KEEP + FOCUS_VALUE.

| Part | Flags in v218 | From |
|---|---|---|
| Contract, not a ping; v213's hurt-alone help ping restored (byte 3 bit 6 + byte 6 bit 7); 3-round hold; settler skips rescores v213 would skip | CONTRACT_MODE 2, CONTRACT_PING_ANSWER false, CONTRACT_HOLD 3, CONTRACT_SKIP_RESCORE, CONTRACT_OUTCLASS | v218-contract |
| g4 veto: v213's FIGHT turns RETREAT only when g4 p_lost - p_won > 0.5 (0.6 staying) and v213's margin < 30 | FIGHT_SIM 4, VETO_BAR 0.5, VETO_STAY 0.6, VETO_MARGIN 30 | v218-sim |
| Focus kept steady, picked by kill value | FOCUS_KEEP, FOCUS_VALUE (PEEL off) | v216-focus-v |
| Vesper micro on a 2-turn projection, shot-keeping order, approach guard, re-entry guard | MICRO_VESPER, MICRO_APPROACH | v218-micro |

Merge changes:
- The help ping's high bit moved from byte 4 bit 7 (FOCUS_KEEP's refresh bit) to byte 6 bit 7. The claim round keeps
  7 bits (CLAIM_ROUND_MASK, 128-round wrap; it is only aged against CAPTURE_TIMEOUT 15), and every writer goes through
  `set_claim_byte`.
- The settler's DISENGAGE -> ENGAGE re-entry also needs score_mode's own FIGHT, so the veto holds there.
- A champion under the recall bar takes no part in the contract, and the veto is blocked for a decider under the
  recall bar (veto_block bit 16). Its own exit is self-preservation and must not move the team.

## Bench (debug with logs, 86 maps once, v218's side alternating by map, vs v213)

| | v218 | v213 |
|---|---|---|
| Record | 34-52 | |
| Banked | 52,572 | 66,140 (-20.5%) |
| Champion kills / deaths | 725 / 838 | |
| Camp deaths | 69 | 52 |
| Fuel per turn | 38,552 | 37,777 (+2.1%) |
| Out-of-fuel turns | 0.48% | 0.35% |
| Bot faults | 0 | 0 |

Same games, share of turns: leaving 10.1% vs 7.6% (`retreat.*` +45-125%, `outclass.leave` +23%), tribute channels
-20%. Deaths are up for almost every champion: HoR 264 vs 218, Vendetta 55 vs 34, Prism 48 vs 24, Vesper 126 vs 112.

Activity per game: g4 veto 24.4 (3.7% of its 654 calls, against 0.9% in its smoke); contract none->engage 41.7,
none->disengage 15.2, engage<->disengage 19.6, holds 20.9; help pings 13.6; ping.retreat 651 turns.

Why it lost (40 games, the cause logged on each retreat turn): 916 retreat turns were **a contract DISENGAGE
overriding a FIGHT the leader had chosen**, against 86 from the veto directly. A DISENGAGE contract is sticky.
CONTRACT_SKIP_RESCORE stops skipping once the state is DISENGAGE, and re-entry needs the bar + CONTRACT_REENTER, the
3-round hold and (merge) the veto. So the team leaves fights the leader is still taking: lesson L3 again.

## Verdict

Not for the ladder. Cheapest next test if more compute is spent: CONTRACT_MODE 2 with DISENGAGE never overriding a
shared FIGHT (the contract only aligns entry, the leader's FIGHT stands), and FIGHT_SIM 0 alongside. One 86-map run.
