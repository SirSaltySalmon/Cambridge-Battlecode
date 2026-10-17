"""One turn's action trace. The turn resets it; movement and casts append to it."""
_trace_acts = []
_trace_stall = None
_trace_attack = None


def _trace_reset():
    global _trace_acts, _trace_stall, _trace_attack
    _trace_acts = []
    _trace_stall = None
    _trace_attack = None


def _act(kind):
    _trace_acts.append(kind)


def _moved(kind):
    global _trace_stall
    _trace_stall = None
    _trace_acts.append(kind)


def _move_failed(reason):
    global _trace_stall
    _trace_stall = reason


def _attack_failed(reason):
    global _trace_attack
    _trace_attack = reason
