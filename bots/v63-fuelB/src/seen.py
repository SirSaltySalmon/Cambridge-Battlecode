"""Fuel cache for engine reads. Each vision list costs 10-60k fuel, so it is read once per turn
and read again only after one of our own actions (a move, attack, cast or ward changes what we see)."""
import cambc
from cambc import game_map

from constants import Team

_turn = {}
_rosters = {}
_static = {}


def reset():
    # Called at turn start and after every action.
    _turn.clear()


def roster(team):
    # Every champion of a team, in or out of vision; the team never changes during a match.
    units = _rosters.get(team)
    if units is None:
        units = _rosters[team] = cambc.champions(team)
    return units


def _living(units):
    living = []
    for unit in units:
        try:
            if unit.health() > 0:
                living.append(unit)
        except cambc.Error:
            pass
    return living


def champions(team):
    # Living visible champions of a team; a fresh list, so callers may change it.
    units = _turn.get(team)
    if units is None:
        units = _turn[team] = _living(cambc.vision.champions(team))
    return list(units)


def camps():
    # Living visible camps.
    units = _turn.get("camps")
    if units is None:
        units = _turn["camps"] = _living(cambc.vision.camps())
    return list(units)


def terrain():
    # Walls and holes; never mutate the returned board.
    board = _static.get("terrain")
    if board is None:
        board = _static["terrain"] = game_map.walls() | game_map.holes()
    return board


def fixed_blocks():
    # Everything that blocks a step and never moves: terrain, altars and the enemy spawn.
    board = _static.get("fixed")
    if board is None:
        board = _static["fixed"] = terrain() | game_map.altars() | game_map.spawn_region(Team.ENEMY)
    return board


def ally_spawn():
    board = _static.get("ally_spawn")
    if board is None:
        board = _static["ally_spawn"] = game_map.spawn_region(Team.ALLY)
    return board


def altar_list():
    # Altar tiles; never mutate the returned list.
    tiles = _static.get("altars")
    if tiles is None:
        tiles = _static["altars"] = game_map.altars().positions()
    return tiles
