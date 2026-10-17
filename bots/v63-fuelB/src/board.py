"""Tiles, walking distance, and what blocks a step."""
import cambc
from cambc import game_map

import seen
from constants import Team


def distance(a, b):
    return a.chebyshev_distance(b)


def at(position):
    return f"({position.x},{position.y})"


def step_room(origin):
    # Tiles a single step can reach, and which rule blocks each one. Enemy spawn counts as a wall.
    me = cambc.me()
    area = tiles_around(origin, me.base_stats().base_move_speed)
    area.set(origin, False)
    enemy = game_map.spawn_region(Team.ENEMY)
    terrain = game_map.walls() | game_map.holes()
    champs = cambc.vision.champion_tiles()
    camps = cambc.vision.camp_tiles()
    altars = game_map.altars()
    n_enemy = (area & enemy).count()
    rest = area & ~enemy
    n_terrain = (rest & terrain).count()
    rest = rest & ~terrain
    n_champ = (rest & champs).count()
    rest = rest & ~champs
    n_camp = (rest & camps).count()
    rest = rest & ~camps
    n_altar = (rest & altars).count()
    rest = rest & ~altars
    return (f"step-room {area.count()}: free {rest.count()}, enemy-spawn {n_enemy}, "
            f"terrain {n_terrain}, champion {n_champ}, camp {n_camp}, altar {n_altar}")


def goal_cover(raw, blocked):
    enemy = game_map.spawn_region(Team.ENEMY)
    on_spawn = (raw & enemy).count()
    other = (raw & blocked & ~enemy).count()
    parts = []
    if on_spawn:
        parts.append(f"{on_spawn} goals on enemy spawn")
    if other:
        parts.append(f"{other} goals otherwise blocked")
    return ", ".join(parts) if parts else "goals filtered empty"


def tiles_around(position, radius):
    tiles = cambc.Bitboard()
    tiles.draw_chebyshev(position, radius)
    return tiles


def walk_field(roots):
    # Walking steps from the roots around terrain; camps and champions move or die, so they don't block.
    field = cambc.DistanceField()
    reached = cambc.Bitboard()
    field.compute(seen.terrain(), roots, reached)
    return field, reached


def steps_to(walk, tiles):
    field, reached = walk
    steps = [field.get(t) for t in tiles if reached.is_set(t)]
    return min(steps) if steps else None


def blocked_tiles():
    # Combine terrain and visible occupants using native bitboard operations.
    blocked = (seen.fixed_blocks() | cambc.vision.champion_tiles()
               | cambc.vision.camp_tiles() | cambc.vision.teleport_tiles())
    blocked.set(cambc.me().position(), False)
    return blocked


def _no_step_reason(origin, reached):
    parts = ["no closer step"]
    if reached is not None and not reached.is_set(origin):
        parts.append("path never reached this tile (boxed in)")
    if game_map.spawn_region(Team.ENEMY).is_set(origin):
        parts.append("standing on enemy spawn")
    if game_map.spawn_region(Team.ALLY).is_set(origin):
        parts.append("standing on ally spawn")
    return "; ".join(parts)
