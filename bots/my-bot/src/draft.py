import struct

import cambc
from cambc import game_map

Draft, Camp = cambc.ChampionDraft, cambc.CampType

# Picks in order; the game may ask for fewer than five.
ROSTER = (Draft.CASSI_AND_ISSAC, Draft.VENDETTA, Draft.VARG, Draft.WILLOW, Draft.REN)
# Groups made only of these kinds can join the pocket.
POCKET_CAMP_KINDS = (Camp.WOLF, Camp.GEM_CLUSTER, Camp.GOLD_CLUSTER, Camp.ELK, Camp.MONOLITH)
DENSITY_RADIUS = 8

_memory = None


def group_center(group):
    tiles = group.spawn_tiles().positions()
    return cambc.Vec2(sum(t.x for t in tiles) // len(tiles), sum(t.y for t in tiles) // len(tiles))


def compute_pocket():
    # Returns the anchor tile and pocket group indexes, nearest to the anchor first.
    groups = game_map.camp_groups()
    centers = {}
    for index, group in enumerate(groups):
        if group.camps() and all(camp.kind() in POCKET_CAMP_KINDS for camp in group.camps()):
            centers[index] = group_center(group)
    if not centers:
        return None, []

    # Count neighbours with native bitboard ops; Python pair loops are too costly in fuel.
    board = cambc.Bitboard()
    for center in centers.values():
        board.set(center)
    counts = {index: (board & disk(center)).count() for index, center in centers.items()}
    top = max(counts.values())

    map_center = cambc.Vec2(game_map.width() // 2, game_map.height() // 2)
    best = None
    for index in (i for i, count in counts.items() if count == top):
        members = pocket_members(centers, centers[index])
        middle = cambc.Vec2(sum(centers[m].x for m in members) // len(members),
                            sum(centers[m].y for m in members) // len(members))
        key = middle.chebyshev_distance(map_center)
        if best is None or key < best[0]:
            best = (key, index, members)

    _, anchor_index, members = best
    anchor = centers[anchor_index]
    members.sort(key=lambda m: (anchor.chebyshev_distance(centers[m]), m))
    return anchor, members[:255]


def disk(center):
    tiles = cambc.Bitboard()
    tiles.draw_chebyshev(center, DENSITY_RADIUS)
    return tiles


def pocket_members(centers, anchor):
    return [index for index, spot in centers.items()
            if anchor.chebyshev_distance(spot) <= DENSITY_RADIUS]


def encode_pocket(anchor, members):
    # Layout: anchor x, anchor y, count, then one little-endian u16 group index per member.
    return struct.pack(f"<BBB{len(members)}H", anchor.x, anchor.y, len(members), *members)


def decode_pocket(data):
    if len(data) < 3 or len(data) < 3 + 2 * data[2]:
        return None, []
    x, y, count = data[0], data[1], data[2]
    return cambc.Vec2(x, y), list(struct.unpack_from(f"<{count}H", data, 3))


def play():
    # Pick first so a long pocket search can never cost us the pick.
    chosen = cambc.draft.current_draft(cambc.Team.ALLY)
    for pick in ROSTER:
        if pick not in chosen:
            cambc.draft.pick(pick)
            break

    global _memory
    if _memory is None:
        anchor, members = compute_pocket()
        _memory = encode_pocket(anchor, members) if anchor is not None else b""
    cambc.draft_memory.submit(_memory)
