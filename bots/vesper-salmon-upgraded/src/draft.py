import struct

import cambc
from cambc import game_map

Draft, Camp = cambc.ChampionDraft, cambc.CampType

# Picks in order, by the team size the map asks for (3 to 5).
ROSTER_5 = (Draft.CASSI_AND_ISSAC, Draft.VARG, Draft.VENDETTA, Draft.WILLOW, Draft.VESPER_VALE)
ROSTER_4 = (Draft.CASSI_AND_ISSAC, Draft.VARG, Draft.VESPER_VALE, Draft.REN)
ROSTER_3 = (Draft.CASSI_AND_ISSAC, Draft.VARG, Draft.VESPER_VALE)
ROSTERS = {3: ROSTER_3, 4: ROSTER_4, 5: ROSTER_5}


# Groups made only of these kinds can join the pocket.
POCKET_CAMP_KINDS = (Camp.WOLF, Camp.GEM_CLUSTER, Camp.GOLD_CLUSTER, Camp.ELK, Camp.MONOLITH)
DENSITY_RADIUS = 8
# Groups this close to the pocket anchor are farmed while the pocket respawns.
ROAM_RADIUS = 60

# Group value, in gems. Early farming rates gold above gems; the aggressive route keeps the lower weight.
FARM_GOLD_WEIGHT = 1.5
GOLD_WEIGHT = 0.6
# Gems lost per point of health the team takes while clearing.
DAMAGE_COST = 0.01
# Rough team basic-attack damage per round, used to estimate clear time.
TEAM_DAMAGE = 200
# Walking distance to the nearest altar at which a pocket is worth half.
ALTAR_FALLOFF = 20
UNREACHABLE_GAP = 255
# Aggressive route: distance to the map centre at which a pocket is worth half, and the closest
# walking distance to the enemy spawn a group may sit at.
CENTER_FALLOFF = 5
ENEMY_SPAWN_BUFFER = 12
# The aggressive route stays on the centre pocket and its neighbours.
AGGRESSIVE_RADIUS = 12

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


def group_value(group, gold_weight=GOLD_WEIGHT):
    # Reward minus the health the team trades for it; never negative.
    damage_taken = sum(CAMP_STATS.get(camp.kind(), (0, 0))[1] for camp in group.camps()) * clear_rounds(group)
    reward = group.gem_value() + gold_weight * group.gold_value()
    return max(0.0, reward - DAMAGE_COST * damage_taken)


def group_income(group, gold_weight=GOLD_WEIGHT):
    # Value per round when the group is farmed every time it returns.
    cycle = group.max_respawn_timer() + clear_rounds(group) + cambc.constants.CAPTURE_TIME
    return group_value(group, gold_weight) / max(1, cycle)


def walking_gaps(groups, indexes, roots):
    # Walking distance from each group to the nearest root tile.
    field = cambc.DistanceField()
    reached = cambc.Bitboard()
    field.compute(game_map.walls() | game_map.holes(), roots, reached)
    gaps = {}
    for index in indexes:
        steps = [field.get(t) for t in groups[index].spawn_tiles().positions() if reached.is_set(t)]
        gaps[index] = min(steps) if steps else UNREACHABLE_GAP
    return gaps


def compute_pocket(aggressive=False):
    # Returns the anchor tile and route group indexes, nearest to the anchor first.
    # The normal route favours pockets near an altar; the aggressive one favours the map centre
    # and leaves out groups close to the enemy spawn.
    groups = game_map.camp_groups()
    centers = {}
    for index, group in enumerate(groups):
        if group.camps() and all(camp.kind() in POCKET_CAMP_KINDS for camp in group.camps()):
            centers[index] = group_center(group)
    if aggressive:
        enemy_gaps = walking_gaps(groups, centers, game_map.spawn_region(cambc.Team.ENEMY))
        centers = {index: center for index, center in centers.items() if enemy_gaps[index] >= ENEMY_SPAWN_BUFFER}
    if not centers:
        return None, []

    weight = GOLD_WEIGHT if aggressive else FARM_GOLD_WEIGHT
    incomes = {index: group_income(groups[index], weight) for index in centers}
    if aggressive:
        middle = cambc.Vec2(game_map.width() // 2, game_map.height() // 2)
        gaps = {index: center.chebyshev_distance(middle) for index, center in centers.items()}
        falloff = CENTER_FALLOFF
    else:
        gaps = walking_gaps(groups, centers, game_map.altars())
        falloff = ALTAR_FALLOFF

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
        score = sum(incomes[m] for m in members) * falloff / (falloff + gap)
        if best is None or score > best[0]:
            best = (score, index)

    anchor = centers[best[1]]
    radius = AGGRESSIVE_RADIUS if aggressive else ROAM_RADIUS
    route = [m for tile in (board & disk(anchor, radius)).positions() for m in at_tile[tile]]
    route.sort(key=lambda m: (anchor.chebyshev_distance(centers[m]), m))
    return anchor, route[:255]


def disk(center, radius):
    tiles = cambc.Bitboard()
    tiles.draw_chebyshev(center, radius)
    return tiles


def encode_pocket(anchor, members):
    # Layout: anchor x, anchor y, count, then one little-endian u16 group index per member.
    return struct.pack(f"<BBB{len(members)}H", anchor.x, anchor.y, len(members), *members)


def decode_pocket(data, offset=0):
    # Returns the anchor, members and the offset just past this block.
    if len(data) < offset + 3 or len(data) < offset + 3 + 2 * data[offset + 2]:
        return None, [], offset
    x, y, count = data[offset], data[offset + 1], data[offset + 2]
    members = list(struct.unpack_from(f"<{count}H", data, offset + 3))
    return cambc.Vec2(x, y), members, offset + 3 + 2 * count


def encode_routes():
    # Normal route block, then the aggressive route block trimmed to fit draft memory.
    anchor, members = compute_pocket()
    if anchor is None:
        return b""
    memory = encode_pocket(anchor, members)
    anchor, members = compute_pocket(aggressive=True)
    room = (cambc.constants.DRAFT_MEMORY_BYTES - len(memory) - 3) // 2
    if anchor is not None and room > 0:
        memory += encode_pocket(anchor, members[:room])
    return memory


def play():
    # Submit before picking: the pick ends our input for the turn and later memory access is rejected.
    global _memory
    if _memory is None:
        _memory = encode_routes()
    cambc.draft_memory.submit(_memory)

    chosen = cambc.draft.current_draft(cambc.Team.ALLY)
    size = min(max(cambc.draft.team_size(), min(ROSTERS)), max(ROSTERS))
    for pick in ROSTERS[size]:
        if pick not in chosen:
            cambc.draft.pick(pick)
            break
