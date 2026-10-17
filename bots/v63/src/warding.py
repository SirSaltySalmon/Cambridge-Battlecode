"""Ward spots, placement, and the walk over to an uncovered spot."""
import cambc
from cambc import game_map

import action_trace as trace
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

    def pick(candidates, count):
        for tile in sorted(candidates.positions(), key=lambda t: (distance(t, middle), t.x, t.y)):
            if count == 0:
                return
            if all(distance(tile, spot) >= ANTI_WARD_RADIUS for spot in spots):
                spots.append(tile)
                count -= 1

    pick(tiles_around(middle, ANTI_WARD_RADIUS) & free, CENTRAL_WARDS)
    near, inner = cambc.Bitboard(), cambc.Bitboard()
    for tile in game_map.spawn_region(Team.ENEMY).positions():
        near.draw_chebyshev(tile, ENEMY_WARD_GAP)
        inner.draw_chebyshev(tile, ENEMY_WARD_GAP - 1)
    # The ring side facing the middle comes first, since that is where the enemy walks out.
    pick(near & ~inner & free, ENEMY_SPAWN_WARDS)
    return spots[:8]


class Warding:
    # Wards

    def open_ward_spots(self, me):
        # Uncovered spot indexes; any spot a visible allied ward already covers is marked for everyone.
        mask = cambc.shared_memory[WARD_MASK_BYTE]
        allied = None
        open_spots = []
        for index, spot in enumerate(self.ward_spots):
            if mask >> index & 1:
                continue
            if allied is None:
                allied = cambc.vision.ward_tiles(Team.ALLY)
                for own in me.ward_positions():
                    allied.set(own, False)
            if not (allied & tiles_around(spot, ANTI_WARD_RADIUS - 1)).is_empty():
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
            tiles &= ~(game_map.walls() | game_map.holes() | cambc.vision.ward_tiles())
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
