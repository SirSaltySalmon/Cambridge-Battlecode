"""One turn's action trace. The turn resets it; movement and casts append to it."""
import seen

_trace_acts = []
_trace_stall = None
_trace_attack = None


def _trace_reset():
    global _trace_acts, _trace_stall, _trace_attack
    _trace_acts = []
    _trace_stall = None
    _trace_attack = None
    seen.reset()


def _act(kind):
    _trace_acts.append(kind)
    seen.reset()


def _moved(kind):
    global _trace_stall
    _trace_stall = None
    _trace_acts.append(kind)
    seen.reset()


def _move_failed(reason):
    global _trace_stall
    _trace_stall = reason
    # A rejected step may have revealed a hidden unit in a bush.
    if reason.startswith("engine rejected"):
        seen.reset()


def _attack_failed(reason):
    global _trace_attack
    _trace_attack = reason
