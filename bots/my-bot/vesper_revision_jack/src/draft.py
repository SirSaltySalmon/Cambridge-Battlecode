import struct
import cambc

# ---------------------------------------------------------------------------
# Lazy Game Map Proxy
# Prevents eager evaluation of cambc.game_map during module preloading.
# ---------------------------------------------------------------------------
class _LazyGameMap:
    def __getattr__(self, name):
        return getattr(cambc.game_map, name)

game_map = _LazyGameMap()

# Numeric constants (safe to evaluate at module load time)
DENSITY_RADIUS = 8
ROAM_RADIUS = 60
GOLD_WEIGHT = 0.2
DAMAGE_COST = 0.01
TEAM_DAMAGE = 200
ALTAR_FALLOFF = 20
UNREACHABLE_GAP = 255

_memory = None

# Lazy containers for SDK enums and tables
_ROSTER = None
_POCKET_CAMP_KINDS = None
_CAMP_STATS = None


def get_roster():
    global _ROSTER
    if _ROSTER is None:
        Draft = cambc.ChampionDraft
        _ROSTER = (Draft.VESPER_VALE, Draft.VARG, Draft.REN, Draft.VENDETTA, Draft.CASSI_AND_ISSAC)
    return _ROSTER


def get_pocket_camp_kinds():
    global _POCKET_CAMP_KINDS
    if _POCKET_CAMP_KINDS is None:
        Camp = cambc.CampType
        _POCKET_CAMP_KINDS = (Camp.WOLF, Camp.GEM_CLUSTER, Camp.GOLD_CLUSTER, Camp.ELK, Camp.MONOLITH)
    return _POCKET_CAMP_KINDS


def get_camp_stats():
    global _CAMP_STATS
    if _CAMP_STATS is None:
        Camp = cambc.CampType
        _CAMP_STATS = {
            Camp.WOLF: (1350, 20),
            Camp.ELK: (2400, 60),
            Camp.GEM_CLUSTER: (500, 0),
            Camp.GOLD_CLUSTER: (500, 0),
            Camp.MONOLITH: (20000, 200),
        }
    return _CAMP_STATS


# ---------------------------------------------------------------------------
# Module Property Proxy for Direct Imports from bot.py
# If bot.py does `from draft import CAMP_STATS` or accesses `draft.CAMP_STATS`
# ---------------------------------------------------------------------------
def __getattr__(name):
    if name == "CAMP_STATS":
        return get_camp_stats()
    if name == "ROSTER":
        return get_roster()
    if name == "POCKET_CAMP_KINDS":
        return get_pocket_camp_kinds()
    raise AttributeError(f"module 'draft' has no attribute '{name}'")


# ---------------------------------------------------------------------------
# Calculation & Pathing Helpers
# ---------------------------------------------------------------------------

def group_center(group):
    tiles = group.spawn_tiles().positions()
    return cambc.Vec2(sum(t.x for t in tiles) // len(tiles), sum(t.y for t in tiles) // len(tiles))


def clear_rounds(group):
    camp_stats = get_camp_stats()
    return sum(camp_stats.get(camp.kind(), (0, 0))[0] for camp in group.camps()) / TEAM_DAMAGE


def group_value(group):
    # Reward minus the health the team trades for it; never negative.
    camp_stats = get_camp_stats()
    damage_taken = sum(camp_stats.get(camp.kind(), (0, 0))[1] for camp in group.camps()) * clear_rounds(group)
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


def disk(center, radius):
    tiles = cambc.Bitboard()
    tiles.draw_chebyshev(center, radius)
    return tiles


def compute_pocket():
    # Returns the anchor tile and route group indexes, nearest to the anchor first.
    groups = game_map.camp_groups()
    pocket_camp_kinds = get_pocket_camp_kinds()
    centers = {}
    for index, group in enumerate(groups):
        if group.camps() and all(camp.kind() in pocket_camp_kinds for camp in group.camps()):
            centers[index] = group_center(group)
    if not centers:
        return None, []

    incomes = {index: group_income(groups[index]) for index in centers}
    gaps = altar_gaps(groups, centers)

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

    if best is None:
        return None, []

    anchor = centers[best[1]]
    route = [m for tile in (board & disk(anchor, ROAM_RADIUS)).positions() for m in at_tile[tile]]
    route.sort(key=lambda m: (anchor.chebyshev_distance(centers[m]), m))
    return anchor, route[:255]


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
    for pick in get_roster():
        if pick not in chosen:
            cambc.draft.pick(pick)
            break