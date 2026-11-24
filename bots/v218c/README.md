# v218c: v213 + the engagement contract with only the parts that held up

Base: bots/v218-contract (see its README) with CONTRACT_HOLD 0 and CONTRACT_SKIP_RESCORE false, so it plays as the
`zz-ct-noans` probe plus v213's hurt-alone help ping restored.

Kept, and why:
- **Contract, ENGAGE not a ping** (CONTRACT_MODE 2, CONTRACT_PING_ANSWER false). zz-ct-noans went 13-11 vs v213's 12-12
  on the contract's 24 worst games, with deaths 204 vs 218. The only fight-plan part with a non-negative signal.
- **CONTRACT_OUTCLASS** on: with it off, 11-13 and 230 deaths.
- **Help ping** (byte 3 bit 6 + byte 4 bit 7, see v218-contract): restores v213's ping and PING_RETREAT, which the
  contract had switched off.

Dropped (failed, or never shown to help): g4 veto (FIGHT_SIM), FOCUS_KEEP / FOCUS_VALUE (neutral over 2 pool seeds),
PEEL, Vesper micro (v218 bench: Vesper deaths 126 vs 112), CONTRACT_HOLD, CONTRACT_SKIP_RESCORE, leader adoption,
outclass group rule, and the hurt-no-part rule. v218 with everything lost 34-52, -20.5%.

## Bench (debug with logs, 86 maps once, v218c's side alternating by map, vs v213)

| | v218c | v213 |
|---|---|---|
| Record | 42-44 | |
| Banked | 57,179 | 63,037 (-9.3%) |
| Champion kills / deaths | 857 / 858 | |
| Camp deaths | 71 | 73 |
| Fuel per turn | 38,217 | 37,143 (+2.9%) |
| Out-of-fuel turns | 0.42% | 0.39% |
| Bot faults | 0 | 0 |

Per game: contract none->engage 46, engage<->disengage 28, help pings 12.7; ping.retreat 646 turns, answers 1,462.

## Verdict

Level with v213 within one run's noise (about +-5 wins and +-10% banked on 86 games), and slightly behind on banked
and fuel. Not a ladder upgrade. Keep v213 live. v218c is the clean base if the contract is worked on again.
Engage<->disengage flips (28 a game) are its open problem.
