"""Shared memory, carried value, and who is threatening us."""
import struct

import cambc

from actions import alive
from board import distance
from constants import (
    AGGRESSIVE_FLAG,
    CARRIER_MASK,
    CARRIER_SHIFT,
    EMBLEM_WORTH,
    EMBLEMS,
    Item,
    MODE_BYTE,
    MODE_MASK,
    NO_CAPTURE,
    SHARDS,
    SHARED_LAYOUT,
    THREAT_RANGE,
    Team,
    VESPER_DONE_FLAG,
)


def emblem_count(champion):
    try:
        counts = champion.item_count()
    except cambc.Error:
        return 0
    return sum(counts[i] for i in EMBLEMS)


def item_worth(champion):
    # Gold sunk into shards and emblems; other champions' purses can't be read.
    try:
        counts = champion.item_count()
    except cambc.Error:
        return 0
    return EMBLEM_WORTH * sum(counts[i] for i in EMBLEMS) + sum(counts[i] * Item(i).cost() for i in SHARDS)


def team_alive():
    # Every ally is listed even out of vision, and liveness is readable for all of them.
    return all(ally.is_alive() for ally in cambc.champions(Team.ALLY))


def gems_carried(champion):
    try:
        return champion.gem_count()
    except cambc.Error:
        return 0


def threat_zone(enemies, include_camps):
    zone = cambc.Bitboard()
    for enemy in enemies:
        zone.draw_chebyshev(enemy.position(), THREAT_RANGE)
    if include_camps:
        for camp in alive(cambc.vision.camps()):
            if camp.strength() > 0:
                zone.draw_chebyshev(camp.position(), camp.attack_range())
    return zone


def under_threat(me):
    # An enemy that can already hit us, or a damaged (awake) camp in its own range.
    position = me.position()
    for enemy in alive(cambc.vision.champions(Team.ENEMY)):
        if distance(position, enemy.position()) <= enemy.attack_range():
            return True
    for camp in alive(cambc.vision.camps()):
        if (camp.strength() > 0 and camp.health() < camp.max_health()
                and distance(position, camp.position()) <= camp.attack_range()):
            return True
    return False


def read_shared():
    data = bytearray(struct.calcsize(SHARED_LAYOUT))
    cambc.shared_memory.read(0, data)
    flags, slot, capture_slot, collector, stamp, claimed = struct.unpack(SHARED_LAYOUT, data)
    if stamp == 0:
        capture_slot = NO_CAPTURE
    return (flags & MODE_MASK, slot, capture_slot, collector, stamp, claimed,
            bool(flags & VESPER_DONE_FLAG), bool(flags & AGGRESSIVE_FLAG))


def write_shared(mode, slot, capture_slot, collector, claimed, vesper_done, aggressive, carrier):
    flags = (mode | carrier << CARRIER_SHIFT | (VESPER_DONE_FLAG if vesper_done else 0)
             | (AGGRESSIVE_FLAG if aggressive else 0))
    cambc.shared_memory.write(0, struct.pack(SHARED_LAYOUT, flags, slot, capture_slot, collector,
                                             cambc.round_number() + 1, claimed & 0xFF))


def mark_vesper_done():
    cambc.shared_memory[MODE_BYTE] = cambc.shared_memory[MODE_BYTE] | VESPER_DONE_FLAG


def read_carrier():
    return (cambc.shared_memory[MODE_BYTE] & CARRIER_MASK) >> CARRIER_SHIFT


def write_carrier(carrier):
    cambc.shared_memory[MODE_BYTE] = (cambc.shared_memory[MODE_BYTE] & ~CARRIER_MASK & 0xFF
                                      | carrier << CARRIER_SHIFT)
