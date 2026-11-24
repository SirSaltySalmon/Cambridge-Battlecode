# v218-contract: the engagement contract without the ping flood (fight_coord_plan.md F2, second pass)

Base: bots/zz-ct-noans (v216-contract with the contract no longer answered as a ping). The full diff against v213 is in
`contract.patch`. New flags in `src/constants.h`, after the v216 CONTRACT block:

- `CONTRACT_PING_ANSWER` false
- `CONTRACT_HOLD` 3
- `CONTRACT_HOLD_OUTCLASS` false
- `CONTRACT_SKIP_RESCORE` true
- the HELP bit layout (`help_bits`, `HELP_LOW_FLAG`, `HELP_HIGH_FLAG`)

`CONTRACT_MODE` 0 still plays as v213. Every new path is gated on `help_bits` / `contract_on()`, which are false then.

## Why

24 debug games against v210 (12 maps × 2 sides):

| build | record | champion deaths | ping-answer turns | ping.retreat turns |
|---|---|---|---|---|
| v213 | 12-12 | 218 | 347 | 170 |
| v216-contract | 1-23 | 337 | 4,020 | 0 |
| zz-ct-noans | 13-11 | 204 | 0 | 0 |

In v216 every ENGAGE set `ping_slot`, so every healthy farmer walked to every contact. That is the PING_GATHER /
FIGHT_COMMIT_PING failure again. It also ended PING_RETREAT, because nobody pinged any more.

zz-ct-noans fixed the flood, but two problems were left:
- It still flipped engage<->disengage about 37 times a game, often 1-2 rounds after the last flip.
- It used more fuel than v213, not less: platform-style bench 37.8k vs 37.0k a turn, and 0.41-0.45% out-of-fuel turns
  vs 0.32%.

## What it does

1. **The contract is not a ping** (`CONTRACT_PING_ANSWER` false).
2. **v213's hurt-and-alone ping is back (HELP).** It uses v213's rule in `update_ping`. It sets `ping_mine_round`, which
   PING_RETREAT needs, and allies answer it as in v213: `answer_ping`, the PING_SCORE +5 bonus and PING_COLLECTOR.
   - **Where it is stored.** Byte 7 stays the contract's anchor plus its age (B, the round of its last write). The
     ping is a 2-bit age K: "sent K-1 rounds before B", with 0 meaning no ping. Bit 0 is byte 3 bit 6 and bit 1 is
     byte 4 bit 7.
   - **Sending.** The pinger publishes after `contract_turn`, so its own settle still reads the old contract. It
     writes its nearest slot, B = now and K = 1.
   - **Keeping it fresh.** While the ping is fresh, settlers keep its anchor and re-code K for their new B
     (K' = age + K), so allies answer the pinger's slot, as in v213. A contact more than 12 tiles from a fresh help
     anchor plays alone, as v213 did.
   - **Expiry.** `update_ping` clears K once the ping is PING_FRESH old, and clears it with byte 7.
   - **Why those bits are free.** Byte 3 holds a collector (champion + 1, at most 14) or CAPTURE_NEWS (bit 7 plus
     bits 0-5), so bit 6 was never used. Byte 4's focus is roster index + 1, at most 6, so bit 7 was never used.
     `read_shared` and `read_focus` mask the bits off. `write_shared`, `write_focus` and the four claim writers
     (`set_collector_byte`) keep them.
3. **Anti-flap (`CONTRACT_HOLD` 3).**
   - Every champion notes, every turn and even while dead, the round it saw the contract state change.
   - A settler may flip ENGAGE<->DISENGAGE only once that state has held 3 rounds, and logs `- contract: keeps ...`
     when it holds one.
   - Settling from NONE is not held. The emblem veto (CONTRACT_OUTCLASS) is not held either (`CONTRACT_HOLD_OUTCLASS`
     false): zz-ct-noans-nooc showed that veto is worth keeping.
4. **Fuel.**
   - **Skipped rescore (`CONTRACT_SKIP_RESCORE`).** The settler takes the shared mode without `score_mode` where v213
     would not rescore: FIGHT gives ENGAGE (unless the contract is DISENGAGE) and RETREAT gives DISENGAGE (unless it is
     ENGAGE).
   - **Shared `outclass_gap`.** The settler's call and that turn's `outclass_step` share one result, cached by round
     and `seen::generation()`.
   - **Fewer reads.** Byte 7 comes from `update_ping`'s read. The nearest slot is computed only by a settler, in one
     distance pass.

## Smoke (debug, 4 games vs v210, against the inv216 replays of the same games)

Games: 01elk-arena-a, 00stampede-a, 00the-line-b, 02arena-b. The manifest is `bench/smoke-manifest.txt`; run it from
the repo root after moving it there. Tools: `bench/cmp218.py` and `bench/flips.py`.

| build | record | banked | K/D | answers | ping.retreat | pings | transitions per game | e<->d flips per game (1-2 rounds apart) | fuel mean | out-of-fuel turns |
|---|---|---|---|---|---|---|---|---|---|---|
| v213 | 1-3 | 1310 vs 3209 | 20/50 | 13 | 37 | 53 | - | - | 34.9k | 53 (0.26%) |
| zz-ct-noans | 2-2 | 2345 vs 2498 | 28/55 | 0 | 0 | 0 | 148 | 37.2 (42) | 36.5k | 140 (0.57%) |
| v218-contract | 1-3 | 1219 vs 3197 | 19/57 | 23 | 28 | 49 | 117 | 33.2 (12) | 35.1k | 90 (0.45%) |

Fuel per map, v218 vs v213 vs zz:

| map | v218 | v213 | zz-ct-noans |
|---|---|---|---|
| elk-arena | 45.3k | 45.0k | 47.4k |
| stampede | 32.3k | 32.2k | 32.8k |
| the-line | 32.4k | 32.0k | 37.1k |
| arena | 26.7k | 26.8k | 26.8k |

Most out-of-fuel turns are on elk-arena (79, against 42 for v213 and 129 for zz). These are debug runs, so the
contract's NOTE lines count, and about 63 of the 216 notes a game are "own contact" lines.

Checks:
- No botFault events and no error lines.
- No loops or freezes in the-line-b r150-220, read by hand.
- The pings, answers and ping.retreat turns are back at v213's level.

## Open

- **The flips moved; they did not go away.** Engage<->disengage flips fell only from 37 to 33 a game. Flips 1-2
  rounds apart fell from 42 to 12, but 23 of the 59 gaps under 10 rounds now sit right at the 3-round hold. Rotating settlers really do
  disagree.
- **Outclass vetoes rose** (47 against 28 for zz). Each one is answered by a score re-entry once the hold ends.
- **Next to try:** a bench of `CONTRACT_HOLD` 5 / 8, or a longer hold for re-entry only. Four games say nothing about
  strength. The-line-b was a zz win, but v213 and v218 lost it the same way.
