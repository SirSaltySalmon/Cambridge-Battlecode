"""Ward spots, placement, and the walk over to an uncovered spot."""
import cambc
from cambc import game_map

import action_trace as trace
import seen
from action_trace import _act
from actions import move_towards_closest
from board import at, distance, tiles_around
from constants import (
    AGGRESSIVE_WARD_DETOUR_RANGE,
    ANTI_WARD_RADIUS,
    CENTRAL_WARDS,
    ENEMY_SPAWN_WARDS,
    ENEMY_WARD_GAP,
    IDLE_BANK,
    Team,
    WARD_DETOUR_RANGE,
    WARD_MASK_BYTE,
    WARD_SLACK,
)
from state import under_threat


def ward_spots():
    # Deterministic, so every champion agrees on the bit for each spot.
    free = ~(game_map.walls() | game_map.holes() | game_map.altars()
             | game_map.spawn_region(Team.ALLY) | game_map.spawn_region(Team.ENEMY))
    middle = cambc.Vec2(game_map.width() // 2, game_map.height() // 2)
    spots = []

    # Tiles within the anti-ward radius of a chosen spot; one lookup replaces a distance to every spot.
    taken = cambc.Bitboard()

    def pick(order, count):
        # order holds (..., tile) tuples, nearest the middle first, then by x and y.
        for entry in order:
            if count == 0:
                break
            tile = entry[-1]
            if not taken.is_set(tile):
                spots.append(tile)
                taken.draw_chebyshev(tile, ANTI_WARD_RADIUS - 1)
                count -= 1
        return count

    # Central spots ring by ring from the middle, so only one ring's tiles are sorted at a time
    # (a Python sort key over every tile cost ~3M fuel on the first turn).
    count = CENTRAL_WARDS
    inside = cambc.Bitboard()
    for radius in range(ANTI_WARD_RADIUS + 1):
        if count == 0:
            break
        disk = tiles_around(middle, radius)
        ring = disk & ~inside & ~taken & free
        inside = disk
        if not ring.is_empty():
            count = pick(sorted([(t.x, t.y, t) for t in ring.positions()]), count)
    near, inner = cambc.Bitboard(), cambc.Bitboard()
    for tile in game_map.spawn_region(Team.ENEMY).positions():
        near.draw_chebyshev(tile, ENEMY_WARD_GAP)
        inner.draw_chebyshev(tile, ENEMY_WARD_GAP - 1)
    # The ring side facing the middle comes first, since that is where the enemy walks out.
    mx, my = middle.x, middle.y
    ring = (near & ~inner & free).positions()
    pick(sorted([(max(abs(t.x - mx), abs(t.y - my)), t.x, t.y, t) for t in ring]), ENEMY_SPAWN_WARDS)
    return spots[:8]


class Warding:
    # Wards

    def open_ward_spots(self, me):
        # Uncovered spot indexes; any spot a visible allied ward already covers is marked for everyone.
        # Each spot's zone is fixed, so the zones and their union are built once.
        if self.ward_zones is None:
            self.ward_zones = [tiles_around(spot, ANTI_WARD_RADIUS - 1) for spot in self.ward_spots]
            self.ward_union = cambc.Bitboard()
            for zone in self.ward_zones:
                self.ward_union |= zone
        mask = cambc.shared_memory[WARD_MASK_BYTE]
        allied = None
        open_spots = []
        for index in range(len(self.ward_spots)):
            if mask >> index & 1:
                continue
            if allied is None:
                allied = cambc.vision.ward_tiles(Team.ALLY)
                for own in me.ward_positions():
                    allied.set(own, False)
                allied &= self.ward_union
                covered = not allied.is_empty()
            if covered and not (allied & self.ward_zones[index]).is_empty():
                mask |= 1 << index
                cambc.shared_memory[WARD_MASK_BYTE] = mask
                continue
            open_spots.append(index)
        return open_spots

    def place_ward(self, me):
        if not self.ward_spots:
            return False
        if self.ward_slot is not None:
            if me.ward_positions():
                return False
            # Our ward is gone, so its spot is open again.
            bit = 1 << self.ward_slot
            cambc.shared_memory[WARD_MASK_BYTE] = cambc.shared_memory[WARD_MASK_BYTE] & ~bit
            self.ward_slot = None
        try:
            if me.ward_cooldown() > 0:
                return False
        except cambc.Error:
            return False

        position = me.position()
        reach = cambc.constants.WARD_PLACEMENT_RADIUS
        for index in self.open_ward_spots(me):
            spot = self.ward_spots[index]
            if distance(position, spot) > reach + WARD_SLACK:
                continue
            tiles = me.ward_placement_tiles() & tiles_around(spot, WARD_SLACK)
            tiles &= ~(seen.terrain() | cambc.vision.ward_tiles())
            for tile in sorted(tiles.positions(), key=lambda t: distance(t, spot)):
                try:
                    if not me.place_ward(tile):
                        continue
                except cambc.Error:
                    continue
                self.ward_slot = index
                cambc.shared_memory[WARD_MASK_BYTE] = cambc.shared_memory[WARD_MASK_BYTE] | 1 << index
                _act("ward")
                self.note(f"placed ward {index} at {at(tile)}")
                print(f"round {cambc.round_number()}: champion {int(me.champion_name())} wards spot {index}")
                return True
        return False

    def ward_detour(self, me, gems):
        # Only a champion whose ward is still free, carrying little and not mid-clear, walks to a spot.
        if (self.ward_slot is not None or not self.ward_spots or gems >= IDLE_BANK
                or self.group_camps() or under_threat(me)):
            return False
        position = me.position()
        limit = AGGRESSIVE_WARD_DETOUR_RANGE if self.aggressive else WARD_DETOUR_RANGE
        nearby = [self.ward_spots[i] for i in self.open_ward_spots(me)
                  if distance(position, self.ward_spots[i]) <= limit]
        if not nearby:
            return False
        spot = min(nearby, key=lambda s: distance(position, s))
        self.branch("ward.detour", f"uncovered ward {at(spot)} within {limit}, bag light")
        ok = move_towards_closest(tiles_around(spot, cambc.constants.WARD_PLACEMENT_RADIUS))
        if not ok:
            self.note(f"ward detour to {at(spot)} failed: {trace._trace_stall}")
        return ok
