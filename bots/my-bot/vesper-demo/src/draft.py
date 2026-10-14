import struct

import cambc
from cambc import game_map

Draft, Camp = cambc.ChampionDraft, cambc.CampType

# Picks in order; the game may ask for fewer than five.
ROSTER = (Draft.VESPER_VALE, Draft.VARG, Draft.REN, Draft.VENDETTA, Draft.CASSI_AND_ISSAC)
# Groups made only of these kinds can join the pocket.
POCKET_CAMP_KINDS = (Camp.WOLF, Camp.GEM_CLUSTER, Camp.GOLD_CLUSTER, Camp.ELK, Camp.MONOLITH)
DENSITY_RADIUS = 8
# Groups this close to the pocket anchor are farmed while the pocket respawns.
ROAM_RADIUS = 60

# Group value, in gems.
GOLD_WEIGHT = 0.2
# Gems lost per point of health the team takes while clearing.
DAMAGE_COST = 0.01
# Rough team basic-attack damage per round, used to estimate clear time.
TEAM_DAMAGE = 200
# Walking distance to the nearest altar at which a pocket is worth half.
ALTAR_FALLOFF = 20
UNREACHABLE_GAP = 255

# Health and strength per kind, from the rules; camps outside vision hide them.
CAMP_STATS = {
    Camp.WOLF: (1350, 20),
    Camp.ELK: (2400, 60),
    Camp.GEM_CLUSTER: (500, 0),
    Camp.GOLD_CLUSTER: (500, 0),
    Camp.MONOLITH: (20000, 200),
}

_memory = None


def group_center(group):
    tiles = group.spawn_tiles().positions()
    return cambc.Vec2(sum(t.x for t in tiles) // len(tiles), sum(t.y for t in tiles) // len(tiles))


def clear_rounds(group):
    return sum(CAMP_STATS.get(camp.kind(), (0, 0))[0] for camp in group.camps()) / TEAM_DAMAGE


def group_value(group):
    # Reward minus the health the team trades for it; never negative.
    damage_taken = sum(CAMP_STATS.get(camp.kind(), (0, 0))[1] for camp in group.camps()) * clear_rounds(group)
    reward = group.gem_value() + GOLD_WEIGHT * group.gold_value()
    return max(0.0, reward - DAMAGE_COST * damage_taken)


def group_income(group):
    # Value per round when the group is farmed every time it returns.
    cycle = group.max_respawn_timer() + clear_rounds(group) + cambc.constants.CAPTURE_TIME
    return group_value(group) / max(1, cycle)


def altar_gaps(groups, indexes):
    # Walking distance from each group to its nearest altar.
    field = cambc.DistanceField()
    reached = cambc.Bitboard()
    field.compute(game_map.walls() | game_map.holes(), game_map.altars(), reached)
    gaps = {}
    for index in indexes:
        steps = [field.get(t) for t in groups[index].spawn_tiles().positions() if reached.is_set(t)]
        gaps[index] = min(steps) if steps else UNREACHABLE_GAP
    return gaps


def compute_pocket():
    # Returns the anchor tile and route group indexes, nearest to the anchor first.
    groups = game_map.camp_groups()
    centers = {}
    for index, group in enumerate(groups):
        if group.camps() and all(camp.kind() in POCKET_CAMP_KINDS for camp in group.camps()):
            centers[index] = group_center(group)
    if not centers:
        return None, []

    incomes = {index: group_income(groups[index]) for index in centers}
    gaps = altar_gaps(groups, centers)

    # Find neighbours with native bitboard ops; Python pair loops are too costly in fuel.
    board = cambc.Bitboard()
    at_tile = {}
    for index, center in centers.items():
        board.set(center)
        at_tile.setdefault(center, []).append(index)

    best = None
    for index, center in centers.items():
        members = [m for tile in (board & disk(center, DENSITY_RADIUS)).positions() for m in at_tile[tile]]
        gap = sum(gaps[m] for m in members) / len(members)
        score = sum(incomes[m] for m in members) * ALTAR_FALLOFF / (ALTAR_FALLOFF + gap)
        if best is None or score > best[0]:
            best = (score, index)

    anchor = centers[best[1]]
    route = [m for tile in (board & disk(anchor, ROAM_RADIUS)).positions() for m in at_tile[tile]]
    route.sort(key=lambda m: (anchor.chebyshev_distance(centers[m]), m))
    return anchor, route[:255]


def disk(center, radius):
    tiles = cambc.Bitboard()
    tiles.draw_chebyshev(center, radius)
    return tiles


def encode_pocket(anchor, members):
    # Layout: anchor x, anchor y, count, then one little-endian u16 group index per member.
    return struct.pack(f"<BBB{len(members)}H", anchor.x, anchor.y, len(members), *members)


def decode_pocket(data):
    if len(data) < 3 or len(data) < 3 + 2 * data[2]:
        return None, []
    x, y, count = data[0], data[1], data[2]
    return cambc.Vec2(x, y), list(struct.unpack_from(f"<{count}H", data, 3))


def play():
    # Submit before picking: the pick ends our input for the turn and later memory access is rejected.
    global _memory
    if _memory is None:
        anchor, members = compute_pocket()
        _memory = encode_pocket(anchor, members) if anchor is not None else b""
    cambc.draft_memory.submit(_memory)

    chosen = cambc.draft.current_draft(cambc.Team.ALLY)
    for pick in ROSTER:
        if pick not in chosen:
            cambc.draft.pick(pick)
            break
