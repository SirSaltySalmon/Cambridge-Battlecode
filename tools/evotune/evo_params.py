"""Read and write a bot's evo_constants file.

Every `inline constexpr int|long|double|float NAME = <number literal>;` declaration is a tuned parameter. Bools and
values that are expressions are listed as fixed. An optional comment tunes one parameter's search:

    // evo: delta=2 min=0 max=20        (on the line above, or trailing on the same line)
    // evo: off                         (never vary it)

Defaults without a comment: delta = 20% of the value (ints at least 1; a 0 gets 1, or 0.1 for a double) and the
range keeps the sign: [value / 4, value * 4] (ints rounded outward, at least 4 deltas wide; a 0 gets +-4 deltas).
"""
import math
import os
import re

NAMES = ("evo_constants.h", "evo_constants.hpp", "evo_constants.hh")
DECL = re.compile(r"^[ \t]*inline[ \t]+constexpr[ \t]+([\w:]+)[ \t]+([^;]+);(.*)$", re.M)
ITEM = re.compile(r"^\s*([A-Za-z_]\w*)\s*=\s*(.+?)\s*$")
NUMBER = re.compile(r"^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?[fF]?$")
NOTE = re.compile(r"//\s*evo:\s*(.*)$")
INT_TYPES = {"int", "long", "short", "unsigned", "std::int32_t", "std::int64_t", "std::uint32_t", "std::uint64_t"}
FLOAT_TYPES = {"double", "float"}


class EvoError(Exception):
    pass


def find(bot_dir):
    """The evo_constants file of a bot folder (src/ first, then anywhere outside build folders)."""
    if not os.path.isdir(bot_dir):
        raise EvoError("not a folder: %s" % bot_dir)
    for name in NAMES:
        p = os.path.join(bot_dir, "src", name)
        if os.path.isfile(p):
            return p
    for root, dirs, files in os.walk(bot_dir):
        dirs[:] = [d for d in dirs if d not in (".cambc", "build", "target", ".git")]
        for name in NAMES:
            if name in files:
                return os.path.join(root, name)
    raise EvoError("no evo_constants.h in %s (looked in src/ and below)" % os.path.relpath(bot_dir).replace("\\", "/"))


def _note(text):
    m = NOTE.search(text)
    if not m:
        return None
    out = {}
    for tok in m.group(1).split():
        if tok == "off":
            out["off"] = True
        elif "=" in tok:
            k, v = tok.split("=", 1)
            if k not in ("delta", "min", "max"):
                raise EvoError("unknown evo key %r in: %s" % (k, text.strip()))
            out[k] = float(v)
    return out


def _defaults(value, is_int):
    if value == 0:
        delta = 1.0 if is_int else 0.1
        return delta, -4 * delta, 4 * delta
    delta = abs(value) * 0.2
    if is_int:
        delta = max(1.0, float(round(delta)))
    a, b = sorted((value / 4.0, value * 4.0))
    if is_int:
        a, b = math.floor(a), math.ceil(b)
    # A small int (2) would get a range of 0..8 = 4 deltas; keep at least 4 deltas each way where the sign allows.
    if value > 0:
        b = max(b, value + 4 * delta)
    else:
        a = min(a, value - 4 * delta)
    return delta, float(a), float(b)


def parse(path):
    """(params, fixed): params are dicts with name, type, int, base, delta, min, max, span (literal offsets)."""
    text = open(path, encoding="utf-8").read()
    params, fixed = [], []
    for m in DECL.finditer(text):
        ctype, body, trail = m.group(1), m.group(2), m.group(3)
        prev = text[:m.start()].rstrip("\r\n").rsplit("\n", 1)[-1]
        note = _note(trail) or (_note(prev) if prev.lstrip().startswith("//") else None) or {}
        # Split declarators at top-level commas; record each literal's absolute offset.
        offset = m.start(2)
        depth, start, parts = 0, 0, []
        for i, ch in enumerate(body):
            if ch in "([{<":
                depth += 1
            elif ch in ")]}>":
                depth -= 1
            elif ch == "," and depth == 0:
                parts.append((start, body[start:i]))
                start = i + 1
        parts.append((start, body[start:]))
        for pos, part in parts:
            im = ITEM.match(part)
            if not im:
                fixed.append({"name": part.strip(), "type": ctype, "why": "not NAME = value"})
                continue
            name, lit = im.group(1), im.group(2)
            if ctype not in INT_TYPES | FLOAT_TYPES:
                fixed.append({"name": name, "type": ctype, "value": lit, "why": "%s is not tuned" % ctype})
                continue
            if not NUMBER.match(lit):
                fixed.append({"name": name, "type": ctype, "value": lit, "why": "not a plain number"})
                continue
            if note.get("off"):
                fixed.append({"name": name, "type": ctype, "value": lit, "why": "evo: off"})
                continue
            is_int = ctype in INT_TYPES
            if is_int and re.search(r"[.eEfF]", lit):
                fixed.append({"name": name, "type": ctype, "value": lit, "why": "int with a decimal literal"})
                continue
            value = float(lit.rstrip("fF"))
            delta, lo, hi = _defaults(value, is_int)
            delta = note.get("delta", delta)
            lo, hi = note.get("min", lo), note.get("max", hi)
            if delta <= 0 or lo > value or hi < value:
                raise EvoError("%s: delta must be > 0 and min <= %s <= max" % (name, lit))
            lit_at = offset + pos + part.index(lit, part.index("=") + 1)
            params.append({"name": name, "type": ctype, "int": is_int, "base": value, "delta": delta,
                           "min": lo, "max": hi, "span": [lit_at, lit_at + len(lit)],
                           "float_suffix": lit[-1] in "fF"})
    names = [p["name"] for p in params]
    if len(set(names)) != len(names):
        raise EvoError("a parameter is declared twice")
    if not params:
        raise EvoError("%s has no tunable int/double constants" % path)
    return params, fixed


def literal(param, value):
    if param["int"]:
        return str(int(value))
    s = "%.6g" % value
    if not re.search(r"[.eE]", s):
        s += ".0"
    return s + ("f" if param["float_suffix"] else "")


def render(path, params, values):
    """The file's text with each parameter's literal replaced by values[name]; everything else kept."""
    text = open(path, encoding="utf-8").read()
    for p in sorted(params, key=lambda p: p["span"][0], reverse=True):
        a, b = p["span"]
        text = text[:a] + literal(p, values[p["name"]]) + text[b:]
    return text
