import struct

import cambc
from cambc import game_map

import draft

Team, Champ, Kind, Item = cambc.Team, cambc.ChampionType, cambc.AbilityKind, cambc.ItemType

# Turn thresholds.
RECALL_HEALTH = 0.35
GEM_BANK = 40
THREAT_RANGE = 4
# While the target is still respawning, bank any bag at least this big.
IDLE_BANK = 10
# Recall to shop after a tribute when carrying this much gold.
RECALL_GOLD = 150
# Vesper collects most loot, so her purse fills fast; a higher bar keeps her with the team.
VESPER_RECALL_GOLD = 450

# Walking is priced in rounds per tile. A camp on the path only adds its extra tiles;
# a camp off the path has to be worth the longer walk.
WALK_WEIGHT = 2.0
# Wait this far from a respawning group; standing on its spawn tiles delays it.
WAIT_RANGE = 2
# Every group is a possible stop on the way, except monoliths until the team owns this many emblems.
MONOLITH_EMBLEMS = 99
# Visible loot this close is taken even if it is slightly off the path to the planned goal.
NEAR_LOOT = 8
# Extra walking tiles allowed for a stop to still count as being on the way.
ON_WAY_SLACK = 4
# A champion back from spawn rejoins the team once this close to the shared target; until then it can't lead.
TEAM_RANGE = 8
# A capture claim is dropped after this many rounds so a dead or lost collector gets replaced.
CAPTURE_TIMEOUT = 15
# Vesper's core build is four emblems; fallback purchases continue after the core is complete.
VESPER_CORE_EMBLEMS = 4
# Vesper takes over a capture claim herself when the cleared group is this close to her.
VESPER_CLAIM_RANGE = 8
# A visible cleared camp is captured when no enemy stands inside this radius of it.
CAPTURE_SAFE_RANGE = 8
# Claim score added while Vesper is still under her core build. One emblem of item worth is 150.
VESPER_CLAIM_BONUS = 12
# Leave this much fuel so a long route search still reaches the move.
ROUTE_FUEL_RESERVE = 40000
# A split must beat the single-party route by this factor.
SPLIT_MARGIN = 1.15
# Camps in the draft pocket are nudged, not fenced in.
POCKET_BONUS = 1.1
# Gems of risk per point of camp-strength times clear rounds.
ROUTE_DAMAGE_COST = 0.05
# Extra risk when a route walks a still-building Vesper into an enemy's reach.
VESPER_ROUTE_RISK = 40

# A bag this big stays behind the team instead of fighting at the front.
ESCORT_GEMS = 150
# A contested altar is legal only when this many other allies are within CONTEST_RANGE.
CONTEST_ALLIES = 2
CONTEST_RANGE = 4
# A carrier below this health stays behind the team instead of fighting at the front.
FRONT_HEALTH = 800
# Altars this close to the map centre are contested, so heavy bags only go there with allies nearby.
CENTRAL_ALTAR_RANGE = 10

# Wards: spots near the map centre and on a ring around the enemy spawn, at most one ward per spot.
# No two spots, and no new ward and a visible allied ward, sit within the anti-ward radius of each other.
ANTI_WARD_RADIUS = 8
CENTRAL_WARDS = 2
ENEMY_SPAWN_WARDS = 3
# Ring distance from the enemy spawn; ward vision is 6, so the ring sees the spawn exits.
ENEMY_WARD_GAP = 7
# A ward still covers its spot when it lands this close to it.
WARD_SLACK = 1
# Walk to an uncovered spot this far away when the route leaves time; further on the aggressive route.
WARD_DETOUR_RANGE = 6
AGGRESSIVE_WARD_DETOUR_RANGE = 12

# Fight score.
FIGHT_THRESHOLD = 5
OUTNUMBER_BONUS = 10
# One fewer body than the enemy. A small damage edge must not buy a fight we are down.
MISSING_BODY_PENALTY = 25
# Inside this ring of the enemy spawn, only an enemy who dies before reaching it is chased.
SPAWN_CHASE_RANGE = 8
# A failed chase walks out to here before it will turn around. An enemy who follows past this line is caught.
SPAWN_RETREAT_RANGE = 16
# A 0.05 gap between kill rate and die rate matches the old +10 of winning both health and power.
EXCHANGE_WEIGHT = 200
DISTANCE_PENALTY = 0.5
# A champion under this max health is squishy, and fights less while ahead of every melee ally.
SQUISHY_HEALTH = 900
OVEREXTEND_PENALTY = 10
# Standing on our own spawn is safe: it heals us, so a fight scores higher.
SPAWN_BONUS = 50
# Write a fight beacon below the take-fight threshold so split packs can still regroup.
FIGHT_PING = 3
# Allies we may be missing (a shopper) before a fight is refused.
COHESION_ABSENT = 1
# Fight score loses this much per tile the nearest enemy sits outside the inner leash.
LEASH_WEIGHT = 1.5
LEASH_INNER = 8
LEASH_OUTER = 16
# Farm/tribute/recall utilities sit on the same scale as fight_score.
FARM_SCALE = 2.0
GEM_SCALE = 5.0
TRIBUTE_DIST = 1.0
IDLE_TRIBUTE_BONUS = 4
SHOP_RECALL_SCORE = 12
PANIC_RECALL_SCORE = 80
# While Vesper is still building, ordinary fights and teammate tributes lose to the farm.
BUILD_CAUTION = 10
BUILD_TRIBUTE = 4
# Once she is online, a fight is worth more with her on the leash tile and less without her.
CARRY_HERE = 6
CARRY_AWAY = 6
# Extra tribute points per gem carried, on top of gems / GEM_SCALE.
GEM_PRESSURE = 0.15
# A fight or tribute ping with no refresh dies after this many rounds.
EVENT_TIMEOUT = 2
# Score sentences are for debugging. The action reads the numbers only.
TRACE = False
# Emblems change at the shop. Poll on spawn, and every this many rounds in the field.
EMBLEM_POLL = 10

# Highest acts first, from the round after it is set.
# Ryth's area stun goes out before anyone steps. Stellar casts from behind the tanks.
PRIORITY = {
    Champ.RYTH: 7,
    Champ.CASSI: 6,
    Champ.ISSAC: 5,
    Champ.VENDETTA: 4,
    Champ.VARG: 3,
    Champ.VESPER_VALE: 2,
    Champ.REN: 1,
    Champ.WILLOW: 0,
    Champ.STELLAR: 0,
}

# Draft body order. Each champion places only its own geometric ward spot; no shared ward byte.
WARD_ORDER = (Champ.CASSI, Champ.ISSAC, Champ.VARG, Champ.VENDETTA, Champ.WILLOW,
              Champ.VESPER_VALE, Champ.REN)

# Ranged bodies hold this far from a camp while they are the nearest champion to it.
RANGED_CAMP_STANDOFF = 4

# One family at a time until its emblem is done; the fallback shard is bought without limit.
_MELEE_STRENGTH = (Item.JUNGLE_SHARD_OF_STRENGTH, Item.MONSTER_SHARD_OF_STRENGTH,
                   Item.BRUISER_SHARD_OF_STRENGTH)
_VENDETTA_STRENGTH = (Item.JUNGLE_SHARD_OF_STRENGTH, Item.MONSTER_SHARD_OF_STRENGTH,
                   Item.POWER_SHARD_OF_STRENGTH)
_MELEE_FOCUS = (Item.JUNGLE_SHARD_OF_FOCUS, Item.BRUISER_SHARD_OF_FOCUS,
                Item.DURABILITY_SHARD_OF_FOCUS)
_RANGED_FOCUS = (Item.JUNGLE_SHARD_OF_FOCUS, Item.MONSTER_SHARD_OF_FOCUS,
                 Item.DURABILITY_SHARD_OF_FOCUS)
# Vesper Vale's kit is all basic attacks, so she builds Strength only.
# Ryth's passive turns bonus health into Strength, so the core is health.
# Stellar's spells scale with Focus.
_VESPER_STRENGTH = (Item.MONSTER_SHARD_OF_STRENGTH, Item.POWER_SHARD_OF_STRENGTH,
                    Item.DURABILITY_SHARD_OF_HEALTH, Item.POWER_SHARD_OF_HEALTH)
_RYTH_HEALTH = (Item.DURABILITY_SHARD_OF_HEALTH, Item.JUNGLE_SHARD_OF_HEALTH,
                Item.BRUISER_SHARD_OF_HEALTH)
SHOP_ORDER = {
    Champ.VENDETTA: _VENDETTA_STRENGTH,
    Champ.VARG: _MELEE_STRENGTH,
    Champ.ISSAC: _MELEE_STRENGTH,
    Champ.REN: _MELEE_FOCUS,
    Champ.CASSI: _RANGED_FOCUS,
    Champ.WILLOW: _RANGED_FOCUS,
    Champ.VESPER_VALE: _VESPER_STRENGTH,
    Champ.RYTH: _RYTH_HEALTH,
    Champ.STELLAR: _RANGED_FOCUS,
}
SHOP_FALLBACK = {
    Champ.VENDETTA: Item.POWER_SHARD_OF_STRENGTH,
    Champ.VARG: Item.POWER_SHARD_OF_STRENGTH,
    Champ.ISSAC: Item.POWER_SHARD_OF_STRENGTH,
    Champ.REN: Item.DURABILITY_SHARD_OF_FOCUS,
    Champ.CASSI: Item.POWER_SHARD_OF_FOCUS,
    Champ.WILLOW: Item.POWER_SHARD_OF_FOCUS,
    Champ.RYTH: Item.POWER_SHARD_OF_STRENGTH,
    Champ.STELLAR: Item.POWER_SHARD_OF_FOCUS,
    Champ.VESPER_VALE: (
        Item.POWER_SHARD_OF_STRENGTH,
        Item.POWER_SHARD_OF_STRENGTH,
        Item.POWER_SHARD_OF_HEALTH,
    ),
}

FARM, FIGHT, RETREAT, TRIBUTE, RECALL, CAPTURE = 0, 1, 2, 3, 4, 5
ACTION_NAMES = ("farm", "fight", "retreat", "tribute", "recall", "capture")

# Billboard, 8 bytes. The route itself lives in draft memory.
# Byte 0 is the farm-route index. Bytes 1-5 are one enemy sighting: quantized raw fight score,
# x, y, champion id, and the round it was written. A higher raw score overwrites it.
# Bytes 6-7 are the capture claim: route slot, and the collector (champion id + 1, 0 if none).
SHARED_LAYOUT = "<BBBBBBBB"
NO_CAPTURE = 255
NO_PEEL = 255
NO_ENEMY = 255
FARM_INDEX_BYTE = 0
PING_SCORE_BYTE = 1
PING_X_BYTE = 2
PING_Y_BYTE = 3
PING_ENEMY_BYTE = 4
PING_STAMP_BYTE = 5

EMBLEMS = tuple(i for i in range(Item.COUNT) if "EMBLEM" in Item(i).name)
SHARDS = tuple(i for i in range(Item.COUNT) if "SHARD" in Item(i).name)
# An emblem is five 30-gold shards.
EMBLEM_WORTH = 150
# Marcella's enhanced attack stuns for one round; the ability record has no duration field.
ENHANCED_STUN = 1


def distance(a, b):
    return a.chebyshev_distance(b)


def at(position):
    return f"@({position.x},{position.y})"


def num(value):
    return f"{value:.1f}".rstrip("0").rstrip(".")


def rate(value):
    return f"{value:.2f}"


def signed(value):
    text = num(value)
    return text if text.startswith("-") else f"+{text}"


class Route:
    def __init__(self, score, gain, risk, time, first):
        self.score = score
        self.gain = gain
        self.risk = risk
        self.time = time
        self.first = first


_CAMP = ("wolf", "elk", "gem", "gold", "mono")


def camp_at(camp):
    try:
        kind = int(camp.kind())
        name = _CAMP[kind] if kind < len(_CAMP) else str(kind)
        return f"{name} {at(camp.position())}"
    except (cambc.Error, AttributeError):
        return "camp"


def unit_at(unit):
    # A swing this turn can remove the unit before the log reads it.
    if unit is None:
        return "unit"
    try:
        return f"c{int(unit.champion_name())} {at(unit.position())}"
    except AttributeError:
        return camp_at(unit)
    except cambc.Error:
        return "unit"


class _Sight:
    # One scan of allies, enemies, and camps until a move, attack, or cast.
    # A new round (including a fuel resume) misses the cache because the round changed.
    def __init__(self):
        self.gen = 0
        self.round = -1
        self.champs = {}
        self.camps = None
        self.camps_gen = -1
        self.alive = {}
        self.blocked = None
        self.blocked_key = None


_sight = _Sight()
_MISSING = object()
_ability_slots = None
_ability_by_kind = {}


def _bump_sight():
    _sight.gen += 1
    _sight.champs.clear()
    _sight.camps = None
    _sight.camps_gen = -1
    _sight.alive.clear()


def _touch_sight():
    rnd = cambc.round_number()
    if rnd != _sight.round:
        _sight.round = rnd
        _bump_sight()
        _sight.blocked = None
        _sight.blocked_key = None


def drop_sight():
    # Who we can see changes after we move, attack, or cast. The occupant mask
    # does not: it stays valid until this body leaves its tile.
    _bump_sight()


def visible_camps():
    # The camp board can still name a tile whose camp left vision this turn.
    _touch_sight()
    if _sight.camps is not None and _sight.camps_gen == _sight.gen:
        return _sight.camps
    found = []
    try:
        tiles = cambc.vision.camp_tiles().positions()
    except cambc.Error:
        _sight.camps = found
        _sight.camps_gen = _sight.gen
        return found
    for tile in tiles:
        try:
            found.append(cambc.vision.camp_at(tile))
        except cambc.Error:
            continue
    _sight.camps = found
    _sight.camps_gen = _sight.gen
    return found


def visible_champions(team):
    # Occupancy can name a tile whose champion is already gone this turn.
    _touch_sight()
    key = int(team)
    hit = _sight.champs.get(key)
    if hit is not None and hit[0] == _sight.gen:
        return hit[1]
    found = []
    try:
        tiles = cambc.vision.champion_tiles(team).positions()
    except cambc.Error:
        _sight.champs[key] = (_sight.gen, found)
        return found
    for tile in tiles:
        try:
            found.append(cambc.vision.champion_at(tile))
        except cambc.Error:
            continue
    _sight.champs[key] = (_sight.gen, found)
    return found


def is_melee(unit):
    try:
        return unit.attack_range() <= 1
    except cambc.Error:
        return False


def tiles_around(position, radius):
    tiles = cambc.Bitboard()
    try:
        tiles.draw_chebyshev(position, radius)
    except cambc.Error:
        pass
    return tiles


def walk_field(roots):
    # Walking steps from the roots around terrain; camps and champions move or die, so they don't block.
    field = cambc.DistanceField()
    reached = cambc.Bitboard()
    try:
        field.compute(game_map.walls() | game_map.holes(), roots, reached)
    except cambc.Error:
        pass
    return field, reached


def steps_to(walk, tiles):
    field, reached = walk
    steps = []
    for tile in tiles:
        try:
            if reached.is_set(tile):
                steps.append(field.get(tile))
        except cambc.Error:
            continue
    return min(steps) if steps else None


def blocked_tiles():
    # Combine terrain and visible occupants using native bitboard operations.
    # Callers mutate the board they get, so the cache hands out a copy.
    # Valid until this body moves; a new round drops it in _touch_sight.
    _touch_sight()
    origin = cambc.me().position()
    key = (origin.x, origin.y, _sight.round)
    if _sight.blocked is not None and _sight.blocked_key == key:
        return _sight.blocked.copy()
    blocked = (game_map.walls() | game_map.holes() | game_map.altars()
               | game_map.spawn_region(Team.ENEMY) | cambc.vision.champion_tiles()
               | cambc.vision.camp_tiles() | cambc.vision.teleport_tiles())
    blocked.set(origin, False)
    _sight.blocked = blocked
    _sight.blocked_key = key
    return blocked.copy()


def move_towards_closest(goals, avoid=None, only_full=False, swing=True):
    me = cambc.me()
    origin = me.position()
    if not me.can_move() or goals.is_set(origin):
        return False

    # Only free goals or occupied camp tiles can seed the field. Never mutate the caller's board.
    blocked = blocked_tiles()
    if avoid is not None:
        blocked |= avoid
        blocked.set(origin, False)
    goals = goals & (~blocked | cambc.vision.camp_tiles())
    if goals.is_empty():
        return False
    # Melee can swing and still walk. A ranged basic spends the walk, so only melee swings on the way.
    if swing and me.attack_range() <= 1:
        kite(me)
        if not me.can_move():
            return True
    dash = ability_of(me, Kind.DASH)
    if dash is not None and not dash.is_ready():
        dash = None
    try:
        if dash is None and avoid is None and goals.count() == 1 and not blocked.is_set(goals.positions()[0]):
            moved = me.move_towards(goals.positions()[0])
            if moved:
                drop_sight()
            return moved

        # Expand from the goals until our current tile is reached.
        field = cambc.DistanceField()
        reached = cambc.Bitboard()
        field.compute_to(blocked & ~goals, goals, tiles_around(origin, 0), reached)

        # Dash whenever it is ready (it leaves the walk free), then walk on from where it lands.
        dashed = dash is not None and dash_along(dash, origin, field, reached & ~blocked, only_full)
        if dashed:
            origin = me.position()
            blocked = blocked_tiles() if avoid is None else (blocked_tiles() | avoid)
            blocked.set(origin, False)
            if goals.is_set(origin) or not me.can_move():
                return True

        # Choose a reachable step with a smaller distance to the goal.
        reachable = blocked.reachable_tiles(origin, me.base_stats().base_move_speed)
        step = min([origin, *(reachable & reached).positions()], key=field.get)
        if step != origin and me.move_absolute(step):
            drop_sight()
            return True
        return dashed
    except cambc.Error:
        return False


def dash_along(dash, origin, field, landings, only_full=False):
    # Land on the tile that gains the most ground towards the goal; any gain is worth the cooldown.
    # only_full: a gap shorter than the dash would land past the tile we need, so walk instead.
    here = field.get(origin)
    span = int(dash.info().range)
    if only_full and here < span:
        return False
    tiles = tiles_around(origin, span) & landings
    for tile in sorted(tiles.positions(), key=field.get):
        if field.get(tile) >= here:
            break
        if cast(dash, tile):
            return True
    return False


def attack_if_in_range(target):
    # Make one basic attack if the target is already in attack range.
    me = cambc.me()
    try:
        if (not me.can_attack() or not target.is_visible() or target.health() <= 0
                or not me.attack_tiles().is_set(target.position())):
            return False
        me.attack(target)
        drop_sight()
        return True
    except cambc.Error:
        return False


def kite(me, extra=()):
    # Hit whoever is already in range. A basic that leaves the move free is taken on the way.
    if not me.can_attack():
        return None
    enemies = [e for e in alive(visible_champions(Team.ENEMY)) if within(me, e, me.attack_range())]
    target = lowest([e for e in enemies if channelling(e)]) or lowest(enemies)
    if target is None:
        target = lowest(c for c in extra if within(me, c, me.attack_range()))
    if target is not None and attack_if_in_range(target):
        return target
    return None


def dash_leaves_range(me, enemies):
    # True when a dash away from the nearest enemy lands outside every enemy's attack range.
    dash = ability_of(me, Kind.DASH)
    if dash is None or not dash.is_ready():
        return False
    origin = me.position()
    threats = []
    for enemy in enemies:
        try:
            threats.append((enemy.position(), enemy.attack_range()))
        except cambc.Error:
            continue
    if not threats or not any(distance(origin, pos) <= reach for pos, reach in threats):
        return False
    nearest = min(threats, key=lambda item: distance(origin, item[0]))
    free = tiles_around(origin, int(dash.info().range)) & ~blocked_tiles()
    for tile in free.positions():
        if distance(tile, nearest[0]) <= distance(origin, nearest[0]):
            continue
        if all(distance(tile, pos) > reach for pos, reach in threats):
            return True
    return False


def step_out(me, goals, extra=()):
    # A dash that clears every attack range goes first; it leaves the basic free, so the swing can follow.
    # Otherwise melee swings and still walks. A ranged basic spends the walk, so they step out and
    # only swing when that step never happened.
    enemies = alive(visible_champions(Team.ENEMY))
    if dash_leaves_range(me, enemies):
        move_towards_closest(goals, swing=False)
        return kite(me, extra)
    if me.attack_range() > 1:
        if move_towards_closest(goals, swing=False):
            return None
        return kite(me, extra)
    hit = kite(me, extra)
    if me.can_move():
        move_towards_closest(goals, swing=False)
    return hit


def alive(units):
    # The full vision lists are scanned once per generation. One-off lists still
    # re-read health, and a stored identity check stops a recycled id from hitting.
    _touch_sight()
    key = id(units)
    hit = _sight.alive.get(key)
    if hit is not None and hit[0] == _sight.gen and hit[2] is units:
        return list(hit[1])
    living = []
    for unit in units:
        try:
            if unit.is_visible() and unit.health() > 0:
                living.append(unit)
        except cambc.Error:
            pass
    _sight.alive[key] = (_sight.gen, living, units)
    return list(living)


def lowest(units):
    best = None
    best_health = None
    for unit in units:
        try:
            health = unit.health()
        except cambc.Error:
            continue
        if best is None or health < best_health:
            best = unit
            best_health = health
    return best


def within(me, unit, radius):
    try:
        return distance(me.position(), unit.position()) <= radius
    except cambc.Error:
        return False


def spot(unit):
    try:
        return unit.position()
    except cambc.Error:
        return None


def ability_of(me, kind):
    # Both slots are fixed for the match. Cooldown is read later off the same object.
    global _ability_slots
    hit = _ability_by_kind.get(kind, _MISSING)
    if hit is not _MISSING:
        return hit
    if _ability_slots is None:
        try:
            _ability_slots = list(me.abilities())
        except cambc.Error:
            return None
    found = None
    for ability in _ability_slots:
        if ability.has(kind):
            found = ability
            break
    _ability_by_kind[kind] = found
    return found


def cast(ability, target=None):
    if ability is None:
        return False
    try:
        if ability.is_ready() and bool(ability.use(target)):
            drop_sight()
            return True
    except cambc.Error:
        return False
    return False


def cast_self(ability):
    # Self-cast is untargeted in the SDK; fall back to targeting ourselves.
    if ability is None or not ability.is_ready():
        return False
    for target in (None, cambc.me()):
        try:
            used = bool(ability.use(target))
        except cambc.Error:
            continue
        if used:
            drop_sight()
        return used
    return False


def spell_damage(ability):
    try:
        return float(ability.damage_amount())
    except (cambc.Error, TypeError, ValueError):
        return 0.0


def aoe_centers(origin, places, reach, radius):
    # A body inside the cast can be the centre. One just past it needs a tile in between.
    centers = []
    seen = set()
    reach = int(reach)
    radius = int(radius)
    for place in places:
        gap = distance(origin, place)
        if gap <= reach:
            tiles = (place,)
        elif gap <= reach + radius:
            tiles = (tiles_around(place, radius) & tiles_around(origin, reach)).positions()
        else:
            continue
        for tile in tiles:
            key = (tile.x, tile.y)
            if key in seen:
                continue
            seen.add(key)
            centers.append(tile)
    return centers


def aoe_aim(origin, bodies, reach, radius, focus=None):
    # More bodies first, then a centre that still covers the chosen target.
    places = []
    for unit in bodies:
        place = spot(unit)
        if place is not None:
            places.append(place)
    if not places:
        return None, 0
    focus_at = spot(focus) if focus is not None else None
    best = None
    best_key = None
    for center in aoe_centers(origin, places, reach, radius):
        hits = sum(1 for place in places if distance(center, place) <= radius)
        if hits <= 0:
            continue
        covers = focus_at is not None and distance(center, focus_at) <= radius
        key = (hits, covers, -distance(origin, center))
        if best_key is None or key > best_key:
            best = center
            best_key = key
    return best, (best_key[0] if best_key else 0)


def cast_aoe(me, ability, bodies, focus, require):
    if ability is None or not ability.is_ready():
        return False
    try:
        info = ability.info()
        center, hits = aoe_aim(me.position(), bodies, info.range, info.radius, focus)
    except (cambc.Error, AttributeError, TypeError, ValueError):
        return False
    if center is None or hits < require:
        return False
    return cast(ability, center)


def channelling(champion):
    try:
        return champion.is_recalling() or champion.is_submitting_gems()
    except cambc.Error:
        return False


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


# Item passive fractions, read once. The rules are 0.10 / 0.30; a live 10 / 30 is percent.
_FAMILY = None


def family_fractions():
    global _FAMILY
    if _FAMILY is not None:
        return _FAMILY
    found = {}
    for item_id in EMBLEMS:
        item = Item(item_id)
        kind = item.passive()
        if kind is None or kind in found:
            continue
        magnitude = float(item.passive_magnitude())
        if magnitude > 1:
            magnitude /= 100.0
        found[kind] = magnitude
    _FAMILY = found
    return _FAMILY


def emblem_mods(champion):
    # One passive per family. Monster raises camp damage; Jungle and Durability stack on camp hits.
    monster = jungle = durability = 0.0
    try:
        counts = champion.item_count()
    except cambc.Error:
        return 1.0, 1.0, 1.0
    fractions = family_fractions()
    seen = set()
    for item_id in EMBLEMS:
        if counts[item_id] <= 0:
            continue
        kind = Item(item_id).passive()
        if kind is None or kind in seen:
            continue
        seen.add(kind)
        frac = fractions.get(kind, 0.0)
        if kind == cambc.ItemPassive.MONSTER:
            monster = frac
        elif kind == cambc.ItemPassive.JUNGLE:
            jungle = frac
        elif kind == cambc.ItemPassive.DURABILITY:
            durability = frac
    taken = max(1e-6, 1.0 - durability)
    return 1.0 + monster, taken, max(1e-6, 1.0 - jungle) * taken


def _cooldown(info):
    value = getattr(info, "max_cooldown", None)
    if value is None:
        value = info.properties.max_cooldown
    return max(1, int(value))


def _rounds(info):
    value = getattr(info, "duration", 0)
    try:
        return int(value or 0)
    except (TypeError, ValueError):
        return 0


class Profile:
    # One champion's sustained round of combat, before stun uptime is applied.
    def __init__(self):
        self.name = None
        self.health = 1.0
        self.max_health = 1.0
        self.position = None
        self.melee = False
        self.camp_out = 1.0
        self.taken = 1.0
        self.camp_taken = 1.0
        self.basic = 0.0
        self.basic_hits = 1.0
        self.single_dps = 0.0
        self.single_hits = 0.0
        self.camp_single_dps = 0.0
        self.areas = []
        self.camp_areas = []
        self.stun_single = 0.0
        self.stun_areas = []
        self.camp_stun_single = 0.0
        self.camp_stun_areas = []
        self.splash = 0.0
        self.heal = 0.0
        self.shield = 0.0
        self.retaliation = 0.0
        self.immune = False
        self.threat = 0.0


def _blank_profile(champion):
    profile = Profile()
    try:
        profile.name = champion.champion_name()
    except cambc.Error:
        pass
    try:
        profile.health = float(champion.health())
        profile.max_health = float(champion.max_health())
    except (cambc.Error, ValueError):
        pass
    try:
        profile.position = champion.position()
    except cambc.Error:
        pass
    try:
        profile.melee = champion.attack_range() <= 1
    except cambc.Error:
        pass
    profile.camp_out, profile.taken, profile.camp_taken = emblem_mods(champion)
    profile.splash_on = False
    profile.retal_on = False
    try:
        passive = champion.passive()
        profile.immune = passive == cambc.PassiveType.STUN_IMMUNITY
        profile.splash_on = passive == cambc.PassiveType.SPLASH_ATTACK
        profile.retal_on = passive == cambc.PassiveType.RETALIATION
    except cambc.Error:
        pass
    profile.spells = None
    return profile


def _per_focus(amount, focus, info):
    # Spells are focus times a fixed modifier. The ratio is read once with the kit.
    if focus > 1e-6:
        return amount / focus
    modifier = getattr(info, "modifier", 0.0)
    try:
        return float(modifier or 0.0)
    except (TypeError, ValueError):
        return 0.0


def _parse_spells(champion, focus):
    # Cooldown, radius, and targeting never change. Damage is reapplied from focus later.
    spells = []
    try:
        slots = champion.abilities()
    except cambc.Error:
        return spells
    for ability in slots:
        try:
            info = ability.info()
        except (cambc.Error, ValueError):
            continue
        if info is None:
            continue
        try:
            cooldown = _cooldown(info)
        except (cambc.Error, ValueError, AttributeError):
            continue
        if ability.is_attack_reset():
            spells.append(("reset", cooldown, bool(getattr(info, "enhanced_attack", False))))
            continue
        if ability.is_heal():
            try:
                amount = float(ability.heal_amount())
            except (cambc.Error, ValueError):
                amount = 0.0
            spells.append(("heal", cooldown, _per_focus(amount, focus, info)))
            continue
        if ability.is_shield():
            try:
                amount = float(ability.shield_amount())
            except (cambc.Error, ValueError):
                amount = 0.0
            spells.append(("shield", cooldown, _per_focus(amount, focus, info)))
            continue
        damaging = ability.is_damage() or ability.is_aoe_damage() or ability.is_cc()
        if not damaging:
            continue
        try:
            hit = float(ability.damage_amount())
        except (cambc.Error, ValueError):
            hit = 0.0
        ratio = _per_focus(hit, focus, info)
        try:
            camps = ability.can_target_camps() or (ability.is_aoe_damage() and ability.can_target_ground())
            hits_enemies = ability.can_target_enemies()
        except cambc.Error:
            continue
        if ability.is_aoe_damage():
            try:
                radius = float(info.radius)
            except (TypeError, ValueError):
                continue
            spells.append(("aoe", cooldown, ratio, radius, _rounds(info), bool(camps)))
            continue
        duration = _rounds(info) if ability.is_cc() else 0
        spells.append(("hit", cooldown, ratio, duration, bool(hits_enemies), bool(camps)))
    return spells


def _clear_combat(profile):
    profile.basic = 0.0
    profile.basic_hits = 1.0
    profile.single_dps = 0.0
    profile.single_hits = 0.0
    profile.camp_single_dps = 0.0
    profile.areas = []
    profile.camp_areas = []
    profile.stun_single = 0.0
    profile.stun_areas = []
    profile.camp_stun_single = 0.0
    profile.camp_stun_areas = []
    profile.splash = 0.0
    profile.heal = 0.0
    profile.shield = 0.0
    profile.retaliation = 0.0
    profile.threat = 0.0


def _apply_spells(profile, power, focus):
    _clear_combat(profile)
    profile.basic = power
    for spell in profile.spells or ():
        kind = spell[0]
        cooldown = spell[1]
        if kind == "reset":
            profile.basic += power / cooldown
            profile.basic_hits += 1.0 / cooldown
            if spell[2]:
                frac = ENHANCED_STUN / cooldown
                profile.stun_single += frac
                profile.camp_stun_single += frac
            continue
        per = spell[2] * focus
        if kind == "heal":
            profile.heal += per / cooldown
            continue
        if kind == "shield":
            profile.shield += per / cooldown
            continue
        dps = per / cooldown
        rate = 1.0 / cooldown
        if kind == "aoe":
            radius, duration, camps = spell[3], spell[4], spell[5]
            profile.areas.append((dps, radius, rate))
            if camps and dps:
                profile.camp_areas.append((dps, radius))
            if duration > 0:
                frac = duration / cooldown
                profile.stun_areas.append((radius, frac))
                if camps:
                    profile.camp_stun_areas.append((radius, frac))
            continue
        duration, hits_enemies, camps = spell[3], spell[4], spell[5]
        if hits_enemies:
            if dps:
                profile.single_dps += dps
                profile.single_hits += rate
            if duration > 0:
                profile.stun_single += duration / cooldown
        if camps:
            if dps:
                profile.camp_single_dps += dps
            if duration > 0:
                profile.camp_stun_single += duration / cooldown
    if profile.splash_on:
        profile.splash = power * profile.basic_hits
    elif profile.retal_on:
        profile.retaliation = cambc.constants.passive_abilities.REN_RETALIATION_FOCUS_RATIO * focus
    profile.threat = profile.basic + profile.single_dps + sum(area[0] for area in profile.areas)


def _refresh_profile(profile, champion):
    # Health, position, strength, focus, and items move. The kit does not.
    try:
        profile.health = float(champion.health())
        profile.max_health = float(champion.max_health())
    except (cambc.Error, ValueError):
        pass
    try:
        profile.position = champion.position()
    except cambc.Error:
        profile.position = None
    profile.camp_out, profile.taken, profile.camp_taken = emblem_mods(champion)
    try:
        power = float(champion.strength())
        focus = float(champion.focus())
    except (cambc.Error, ValueError):
        _clear_combat(profile)
        profile.basic = draft.TEAM_DAMAGE / 5
        profile.threat = profile.basic
        return
    _apply_spells(profile, power, focus)


def _profile_key(champion):
    try:
        return (int(champion.team()), int(champion.champion_name()))
    except cambc.Error:
        return id(champion)


def _in_radius(body, focus, radius):
    if body.position is None or focus.position is None:
        return body is focus
    return distance(body.position, focus.position) <= radius


def stun_fractions(sources, victims):
    # Single-target stuns land on the highest damage body. Area stuns land around the lowest health one.
    fractions = [0.0] * len(victims)
    if not victims or not sources:
        return fractions
    focus_i = min(range(len(victims)), key=lambda i: victims[i].health)
    carry_i = max(range(len(victims)), key=lambda i: victims[i].threat)
    focus = victims[focus_i]
    for source in sources:
        if source.stun_single and not victims[carry_i].immune:
            fractions[carry_i] += source.stun_single
        for radius, frac in source.stun_areas:
            if frac <= 0:
                continue
            for i, victim in enumerate(victims):
                if victim.immune:
                    continue
                if _in_radius(victim, focus, radius):
                    fractions[i] += frac
    return [min(1.0, frac) for frac in fractions]


def strike(attackers, defenders):
    # Raw damage into the defender pool, damage on each body, and the retaliation those hits provoke.
    raw_on = [0.0] * len(defenders)
    if not attackers or not defenders:
        return 0.0, raw_on, 0.0, 0.0, []
    stunned = stun_fractions(defenders, attackers)
    focus_i = min(range(len(defenders)), key=lambda i: defenders[i].health)
    focus = defenders[focus_i]
    splash_radius = cambc.constants.passive_abilities.VARG_SPLASH_RADIUS
    hits_on = [0.0] * len(defenders)
    for attacker, stun in zip(attackers, stunned):
        up = 1.0 - stun
        if up <= 0:
            continue
        raw_on[focus_i] += (attacker.basic + attacker.single_dps) * up
        hits_on[focus_i] += (attacker.basic_hits + attacker.single_hits) * up
        for dps, radius, hits in attacker.areas:
            for i, defender in enumerate(defenders):
                if _in_radius(defender, focus, radius):
                    raw_on[i] += dps * up
                    hits_on[i] += hits * up
        if attacker.splash <= 0:
            continue
        for i, defender in enumerate(defenders):
            if i == focus_i:
                continue
            if _in_radius(defender, focus, splash_radius):
                raw_on[i] += attacker.splash * up
                hits_on[i] += attacker.basic_hits * up
    retal = 0.0
    for defender, hits in zip(defenders, hits_on):
        if defender.retaliation and hits:
            retal += hits * defender.retaliation
    return sum(raw_on), raw_on, retal, max(stunned), stunned


def body_ehp(profile, raw):
    # Durability is already a damage-taken multiplier. A consumed shield adds the raw it cancels.
    ehp = profile.health / profile.taken
    if profile.shield > 0 and raw > 0:
        ehp += min(profile.shield / profile.taken, raw)
    return ehp


def partner_scale(profile):
    # A full partner leaves effective health unchanged. A low partner shrinks it, because that death kills both.
    full = profile.max_health
    if full <= 0:
        return 1.0
    return min(1.0, max(0.0, profile.health / full))


def pool_ehp(profiles, raw_on):
    # Each body contributes its own effective health. Cassi and Issac stay separate, scaled by the partner.
    cassi = issac = None
    total = 0.0
    for profile, raw in zip(profiles, raw_on):
        ehp = body_ehp(profile, raw)
        if profile.name == Champ.CASSI:
            cassi = (profile, ehp)
        elif profile.name == Champ.ISSAC:
            issac = (profile, ehp)
        else:
            total += ehp
    if cassi is not None and issac is not None:
        total += cassi[1] * partner_scale(issac[0])
        total += issac[1] * partner_scale(cassi[0])
    elif cassi is not None:
        total += cassi[1]
    elif issac is not None:
        total += issac[1]
    return max(total, 1.0)


def side_heal(profiles, stunned):
    # Willow cannot heal herself, and a stun skips the cast. A lone body has nothing to restore.
    if len(profiles) < 2:
        return 0.0
    return sum(profile.heal * (1.0 - stun) for profile, stun in zip(profiles, stunned))


class Exchange:
    def __init__(self, kill_rate, die_rate, our_stun, their_stun, our_heal, their_heal, our_ret, their_ret):
        self.kill_rate = kill_rate
        self.die_rate = die_rate
        self.our_stun = our_stun
        self.their_stun = their_stun
        self.our_heal = our_heal
        self.their_heal = their_heal
        self.our_ret = our_ret
        self.their_ret = their_ret


def fight_exchange(ours, theirs):
    # strike already stunned each side. side_heal uses those fractions again.
    _, on_them, their_ret, our_stun, stunned_us = strike(ours, theirs)
    _, on_us, our_ret, their_stun, stunned_them = strike(theirs, ours)
    our_dps = sum(on_them) + our_ret
    their_dps = sum(on_us) + their_ret
    our_heal = side_heal(ours, stunned_us)
    their_heal = side_heal(theirs, stunned_them)
    kill_rate = max(0.0, our_dps - their_heal) / pool_ehp(theirs, on_them)
    die_rate = max(0.0, their_dps - our_heal) / pool_ehp(ours, on_us)
    return Exchange(kill_rate, die_rate, our_stun, their_stun, our_heal, their_heal, our_ret, their_ret)


def threat_zone(enemies, include_camps):
    zone = cambc.Bitboard()
    for enemy in enemies:
        try:
            zone.draw_chebyshev(enemy.position(), THREAT_RANGE)
        except cambc.Error:
            continue
    if include_camps:
        for camp in alive(visible_camps()):
            try:
                if camp.strength() > 0:
                    zone.draw_chebyshev(camp.position(), camp.attack_range())
            except cambc.Error:
                continue
    return zone


def threatened(position, include_camps=True):
    try:
        enemies = alive(visible_champions(Team.ENEMY))
        return threat_zone(enemies, include_camps).is_set(position)
    except cambc.Error:
        return False


def under_threat(me):
    # An enemy that can already hit us, or a damaged (awake) camp in its own range.
    position = me.position()
    for enemy in alive(visible_champions(Team.ENEMY)):
        try:
            if distance(position, enemy.position()) <= enemy.attack_range():
                return True
        except cambc.Error:
            continue
    for camp in alive(visible_camps()):
        try:
            if (camp.strength() > 0 and camp.health() < camp.max_health()
                    and distance(position, camp.position()) <= camp.attack_range()):
                return True
        except cambc.Error:
            continue
    return False


def enemy_closes_in(me, finishes_next_turn):
    # A basic move is one tile. An enemy already in range is hitting now.
    # One step out only matters when the stand outlasts next turn: a finish then
    # is gone by the time they arrive.
    origin = me.position()
    for enemy in alive(visible_champions(Team.ENEMY)):
        try:
            gap = distance(origin, enemy.position()) - enemy.attack_range()
        except cambc.Error:
            continue
        if gap <= 0 or (gap <= 1 and not finishes_next_turn):
            return enemy
    return None


def enemy_reaches_channel(me):
    try:
        finishes_next_turn = me.remaining_channel_time() <= 1
    except cambc.Error:
        finishes_next_turn = False
    return enemy_closes_in(me, finishes_next_turn)


def stop_channel(me):
    # Recall and tribute cancel. A teleport runs to the end; the engine rejects a cancel.
    try:
        if me.is_recalling():
            me.cancel_recall()
        elif me.is_submitting_gems():
            me.cancel_submit_gems()
        else:
            return False
    except (cambc.Error, ValueError):
        return False
    return True


def quantize_fight(score):
    # The sighting byte stores a non-negative raw fight score. A losing fight publishes nothing.
    if score <= 0:
        return 0
    return min(255, int(round(score)))


def read_board():
    # Farm index, the enemy sighting, then the capture claim. A score of 0, or a stamp older than
    # EVENT_TIMEOUT, is an empty sighting. The claim is kept either way.
    data = bytearray(struct.calcsize(SHARED_LAYOUT))
    cambc.shared_memory.read(0, data)
    farm_index, score, x, y, enemy, stamp, capture_slot, collector = struct.unpack(SHARED_LAYOUT, data)
    now = cambc.round_number()
    if score == 0 or (now - stamp) & 0xFF > EVENT_TIMEOUT:
        return farm_index, 0, 0, 0, NO_ENEMY, 0, capture_slot, collector
    return farm_index, score, x, y, enemy, stamp, capture_slot, collector


def buy(me, item, count):
    if count <= 0:
        return False
    try:
        if me.can_buy(item, count):
            me.buy(item, count)
            return True
    except cambc.Error:
        pass
    return False


def ward_spots():
    # Deterministic, so every champion agrees on the same spot list and can take one by draft order.
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


def dash_span(me):
    dash = ability_of(me, Kind.DASH)
    return int(dash.info().range) if dash is not None else 0


def dash_towards(me, target_position, reach):
    # Dash to the free tile closest to the target, if it leaves at most one walk to reach it.
    # Skip when the remaining gap is shorter than the dash: the landing would pass the useful tile.
    dash = ability_of(me, Kind.DASH)
    if dash is None or not dash.is_ready():
        return False
    origin = me.position()
    gap = distance(origin, target_position)
    if gap - reach < int(dash.info().range):
        return False
    free = tiles_around(origin, int(dash.info().range)) & ~blocked_tiles()
    for tile in sorted(free.positions(), key=lambda t: distance(t, target_position)):
        landing_gap = distance(tile, target_position)
        if landing_gap > reach + 1 or landing_gap >= gap:
            break
        if cast(dash, tile):
            return True
    return False


class Bot:
    def __init__(self):
        self.ready = False
        self.priority_set = False
        self.route = []
        self.draft_size = 5
        self.spawns = []
        # Visit order, as indexes into self.route. Draft memory is the same list on every champion.
        self.farm_slots = []
        self.aggressive_slots = []
        self.farm_index = 0
        self.agg_index = 0
        self.cores = ([], [])
        self.monoliths = set()
        # Most emblems seen on each ally, by champion; emblems are never lost.
        self.emblems = {}
        self.centers = []
        self.spawn_lists = []
        # Spawn tile to slot, for reading visible camps without a pass over every group.
        self.slot_at = {}
        # Walking steps between groups, cached per destination group since terrain never changes.
        self.goal_fields = {}
        self.gaps = {}
        # Per-slot value while farming (gold weighted above gems), then the original rating.
        self.values = ([], [])
        self.clears = []
        # Round each route group is next expected up; 0 means up or never seen.
        self.ready_at = []
        # Route groups last seen cleared with their reward still uncollected.
        self.rewards = set()
        self.action = FARM
        self.slot = 0
        self.peel = NO_PEEL
        # Last route or split decision printed, so a held stop does not log every round.
        self.route_note = None
        self.event_kind = 0
        self.event_intensity = 0
        self.event_xy = None
        self.event_stamp = 0
        self.ping_score = 0
        self.ping_xy = None
        self.ping_enemy = NO_ENEMY
        self.ping_stamp = 0
        self.peel_on = False
        # 0 means no altar published. A published altar is stored as index + 1.
        self.altar_code = 0
        self.altar_list = []
        self.capture_slot = NO_CAPTURE
        self.collector = 0
        self.claimed = 0
        # (route slot, first round we held a spawn tile) while a capture is ticking.
        self.capture_on = (None, 0)
        self.tributed = False
        self.vesper_done = False
        self.aggressive = False
        self.fight_score = None
        self.utilities = None
        # Score tail for this turn: totals, the adjustments that built them, and any gate that overrode them.
        self.score_line = None
        self.fight_why = ""
        self.farm_why = ""
        self.tribute_why = ""
        self.recall_why = ""
        self.leash_why = ""
        self.cohesion = (0, 0)
        # Set after a chase into their spawn fails, until this champion is past SPAWN_RETREAT_RANGE.
        self.spawn_bait = False
        # Set after a chase is leashed off the objective, until we are back inside LEASH_INNER.
        self.leash_bait = False
        # Last leash tile, so the walk back still knows the objective before this turn is scored.
        self.leash = None
        # Gold Vesper has spent toward the current VESPER_RECALL_GOLD batch.
        self.batch_spent = 0
        self.ward_spots = []
        # Index of the ward spot our own ward covers, or None while it is free to move.
        self.ward_slot = None
        # Set from spawn until we reach the shared target again; everyone starts on spawn.
        self.rejoining = True
        # Whether this champion settled farm slot A this round.
        self.led = False
        self.escort_wait = 0
        # Altars a heavy bag may visit alone (far from the centre, on our side) and the escorted rest.
        self.solo_altars = []
        self.escort_altars = []
        # Set for the rest of a turn after a channel is cancelled, so it is not started again.
        self.channel_cut = False
        # Kit is parsed once. Each score refreshes health, position, strength, focus, and items.
        self.profile_cache = {}
        self._camp_gen = -1
        self._camps_by_slot = {}
        self._camps_all = ()
        self._zone_gen = -1
        self._zone = None

    def setup(self):
        groups = game_map.camp_groups()
        data = bytes(cambc.DraftMemoryBuffer())
        _, farm, offset = draft.decode_pocket(data)
        _, aggressive, _ = draft.decode_pocket(data, offset)
        if not farm or any(index >= len(groups) for index in farm):
            _, farm = draft.compute_pocket()
            _, aggressive = draft.compute_pocket(aggressive=True)
        if any(index >= len(groups) for index in aggressive):
            aggressive = []
        if not farm:
            farm = [index for index, group in enumerate(groups) if group.camps()]

        # Farm order first, then aggressive stops that are not already on it. Indexes stay the visit order.
        indexes = []
        slot_of = {}
        for index in list(farm) + list(aggressive):
            if index in slot_of or index >= len(groups):
                continue
            slot_of[index] = len(indexes)
            indexes.append(index)
        self.farm_slots = [slot_of[index] for index in farm if index in slot_of]
        self.aggressive_slots = [slot_of[index] for index in aggressive if index in slot_of]
        self.cores = (list(self.farm_slots), list(self.aggressive_slots) or list(self.farm_slots))

        self.route = [groups[index] for index in indexes]
        self.spawns = [group.spawn_tiles() for group in self.route]
        self.spawn_lists = [spawns.positions() for spawns in self.spawns]
        self.slot_at = {tile: slot for slot, tiles in enumerate(self.spawn_lists) for tile in tiles}
        self.monoliths = {slot for slot, group in enumerate(self.route)
                          if any(camp.kind() == cambc.CampType.MONOLITH for camp in group.camps())}
        self.centers = [draft.group_center(group) for group in self.route]
        self.values = (
            [draft.group_value(group, draft.FARM_GOLD_WEIGHT) for group in self.route],
            [draft.group_value(group) for group in self.route],
        )
        self.clears = [draft.clear_rounds(group) for group in self.route]
        self.reward_gems = []
        self.reward_golds = []
        self.design_health = []
        self.design_strengths = []
        self.camp_tiles = []
        for group in self.route:
            health = 0
            tiles = []
            strengths = []
            for camp in group.camps():
                stats = draft.CAMP_STATS.get(camp.kind(), (0, 0))
                health += stats[0]
                tiles.append(camp.spawn_position())
                strengths.append(stats[1])
            self.reward_gems.append(group.gem_value())
            self.reward_golds.append(group.gold_value())
            self.design_health.append(health)
            self.design_strengths.append(strengths)
            self.camp_tiles.append(tiles)
        self.ready_at = [0] * len(self.route)
        self.altar_list = list(game_map.altars().positions())
        self.ward_spots = ward_spots()
        self.split_altars()
        try:
            self.draft_size = int(cambc.draft.team_size())
        except (cambc.Error, TypeError, ValueError):
            self.draft_size = 5
        # Walking steps from every tile to the enemy spawn, and the tiles close enough to it to stop chasing.
        theirs = game_map.spawn_region(Team.ENEMY)
        self.spawn_steps = cambc.DistanceField()
        self.spawn_steps.compute(game_map.walls() | game_map.holes(), theirs, cambc.Bitboard())
        self.spawn_zone = cambc.Bitboard()
        self.retreat_zone = cambc.Bitboard()
        for tile in theirs.positions():
            self.spawn_zone.draw_chebyshev(tile, SPAWN_CHASE_RANGE)
            self.retreat_zone.draw_chebyshev(tile, SPAWN_RETREAT_RANGE)
        self.ready = True

    def split_altars(self):
        # Solo altars sit away from the centre and no closer to the enemy spawn than to ours.
        middle = cambc.Vec2(game_map.width() // 2, game_map.height() // 2)
        ours = game_map.spawn_region(Team.ALLY).positions()
        theirs = game_map.spawn_region(Team.ENEMY).positions()
        for altar in game_map.altars().positions():
            home = min((distance(altar, t) for t in ours), default=0)
            away = min((distance(altar, t) for t in theirs), default=0)
            if distance(altar, middle) > CENTRAL_ALTAR_RANGE and home <= away:
                self.solo_altars.append(altar)
            else:
                self.escort_altars.append(altar)
        print(f"altars: {len(self.solo_altars)} solo, {len(self.escort_altars)} escorted")

    # Route memory

    def observe(self):
        # Vision is not shared, so each champion keeps its own respawn record.
        seen = cambc.vision.visible_tiles()
        now = cambc.round_number()
        for index, group in enumerate(self.route):
            if (seen & self.spawns[index]).is_empty():
                continue
            try:
                state = group.state()
            except cambc.Error:
                continue
            self.ready_at[index] = now + state.timer if isinstance(state, game_map.CampGroupRespawning) else 0
            if isinstance(state, game_map.CampGroupRewardAvailable):
                self.rewards.add(index)
            else:
                self.rewards.discard(index)

    def wait_at(self, slot, origin):
        # Rounds left to wait on arrival at a route group.
        travel = distance(origin, self.centers[slot])
        return max(0, self.ready_at[slot] - cambc.round_number() - travel)

    def update_emblems(self):
        # Item reads are costly and emblems change at the shop, so poll on spawn and every few rounds.
        now = cambc.round_number()
        if now % EMBLEM_POLL != 0:
            me = cambc.me()
            try:
                home = (not me.is_alive()) or game_map.spawn_region(Team.ALLY).is_set(me.position())
            except cambc.Error:
                home = False
            if not home:
                return
        for ally in cambc.champions(Team.ALLY):
            try:
                key = int(ally.champion_name())
            except cambc.Error:
                continue
            self.emblems[key] = max(self.emblems.get(key, 0), emblem_count(ally))

    def team_emblems(self):
        # Items are only readable in vision, so each champion keeps the most it has seen.
        # Vesper's done flag means she finished her core even if nobody else can see her bag.
        total = sum(self.emblems.values())
        if self.vesper_done:
            have = self.emblems.get(int(Champ.VESPER_VALE), 0)
            if have < VESPER_CORE_EMBLEMS:
                total += VESPER_CORE_EMBLEMS - have
        return total

    def allowed(self, slot):
        return slot not in self.monoliths or self.team_emblems() >= MONOLITH_EMBLEMS

    def can_take_camp(self, camp):
        slot = self.slot_at.get(camp.spawn_position())
        return slot is None or self.allowed(slot)

    def gap(self, start, goal):
        key = (start, goal)
        if key not in self.gaps:
            if goal not in self.goal_fields:
                self.goal_fields[goal] = walk_field(self.spawns[goal])
            self.gaps[key] = steps_to(self.goal_fields[goal], self.spawn_lists[start])
        return self.gaps[key]

    def mid_clear(self, slot):
        return any(camp.health() < camp.max_health() for camp in self.camps_at(slot))

    def visible_loot(self, skip):
        # Groups whose camps or leftover reward are in our own sight.
        loot = set(self.rewards)
        for camp in alive(visible_camps()):
            try:
                spawn = camp.spawn_position()
            except cambc.Error:
                continue
            slot = self.slot_at.get(spawn)
            if slot is not None:
                loot.add(slot)
        loot.discard(skip)
        now = cambc.round_number()
        return [s for s in loot if self.ready_at[s] <= now or s in self.rewards]

    def route_cycle(self, slot, steps):
        wait = max(0, self.ready_at[slot] - cambc.round_number() - steps)
        cycle = WALK_WEIGHT * steps + wait + self.clears[slot] + cambc.constants.CAPTURE_TIME
        return self.values[self.aggressive][slot], cycle

    def group_health(self, slot):
        # A dropped reward needs no clear. A down camp respawns at full design health.
        if slot in self.rewards:
            return 0.0
        camps = self.camps_at(slot)
        if camps:
            return sum(camp.health() for camp in camps)
        return self.design_health[slot]

    def camp_bodies(self, slot):
        # Living camps when we can see them, otherwise the design row in spawn-tile order.
        if slot in self.rewards:
            return []
        camps = self.camps_at(slot)
        if camps:
            return [(camp.strength(), camp.position()) for camp in camps]
        return list(zip(self.design_strengths[slot], self.camp_tiles[slot]))

    def camp_hits(self, slot, radius):
        # One cast hits every camp of the group that sits inside an area skill's radius.
        if radius is None:
            return 1
        bodies = self.camp_bodies(slot)
        if not bodies:
            tiles = self.camp_tiles[slot]
        else:
            tiles = [tile for _, tile in bodies]
        center = self.centers[slot]
        return max(1, sum(1 for tile in tiles if distance(tile, center) <= radius))

    def splash_extras(self, slot):
        # Varg splashes other camps within radius one of the camp he is hitting, not the primary.
        bodies = self.camp_bodies(slot)
        if len(bodies) <= 1:
            return 0
        center = self.centers[slot]
        primary = min(range(len(bodies)), key=lambda i: distance(bodies[i][1], center))
        origin = bodies[primary][1]
        radius = cambc.constants.passive_abilities.VARG_SPLASH_RADIUS
        return sum(1 for i, (_, tile) in enumerate(bodies)
                   if i != primary and distance(tile, origin) <= radius)

    def profiles_for(self, party):
        profiles = []
        for champion in party:
            key = _profile_key(champion)
            profile = self.profile_cache.get(key)
            if profile is None:
                profile = _blank_profile(champion)
                try:
                    focus = float(champion.focus())
                except (cambc.Error, ValueError):
                    focus = 0.0
                profile.spells = _parse_spells(champion, focus)
                self.profile_cache[key] = profile
            _refresh_profile(profile, champion)
            profiles.append(profile)
        return profiles

    def camp_trade(self, slot, profiles):
        # Damage the party deals, and the damage one tank takes, both per round.
        bodies = self.camp_bodies(slot)
        if not bodies or not profiles:
            return 0.0, 0.0
        extras = self.splash_extras(slot) if any(profile.splash for profile in profiles) else 0
        dps = 0.0
        for profile in profiles:
            mult = profile.camp_out
            dps += (profile.basic + profile.camp_single_dps) * mult
            for spell, radius in profile.camp_areas:
                dps += spell * self.camp_hits(slot, radius) * mult
            if profile.splash and extras:
                dps += profile.splash * extras * mult
        tanks = [profile for profile in profiles if profile.melee] or list(profiles)
        taken = sum(profile.camp_taken for profile in tanks) / len(tanks)
        attacking = sum(1 for strength, _ in bodies if strength > 0)
        if attacking:
            share = 1.0 / len(tanks)
            for profile in tanks:
                if profile.retaliation:
                    dps += share * attacking * profile.retaliation * profile.camp_out
        incoming = 0.0
        center = self.centers[slot]
        single = min(1.0, sum(profile.camp_stun_single for profile in profiles))
        hardest = max(range(len(bodies)), key=lambda i: bodies[i][0])
        for i, (strength, tile) in enumerate(bodies):
            if strength <= 0:
                continue
            stun = single if i == hardest else 0.0
            for profile in profiles:
                for radius, frac in profile.camp_stun_areas:
                    if distance(tile, center) <= radius:
                        stun += frac
            incoming += strength * (1.0 - min(1.0, stun))
        return dps, incoming * taken

    def travel_from(self, origin):
        walk = walk_field(tiles_around(origin, 0))
        travel = {}
        for slot in range(len(self.route)):
            steps = steps_to(walk, self.spawn_lists[slot])
            if steps is not None:
                travel[slot] = steps
        return travel

    def enemy_field(self):
        enemies = alive(visible_champions(Team.ENEMY))
        if not enemies:
            return None
        roots = cambc.Bitboard()
        for enemy in enemies:
            roots.set(enemy.position())
        return walk_field(roots)

    def _open(self, slot, reserved):
        if reserved != NO_CAPTURE and slot == reserved:
            return False
        return self.allowed(slot)

    def _quote(self, slot, profiles, enemy_walk):
        # Camp trade and reward, once per group. Path and arrival time are applied per leg.
        damage, incoming = self.camp_trade(slot, profiles)
        clear = self.group_health(slot) / max(1.0, damage)
        weight = draft.GOLD_WEIGHT if self.aggressive else draft.FARM_GOLD_WEIGHT
        gain = self.reward_gems[slot] + weight * self.reward_golds[slot]
        if slot in self.cores[self.aggressive]:
            gain *= POCKET_BONUS
        enemy = None if enemy_walk is None else steps_to(enemy_walk, self.spawn_lists[slot])
        return gain, clear, incoming * clear * ROUTE_DAMAGE_COST, enemy, self.ready_at[slot]

    def _apply(self, quote, steps, elapsed, gems, vesper_risk):
        gain, clear, risk, enemy, ready = quote
        wait = max(0, ready - cambc.round_number() - elapsed - steps)
        if enemy is not None and enemy <= elapsed + steps + clear:
            risk += gems
            if vesper_risk:
                risk += VESPER_ROUTE_RISK
        time = WALK_WEIGHT * steps + wait + clear + cambc.constants.CAPTURE_TIME
        return gain, risk, time

    def _as_route(self, gain, risk, time, first):
        return Route((gain - risk) / max(1.0, time), gain, risk, time, first)

    def score_route(self, origin, party, reserved, enemy_walk=None, only=None, lock_first=None, travel=None):
        # Best one- or two-stop route. Every open group is a first stop and a second stop;
        # the score drops a detour that costs more time or risk than it adds.
        if not party or not self.route:
            return None
        if travel is None:
            travel = self.travel_from(origin)
        profiles = self.profiles_for(party)
        gems = sum(gems_carried(ally) for ally in party)
        vesper_risk = any(ally.champion_name() == Champ.VESPER_VALE
                          and emblem_count(ally) < VESPER_CORE_EMBLEMS for ally in party)
        quotes = {}
        for slot in travel:
            if not self._open(slot, reserved):
                continue
            if only is not None and slot not in only and slot != lock_first:
                continue
            quotes[slot] = self._quote(slot, profiles, enemy_walk)
        if lock_first is not None:
            firsts = (lock_first,) if lock_first in quotes else ()
        else:
            # Near groups first, so a search that runs out of fuel has already priced the camps on the walk.
            firsts = sorted(quotes, key=lambda slot: travel[slot])
        best = None
        for slot in firsts:
            gain, risk, time = self._apply(quotes[slot], travel[slot], 0, gems, vesper_risk)
            option = self._as_route(gain, risk, time, slot)
            if cambc.fuel_remaining() >= ROUTE_FUEL_RESERVE:
                for other, quote in quotes.items():
                    if other == slot or (only is not None and other not in only):
                        continue
                    gap = self.gap(slot, other)
                    if gap is None:
                        continue
                    gain2, risk2, time2 = self._apply(quote, gap, time, gems, vesper_risk)
                    pair = self._as_route(gain + gain2, risk + risk2, time + time2, slot)
                    if pair.score > option.score:
                        option = pair
            if best is None or option.score > best.score:
                best = option
        return best

    def prefer_current(self, best, origin, party, reserved, enemy_walk, travel):
        # Finish a camp already under attack. Otherwise the higher score is the plan.
        current = self.slot % len(self.route)
        if current == reserved:
            return best
        if self.mid_clear(current):
            locked = self.score_route(
                origin, party, reserved, enemy_walk, travel=travel, lock_first=current)
            if locked is not None:
                return locked
            return Route(0.0, 0.0, 0.0, 1.0, current)
        return best

    def enemy_pressure(self, me):
        # In range now, or one step from it. Farming through that swing is how a route walks into a death.
        origin = me.position()
        for enemy in alive(visible_champions(Team.ENEMY)):
            try:
                gap = distance(origin, enemy.position()) - enemy.attack_range()
            except cambc.Error:
                continue
            if gap <= 1:
                return True
        return False

    def party_origin(self, party):
        xs = [ally.position().x for ally in party]
        ys = [ally.position().y for ally in party]
        return cambc.Vec2(sum(xs) // len(xs), sum(ys) // len(ys))

    def visible_party(self, me):
        party = [ally for ally in alive(visible_champions(Team.ALLY)) if not channelling(ally)]
        if me.is_alive() and not channelling(me) and me not in party:
            party.append(me)
        return party

    def log_plan(self, chosen, together_score, split_score, peel):
        if chosen is None:
            return
        if peel != NO_PEEL and split_score is not None and together_score is not None:
            key = ("split", chosen.first, peel)
            text = (f"split {num(split_score)} > together {num(together_score)} "
                    f"s{chosen.first}+s{peel}")
        else:
            key = ("route", chosen.first)
            text = (f"route {num(chosen.score)} t{num(chosen.time)} risk{num(chosen.risk)} "
                    f"-> s{chosen.first}")
        if key == self.route_note:
            return
        self.route_note = key
        print(f"round {cambc.round_number()}: {text}")

    def plan_routes(self, me):
        # One score for the whole team, and the same score again if someone is already off the blob.
        allies = self.visible_party(me)
        if not allies:
            self.peel = NO_PEEL
            return
        anchor = self.centers[self.slot % len(self.route)]
        near = [ally for ally in allies if distance(ally.position(), anchor) <= TEAM_RANGE]
        if near:
            side = [ally for ally in allies if ally not in near]
            blob = near
        else:
            side = []
            blob = allies
        enemy_walk = self.enemy_field()
        origin = self.party_origin(blob)
        travel = self.travel_from(origin)
        if not travel:
            origin = blob[0].position()
            travel = self.travel_from(origin)
        reserved = self.capture_slot
        together = self.score_route(origin, allies, reserved, enemy_walk, travel=travel)
        together = self.prefer_current(together, origin, allies, reserved, enemy_walk, travel)
        chosen = together
        peel = NO_PEEL
        split_score = None
        together_score = together.score if together is not None else None
        # A three-champion roster stays one party. Larger rosters may peel when the split scores higher.
        if side and together is not None and self.draft_size != 3:
            blob_route = self.score_route(origin, blob, reserved, enemy_walk, travel=travel)
            blob_route = self.prefer_current(blob_route, origin, blob, reserved, enemy_walk, travel)
            if blob_route is not None:
                side_route = self.score_route(
                    self.party_origin(side), side, blob_route.first, enemy_walk)
                if side_route is not None and side_route.first != blob_route.first:
                    split_score = ((blob_route.gain + side_route.gain
                                    - blob_route.risk - side_route.risk)
                                   / max(blob_route.time, side_route.time, 1.0))
                    if split_score >= together.score * SPLIT_MARGIN:
                        chosen = blob_route
                        peel = side_route.first
        if chosen is not None:
            self.slot = chosen.first
        self.peel = peel
        if self.peel == self.slot:
            self.peel = NO_PEEL
        self.log_plan(chosen, together_score, split_score, self.peel)

    def update_rejoining(self, me, slot):
        if not me.is_alive():
            return
        position = me.position()
        if game_map.spawn_region(Team.ALLY).is_set(position):
            self.rejoining = True
        elif self.rejoining and self.route and distance(position, self.centers[slot % len(self.route)]) <= TEAM_RANGE:
            self.rejoining = False

    def can_lead(self, me, stale):
        # Only a champion with the blob retargets slot A. A peeler, shopper or walker-back would
        # pick from its own tile and drag the main group. Tribute/fight pings do not use this.
        if not me.is_alive() or channelling(me) or me.is_teleporting():
            return False
        if self.peel != NO_PEEL and self.peel < len(self.route):
            peel = self.peel % len(self.route)
            blob = self.slot % len(self.route)
            if distance(me.position(), self.centers[peel]) < distance(me.position(), self.centers[blob]):
                return False
        return stale or not self.rejoining

    def share_loot(self, me):
        # A follower who sees a camp the leader cannot may replace the stop their own party is on.
        if (self.led or self.rejoining or self.event_kind != 0 or not self.route
                or not me.is_alive() or channelling(me) or under_threat(me)):
            return
        mine = self.my_farm_slot()
        if self.mid_clear(mine):
            return
        seen = set(self.visible_loot(self.capture_slot))
        if not seen:
            return
        near = [ally for ally in self.visible_party(me)
                if distance(ally.position(), me.position()) <= TEAM_RANGE]
        if me not in near:
            near.append(me)
        enemy_walk = self.enemy_field()
        travel = self.travel_from(me.position())
        best = self.score_route(me.position(), near, self.capture_slot, enemy_walk, only=seen, travel=travel)
        if best is None or best.first == mine:
            return
        stay = self.score_route(
            me.position(), near, self.capture_slot, enemy_walk, lock_first=mine, travel=travel)
        if stay is not None and best.score <= stay.score:
            return
        blob = self.slot % len(self.route)
        if mine == blob:
            self.slot = best.first
        else:
            self.peel = best.first
        if self.peel == self.slot:
            self.peel = NO_PEEL
        self.flush_board()
        print(f"round {cambc.round_number()}: champion {int(me.champion_name())} "
              f"spots loot at route slot {best.first}")

    def order_for(self, aggressive):
        order = self.aggressive_slots if aggressive and self.aggressive_slots else self.farm_slots
        return order or self.farm_slots

    def on_aggressive_route(self, me):
        # The whole team moves to the centre route once Vesper is online and everyone is alive.
        # Before that, one low-priority body can walk it while the farm stop is down.
        if self.vesper_done and team_alive() and self.aggressive_slots:
            return True
        return bool(self.peel_on and self.aggressive_slots and self.is_designated_peeler(me))

    def is_designated_peeler(self, me):
        if self.draft_size <= 3:
            return False
        try:
            mine = me.champion_name()
        except cambc.Error:
            return False
        eligible = []
        for ally in cambc.champions(Team.ALLY):
            try:
                if not ally.is_alive():
                    continue
                name = ally.champion_name()
            except cambc.Error:
                continue
            if self.capture_slot != NO_CAPTURE and self.collector == int(name) + 1:
                continue
            if (name == Champ.VESPER_VALE and not self.vesper_done
                    and emblem_count(ally) < VESPER_CORE_EMBLEMS):
                continue
            eligible.append(name)
        if not eligible:
            return False
        chosen = min(eligible, key=lambda name: (PRIORITY.get(name, 0), int(name)))
        return mine == chosen

    def my_farm_slot(self):
        # One shared farm stop. The index in byte 0 is the visit cursor; everyone farms that slot.
        if not self.route or not self.farm_slots:
            return 0
        return self.farm_slots[self.farm_index % len(self.farm_slots)]

    def stop_visible(self, slot):
        if slot >= len(self.spawns):
            return False
        return not (cambc.vision.visible_tiles() & self.spawns[slot]).is_empty()

    def stop_busy(self, slot):
        # Camps still up, a reward sitting there, or a capture already claimed on it.
        if self.camps_at(slot) or slot in self.rewards or self.capture_slot == slot:
            return True
        return False

    def next_up_index(self, order, index):
        # The next stop on this same list that is not a respawn we have already seen.
        now = cambc.round_number()
        count = len(order)
        for step in range(1, count):
            slot = order[(index + step) % count]
            if not self.allowed(slot):
                continue
            if self.ready_at[slot] <= now:
                return (index + step) % count
        return None

    def maybe_advance(self, aggressive):
        # Move the index only forward, and only when this body can see the current stop respawning
        # and waiting there costs more than walking to the next stop that is up.
        if aggressive and not self.aggressive_slots:
            return
        order = self.order_for(aggressive)
        if not order:
            return
        index = (self.agg_index if aggressive else self.farm_index) % len(order)
        slot = order[index]
        if not self.stop_visible(slot) or self.stop_busy(slot):
            return
        try:
            state = self.route[slot].state()
        except cambc.Error:
            return
        if not isinstance(state, game_map.CampGroupRespawning):
            return
        now = cambc.round_number()
        wait = max(0, self.ready_at[slot] - now)
        nxt = self.next_up_index(order, index)
        if nxt is None:
            return
        travel = self.gap(slot, order[nxt])
        if travel is None or wait <= travel:
            return
        if aggressive:
            self.agg_index = nxt
        else:
            self.farm_index = nxt
        print(f"round {now}: route {'agg' if aggressive else 'farm'} {index} -> {nxt}")

    def maybe_peel(self, me):
        # A body that cannot see the farm stop leaves the peel bit alone.
        if not self.farm_slots or not self.aggressive_slots or self.draft_size <= 3:
            return
        slot = self.farm_slots[self.farm_index % len(self.farm_slots)]
        if not self.stop_visible(slot):
            return
        if self.stop_busy(slot):
            self.peel_on = False
            return
        if self.ready_at[slot] > cambc.round_number():
            self.peel_on = True

    def enemy_attack_zone(self):
        _touch_sight()
        if self._zone is not None and self._zone_gen == _sight.gen:
            return self._zone
        zone = cambc.Bitboard()
        for enemy in alive(visible_champions(Team.ENEMY)):
            try:
                zone.draw_chebyshev(enemy.position(), enemy.attack_range())
            except cambc.Error:
                continue
        self._zone = zone
        self._zone_gen = _sight.gen
        return zone

    def published_altar(self):
        if self.altar_code <= 0 or self.altar_code > len(self.altar_list):
            return None
        return self.altar_list[self.altar_code - 1]

    def refresh_altar(self, me):
        # A heavy bag publishes its altar. Someone who can see that altar, and no bag still on it, clears it.
        altar = self.published_altar()
        if altar is None or not cambc.vision.visible_tiles().is_set(altar):
            return
        if me.gem_count() >= ESCORT_GEMS:
            return
        for ally in alive(visible_champions(Team.ALLY)):
            submitting = False
            try:
                submitting = ally.is_submitting_gems()
            except cambc.Error:
                submitting = False
            if gems_carried(ally) >= ESCORT_GEMS or submitting:
                return
        self.altar_code = 0

    def flush_board(self):
        if self.ping_xy is None or self.ping_score <= 0:
            score, x, y, enemy, stamp = 0, 0, 0, NO_ENEMY, 0
        else:
            score = self.ping_score & 0xFF
            x = self.ping_xy.x & 0xFF
            y = self.ping_xy.y & 0xFF
            enemy = self.ping_enemy & 0xFF
            stamp = self.ping_stamp & 0xFF
        capture_slot = self.capture_slot if self.collector else NO_CAPTURE
        cambc.shared_memory.write(0, struct.pack(
            SHARED_LAYOUT,
            self.farm_index & 0xFF,
            score, x, y, enemy, stamp, capture_slot & 0xFF, self.collector & 0xFF))

    def update_capture(self, me, capture_slot, collector, claimed):
        # Returns the capture claim after dropping finished or stale ones and claiming a fresh reward.
        now = cambc.round_number()
        if capture_slot != NO_CAPTURE and (capture_slot >= len(self.route) or self.ready_at[capture_slot] > now
                                           or (now - claimed) & 0xFF > CAPTURE_TIMEOUT):
            capture_slot = NO_CAPTURE
        if capture_slot != NO_CAPTURE or not self.rewards:
            return capture_slot, collector, claimed

        slot = min(self.rewards, key=lambda s: distance(me.position(), self.centers[s]))
        # Only a fresh sighting can claim this group again, so a stale record can't re-send a collector.
        self.rewards.discard(slot)
        spawns = self.spawns[slot].positions()
        allies = [a for a in alive(visible_champions(Team.ALLY)) if not channelling(a)]
        if me not in allies:
            allies.append(me)
        ranked = sorted(allies, key=lambda ally: self.claim_rank(ally, spawns), reverse=True)
        chosen = ranked[0]
        margin = self.claim_rank(chosen, spawns)
        if len(ranked) > 1:
            margin -= self.claim_rank(ranked[1], spawns)
        print(f"round {now}: claim c{int(chosen.champion_name())} {signed(margin)} s{slot}")
        return slot, int(chosen.champion_name()) + 1, now

    def claim_rank(self, ally, spawns):
        # Distance loses to a building Vesper. After her core, the poorest ally is preferred.
        score = -float(min(distance(ally.position(), tile) for tile in spawns))
        if ally.champion_name() == Champ.VESPER_VALE and emblem_count(ally) < VESPER_CORE_EMBLEMS:
            return score + VESPER_CLAIM_BONUS
        return score - item_worth(ally) / EMBLEM_WORTH

    # Shared decision

    def seen_vesper_core(self, me):
        # Vesper knows her own emblems. Anyone else only knows what they have actually seen.
        try:
            if me.champion_name() == Champ.VESPER_VALE and emblem_count(me) >= VESPER_CORE_EMBLEMS:
                return True
        except cambc.Error:
            pass
        return self.emblems.get(int(Champ.VESPER_VALE), 0) >= VESPER_CORE_EMBLEMS

    def sync_board(self, me):
        farm_index, score, x, y, enemy, stamp, capture_slot, collector = read_board()
        self.farm_index = farm_index
        self.ping_score = score
        self.ping_stamp = stamp
        self.ping_enemy = enemy
        self.ping_xy = cambc.Vec2(x, y) if score else None
        self.note_capture(capture_slot, collector)
        self.vesper_done = self.seen_vesper_core(me)
        if not self.route:
            self.flush_board()
            return
        if self.claim_stale():
            self.clear_capture()
        # Whoever can see the current stop finished moves the shared farm index forward.
        # A claimed reward does not hold the cursor: the collector stays, everyone else moves on.
        self.maybe_advance(False)
        self.release_farm_stop()
        self.slot = self.my_farm_slot()
        self.flush_board()

    def note_capture(self, slot, collector):
        # Remember when this body first saw a claim, so a dead collector is replaced after the timeout.
        if collector == 0 or slot == NO_CAPTURE:
            self.capture_slot = NO_CAPTURE
            self.collector = 0
            self.claimed = 0
            return
        if slot != self.capture_slot or collector != self.collector:
            self.claimed = cambc.round_number()
        self.capture_slot = slot
        self.collector = collector

    def clear_capture(self):
        self.capture_slot = NO_CAPTURE
        self.collector = 0
        self.claimed = 0

    def collector_alive(self):
        found = False
        for ally in cambc.champions(Team.ALLY):
            try:
                if int(ally.champion_name()) + 1 != self.collector:
                    continue
                found = True
                if ally.is_alive():
                    return True
            except cambc.Error:
                continue
        # A missed read keeps the claim. A body we could see and that is dead does not.
        return not found

    def claim_stale(self):
        slot = self.capture_slot
        if slot == NO_CAPTURE or self.collector == 0:
            return False
        now = cambc.round_number()
        if slot >= len(self.route) or (now - self.claimed) & 0xFF > CAPTURE_TIMEOUT:
            return True
        if not self.collector_alive():
            return True
        # Out of vision the reward may still be sitting there. Only a fresh look can retire it.
        if not self.stop_visible(slot):
            return False
        if self.ready_at[slot] > now or slot not in self.rewards:
            return True
        return False

    def vesper_building(self, ally):
        # Finished once anyone has seen her core, or her bag reads that many emblems now.
        try:
            if ally.champion_name() != Champ.VESPER_VALE:
                return False
        except cambc.Error:
            return False
        if self.vesper_done:
            return False
        count = max(emblem_count(ally), self.emblems.get(int(Champ.VESPER_VALE), 0))
        return count < VESPER_CORE_EMBLEMS

    def visible_building_vesper(self, me):
        allies = alive(visible_champions(Team.ALLY))
        if me not in allies:
            allies.append(me)
        for ally in allies:
            if self.vesper_building(ally):
                return ally
        return None

    def pick_collector(self, me, slot):
        # A building Vesper who can arrive before the claim expires collects. Otherwise the poorest ally.
        spawns = self.spawn_lists[slot]
        allies = [a for a in alive(visible_champions(Team.ALLY)) if not channelling(a)]
        if me not in allies:
            allies.append(me)
        builders = []
        for ally in allies:
            if not self.vesper_building(ally):
                continue
            try:
                gap = min(distance(ally.position(), tile) for tile in spawns)
            except cambc.Error:
                continue
            if gap <= CAPTURE_TIMEOUT:
                builders.append(ally)
        pool = builders or allies

        def rank(ally):
            try:
                gap = min(distance(ally.position(), tile) for tile in spawns)
            except cambc.Error:
                gap = 999
            return (item_worth(ally), gap)

        return min(pool, key=rank)

    def write_claim(self, ally, slot):
        try:
            collector = int(ally.champion_name()) + 1
        except cambc.Error:
            return
        if self.capture_slot == slot and self.collector == collector:
            return
        now = cambc.round_number()
        self.capture_slot = slot
        self.collector = collector
        self.claimed = now
        self.release_farm_stop()
        print(f"round {now}: c{collector - 1} collects s{slot}")

    def next_farm_index(self, order, index):
        # The next stop that is up, or the next stop on the route if every other one is still respawning.
        count = len(order)
        fallback = None
        now = cambc.round_number()
        for step in range(1, count):
            nxt = (index + step) % count
            slot = order[nxt]
            if not self.allowed(slot) or slot == self.capture_slot:
                continue
            if fallback is None:
                fallback = nxt
            if self.ready_at[slot] <= now:
                return nxt
        return fallback

    def release_farm_stop(self):
        # Point the shared cursor past a claimed reward so the rest of the team farms the next camp.
        if self.capture_slot == NO_CAPTURE or not self.farm_slots:
            return
        order = self.farm_slots
        index = self.farm_index % len(order)
        if order[index] != self.capture_slot:
            return
        nxt = self.next_farm_index(order, index)
        if nxt is None:
            return
        self.farm_index = nxt
        print(f"round {cambc.round_number()}: farm {index} -> {nxt}, c{self.collector - 1} collects")

    def propose_capture(self, me):
        # One shared collector. A building Vesper within reach takes a claim someone else started.
        if self.capture_slot == NO_CAPTURE:
            slot = self.safe_capture_slot()
            if slot is not None:
                self.write_claim(self.pick_collector(me, slot), slot)
            return
        # She takes a nearby claim the assigner gave to someone else. Anyone who can see her does the same.
        self.vesper_claim(me)
        if self.collector == int(Champ.VESPER_VALE) + 1 or self.enemy_near_group(self.capture_slot):
            return
        vesper = self.visible_building_vesper(me)
        if vesper is not None and self.vesper_should_take(vesper, self.capture_slot):
            self.write_claim(vesper, self.capture_slot)

    def vesper_should_take(self, vesper, slot):
        # Hand her the camp when she can arrive, unless someone closer is already collecting it.
        if slot >= len(self.centers) or under_threat(vesper):
            return False
        try:
            gap = distance(vesper.position(), self.centers[slot])
        except cambc.Error:
            return False
        if gap > CAPTURE_TIMEOUT:
            return False
        holder = None
        for ally in alive(visible_champions(Team.ALLY)):
            try:
                if int(ally.champion_name()) + 1 == self.collector:
                    holder = ally
                    break
            except cambc.Error:
                continue
        if holder is None:
            return gap <= VESPER_CLAIM_RANGE
        try:
            return gap < distance(holder.position(), self.centers[slot])
        except cambc.Error:
            return False

    def is_collector(self, me):
        return self.capture_slot != NO_CAPTURE and self.collector == int(me.champion_name()) + 1

    def vesper_claim(self, me):
        # She publishes the claim herself when the body who started it could not see her.
        if not self.vesper_building(me) or self.is_collector(me) or under_threat(me) or not self.route:
            return
        position = me.position()
        slot = self.capture_slot
        if slot == NO_CAPTURE or slot >= len(self.route):
            slot = min(self.rewards, key=lambda s: distance(position, self.centers[s]), default=NO_CAPTURE)
        if slot == NO_CAPTURE or slot >= len(self.centers):
            return
        if distance(position, self.centers[slot]) > VESPER_CLAIM_RANGE or self.enemy_near_group(slot):
            return
        self.write_claim(me, slot)

    def enemy_near_group(self, slot):
        # True when a visible enemy is inside CAPTURE_SAFE_RANGE of any camp in the group.
        tiles = self.spawn_lists[slot]
        for enemy in alive(visible_champions(Team.ENEMY)):
            try:
                place = enemy.position()
            except cambc.Error:
                continue
            if any(distance(place, tile) <= CAPTURE_SAFE_RANGE for tile in tiles):
                return True
        return False

    def safe_capture_slot(self):
        # A cleared group we can see. The farm stop we just finished wins; otherwise the closest one.
        if not self.route or not self.rewards:
            return None
        now = cambc.round_number()
        farm = self.my_farm_slot()
        found = []
        for slot in self.rewards:
            if slot >= len(self.route) or not self.stop_visible(slot):
                continue
            if self.camps_at(slot) or self.ready_at[slot] > now or self.enemy_near_group(slot):
                continue
            found.append(slot)
        if not found:
            return None
        if farm in found:
            return farm
        origin = self.centers[farm]
        return min(found, key=lambda slot: (distance(origin, self.centers[slot]), slot))

    def capture_rounds_left(self, me, index):
        # Captures tick at round start, so standing on the tile now means this round already counted.
        # An enemy on the region, or stepping off, resets it.
        spawns = self.spawns[index]
        occupied = spawns.is_set(me.position())
        contested = any(spawns.is_set(enemy.position())
                        for enemy in alive(visible_champions(Team.ENEMY)))
        now = cambc.round_number()
        if not occupied or contested:
            self.capture_on = (None, 0)
            return cambc.constants.CAPTURE_TIME
        slot, since = self.capture_on
        if slot != index:
            self.capture_on = (index, now)
            since = now
        return max(0, cambc.constants.CAPTURE_TIME - (now - since + 1))

    def collect(self, me):
        # Stand on the cleared group until its reward lands. The 8-tile check already passed this turn.
        index = self.capture_slot
        if index >= len(self.route):
            return False
        if self.ready_at[index] > cambc.round_number():
            self.capture_slot, self.collector = NO_CAPTURE, 0
            self.flush_board()
            return False
        if self.camps_at(index):
            return False
        center = self.centers[index]
        self.log_turn(me, f"collect s{index}{at(center)}")
        move_towards_closest(self.spawns[index])
        return True

    def score_fight(self, leash):
        # Damage versus effective health, then a leash so a runner past the objective is not worth it.
        enemies = alive(visible_champions(Team.ENEMY))
        self.fight_why = ""
        self.losing_fight = False
        self.winning_fight = False
        if not enemies:
            self.fight_score = (0.0, FARM)
            return 0.0
        allies = alive(visible_champions(Team.ALLY))
        parts = [] if TRACE else None
        score = 0.0
        if parts is not None:
            parts.append(f"{len(allies)}v{len(enemies)}")
        if len(allies) > len(enemies):
            score += OUTNUMBER_BONUS
            if parts is not None:
                parts.append(f"out{signed(OUTNUMBER_BONUS)}")
        else:
            missing = MISSING_BODY_PENALTY * (len(enemies) - len(allies))
            score -= missing
            if parts is not None and missing:
                parts.append(f"miss{signed(-missing)}")
        trade = fight_exchange(self.profiles_for(allies), self.profiles_for(enemies))
        edge = EXCHANGE_WEIGHT * (trade.kill_rate - trade.die_rate)
        self.losing_fight = trade.die_rate > trade.kill_rate
        self.winning_fight = trade.kill_rate > trade.die_rate
        score += edge
        if parts is not None:
            parts.append(
                f"ex{signed(edge)} k{rate(trade.kill_rate)}/{rate(trade.die_rate)} "
                f"stun{rate(trade.our_stun)}/{rate(trade.their_stun)} "
                f"heal{num(trade.our_heal)}/{num(trade.their_heal)} "
                f"ret{num(trade.our_ret)}/{num(trade.their_ret)}")
        if leash is not None:
            gaps = []
            for enemy in enemies:
                try:
                    gaps.append(distance(enemy.position(), leash))
                except cambc.Error:
                    continue
            nearest = min(gaps) if gaps else None
            if nearest is None:
                nearest = 0
            dist = DISTANCE_PENALTY * nearest
            leash_pen = LEASH_WEIGHT * max(0, nearest - LEASH_INNER)
            score -= dist + leash_pen
            if parts is not None:
                parts.append(f"near{nearest}")
                if dist:
                    parts.append(f"dist{signed(-dist)}")
                if leash_pen:
                    parts.append(f"leash{signed(-leash_pen)}")
        if self.squishy_overextended(allies, enemies):
            score -= OVEREXTEND_PENALTY
            if parts is not None:
                parts.append(f"front{signed(-OVEREXTEND_PENALTY)}")
        try:
            at_spawn = game_map.spawn_region(Team.ALLY).is_set(cambc.me().position())
        except cambc.Error:
            at_spawn = False
        if at_spawn:
            score += SPAWN_BONUS
            if parts is not None:
                parts.append(f"spawn{signed(SPAWN_BONUS)}")
        if not self.vesper_done:
            score -= BUILD_CAUTION
            if parts is not None:
                parts.append(f"build{signed(-BUILD_CAUTION)}")
        else:
            carry = next((ally for ally in allies if ally.champion_name() == Champ.VESPER_VALE), None)
            me = cambc.me()
            if carry is None and me.is_alive() and me.champion_name() == Champ.VESPER_VALE:
                carry = me
            if carry is not None and leash is not None and distance(carry.position(), leash) <= TEAM_RANGE:
                score += CARRY_HERE
                if parts is not None:
                    parts.append(f"carry{signed(CARRY_HERE)}")
            else:
                score -= CARRY_AWAY
                if parts is not None:
                    parts.append(f"away{signed(-CARRY_AWAY)}")
        if parts is not None:
            self.fight_why = " ".join(parts)
        choice = FIGHT if score >= FIGHT_THRESHOLD else RETREAT
        self.fight_score = (score, choice)
        return score

    def steps_at(self, slot, origin):
        # Walking distance is symmetric, so the spawn-rooted field answers one get.
        field = self.goal_fields.get(slot)
        if field is None:
            field = walk_field(self.spawns[slot])
            self.goal_fields[slot] = field
        return steps_to(field, [origin])

    def score_farm(self, me):
        self.farm_why = ""
        if not self.route:
            return 0.0
        slot = self.my_farm_slot()
        try:
            steps = self.steps_at(slot, me.position())
        except cambc.Error:
            if TRACE:
                self.farm_why = "blocked"
            return 0.0
        if steps is None:
            if TRACE:
                self.farm_why = "blocked"
            return 0.0
        value, cycle = self.route_cycle(slot, steps)
        if TRACE:
            self.farm_why = f"v{num(value)}/{cycle}"
        return FARM_SCALE * value / max(1, cycle)

    def score_tribute(self, me):
        self.tribute_why = ""
        gems = me.gem_count()
        building = me.champion_name() == Champ.VESPER_VALE and emblem_count(me) < VESPER_CORE_EMBLEMS
        if building:
            if TRACE:
                self.tribute_why = "build"
            return -1.0
        if gems <= 0:
            return -1.0
        idle = bool(self.route) and self.wait_at(self.my_farm_slot(), me.position()) > 0
        if gems < IDLE_BANK and not idle:
            if TRACE:
                self.tribute_why = "low"
            return -1.0
        parts = [] if TRACE else None
        gem_part = gems / GEM_SCALE
        bag = GEM_PRESSURE * gems
        base = gem_part + bag
        if parts is not None:
            parts.append(f"gem{signed(gem_part)}")
            parts.append(f"bag{signed(bag)}")
        if idle and gems >= IDLE_BANK:
            base += IDLE_TRIBUTE_BONUS
            if parts is not None:
                parts.append(f"idle{signed(IDLE_TRIBUTE_BONUS)}")
        choices = self.legal_altars(me)
        nearest, steps = self.nearest_altar(me.position(), choices)
        if nearest is None:
            if TRACE:
                self.tribute_why = "altar"
            return -1.0
        walk_pen = TRIBUTE_DIST * (steps or 20) / 10
        score = base - walk_pen
        if not self.vesper_done and me.champion_name() != Champ.VESPER_VALE:
            score -= BUILD_TRIBUTE
        if parts is not None:
            walked = steps if steps is not None else "?"
            parts.append(f"walk{walked}{signed(-walk_pen)}")
            if not self.vesper_done and me.champion_name() != Champ.VESPER_VALE:
                parts.append(f"build{signed(-BUILD_TRIBUTE)}")
            self.tribute_why = " ".join(parts)
        return score

    def score_recall(self, me):
        self.recall_why = ""
        if me.health() < me.max_health() * RECALL_HEALTH:
            if TRACE:
                self.recall_why = "panic"
            return PANIC_RECALL_SCORE
        vesper = me.champion_name() == Champ.VESPER_VALE
        ready = vesper or (self.tributed and me.gem_count() == 0)
        if not ready:
            return -1.0
        need = self.vesper_recall_gold(me) if vesper else RECALL_GOLD
        if me.gold_count() >= need:
            if TRACE:
                self.recall_why = "shop"
            return SHOP_RECALL_SCORE
        if TRACE:
            self.recall_why = f"g{me.gold_count()}/{need}"
        return -1.0

    def leash_tile(self, me, tribute_score, farm_score):
        self.leash_why = ""
        if tribute_score > farm_score:
            nearest, _ = self.nearest_altar(me.position(), self.legal_altars(me))
            if nearest is not None:
                self.leash_why = "leash altar"
                return nearest
        if self.ping_xy is not None:
            self.leash_why = "leash ping"
            return self.ping_xy
        if self.route:
            return self.centers[self.my_farm_slot()]
        return me.position()

    def cohesion_ok(self, me):
        living = sum(1 for a in cambc.champions(Team.ALLY) if a.is_alive())
        need = max(2, living - COHESION_ABSENT)
        allies = alive(visible_champions(Team.ALLY))
        rally = self.event_xy
        if rally is not None:
            near = sum(1 for a in allies if distance(a.position(), rally) <= TEAM_RANGE)
        else:
            near = len(allies)
        want = min(need, living)
        self.cohesion = (near, want)
        return near >= want

    def squishy_overextended(self, allies, enemies):
        # Melee allies are the tanks. A squishy with none of them closer to the nearest enemy is in front.
        tanks = [ally for ally in allies if is_melee(ally)]
        for squishy in allies:
            try:
                if squishy.max_health() > SQUISHY_HEALTH:
                    continue
                nearest = min(enemies, key=lambda enemy: distance(squishy.position(), enemy.position()))
                gap = distance(squishy.position(), nearest.position())
                covered = any(distance(tank.position(), nearest.position()) < gap for tank in tanks)
            except (cambc.Error, ValueError):
                continue
            if not covered:
                return True
        return False

    def hold_behind_tanks(self, me, enemies):
        if me.max_health() > SQUISHY_HEALTH or not enemies:
            return False
        tanks = [a for a in alive(visible_champions(Team.ALLY)) if a != me and is_melee(a)]
        if not tanks:
            return False
        try:
            nearest = min(enemies, key=lambda enemy: distance(me.position(), enemy.position()))
            tank = min(tanks, key=lambda ally: distance(ally.position(), nearest.position()))
            if distance(me.position(), nearest.position()) <= distance(tank.position(), nearest.position()):
                move_towards_closest(tiles_around(tank.position(), 1))
                return True
        except (cambc.Error, ValueError):
            return False
        return False

    def allies_around(self, me):
        count = 0
        for ally in alive(visible_champions(Team.ALLY)):
            if ally == me:
                continue
            try:
                if distance(me.position(), ally.position()) <= CONTEST_RANGE:
                    count += 1
            except cambc.Error:
                continue
        return count

    def legal_altars(self, me):
        # Safe altars are always legal. A contested altar needs two other allies already here.
        choices = list(self.solo_altars)
        if self.allies_around(me) >= CONTEST_ALLIES:
            choices.extend(self.escort_altars)
        return choices

    def nearest_visible_enemy(self, me):
        best = None
        best_gap = None
        for enemy in alive(visible_champions(Team.ENEMY)):
            try:
                gap = distance(me.position(), enemy.position())
            except cambc.Error:
                continue
            if best is None or gap < best_gap:
                best = enemy
                best_gap = gap
        return best

    def publish_sighting(self, me, raw_fight):
        # Write this body's raw fight score only when it beats the sighting already on the board.
        quantized = quantize_fight(raw_fight)
        if quantized <= self.ping_score:
            return
        enemy = self.nearest_visible_enemy(me)
        if enemy is None:
            return
        try:
            name = int(enemy.champion_name())
            place = enemy.position()
        except cambc.Error:
            return
        self.ping_score = quantized
        self.ping_xy = place
        self.ping_enemy = name & 0xFF
        self.ping_stamp = cambc.round_number() & 0xFF

    def apply_sighting(self, me, raw_fight, board_score, board_xy):
        # A stronger ally sighting raises this fight score. The writer does not add their own ping.
        if board_xy is None or board_score <= quantize_fight(raw_fight):
            return raw_fight
        walk = DISTANCE_PENALTY * distance(me.position(), board_xy)
        return raw_fight + board_score - walk

    def choose_intent(self, me):
        # The claim is settled first, so farm and fight scores use the stop the rest of the team is on.
        self.propose_capture(me)
        slot = self.my_farm_slot()
        self.slot = slot
        self.leash = self.centers[slot] if self.route else me.position()
        enemies = alive(visible_champions(Team.ENEMY))
        if enemies:
            raw_fight = self.score_fight(self.leash)
        else:
            raw_fight = 0.0
            self.fight_why = ""
            self.losing_fight = False
            self.winning_fight = False
        board_score = self.ping_score
        board_xy = self.ping_xy
        self.publish_sighting(me, raw_fight)
        fight = self.apply_sighting(me, raw_fight, board_score, board_xy)
        tribute = self.score_tribute(me)
        recall = self.score_recall(me)
        farm = self.score_farm(me)
        notes = []
        if fight != raw_fight:
            notes.append(f"ping{signed(fight - raw_fight)}")
        options = [(farm, FARM), (tribute, TRIBUTE), (recall, RECALL)]
        if fight >= FIGHT_THRESHOLD:
            options.append((fight, FIGHT))
        _, self.action = max(options, key=lambda item: item[0])
        # Only the claimed collector stands on the reward. Fight and retreat still win for that body.
        # Everyone else keeps the farm, tribute, or recall score and walks to the next stop.
        if self.capture_slot != NO_CAPTURE and not self.is_collector(me):
            notes.append(f"cap c{self.collector - 1} s{self.capture_slot}")
        elif self.is_collector(me) and self.action not in (FIGHT, RETREAT):
            if fight >= FIGHT_THRESHOLD:
                self.action = FIGHT
            elif self.capture_slot < len(self.route) and self.enemy_near_group(self.capture_slot):
                self.clear_capture()
                notes.append("cap drop")
            else:
                self.action = CAPTURE
                notes.append(f"cap s{self.capture_slot}")
        if TRACE:
            self.score_line = self.format_scores(fight, farm, tribute, recall, notes, None)
        self.flush_board()
        return self.action

    def publish_altar(self, me):
        if me.gem_count() < ESCORT_GEMS or not self.altar_list:
            return
        altar = self.tribute_altar(me)
        if altar is None:
            return
        for index, tile in enumerate(self.altar_list):
            if tile.x == altar.x and tile.y == altar.y:
                self.altar_code = index + 1
                return

    def pick_action(self, me):
        return self.choose_intent(me)

    def format_scores(self, fight, farm, tribute, recall, notes, margin):
        def why(text):
            return f"[{text}]" if text else ""

        line = (f"f{num(fight)}{why(self.fight_why)} "
                f"p{num(farm)}{why(self.farm_why)} "
                f"t{num(tribute)}{why(self.tribute_why)} "
                f"r{num(recall)}{why(self.recall_why)}"
                f"/{FIGHT_THRESHOLD:g}")
        if notes:
            line += " " + " ".join(notes)
        line += f" -> {ACTION_NAMES[self.action]}"
        if margin is not None:
            line += f" by {num(margin)}"
        return line

    def note(self, text):
        # A gate after the score (leash, spawn chase, low-health bag) appends why the body did not follow it.
        if not TRACE:
            return
        if self.score_line:
            self.score_line += f" {text}"
        else:
            self.score_line = text

    def maybe_ping(self, me, fight, tribute, farm):
        # Contact is published by choose_intent. A fight score no longer freezes the route.
        return

    def log_turn(self, me, action):
        # One line: this body's move, the shared plan, then the utilities and the adjustments that moved them.
        if not TRACE:
            return
        who = int(me.champion_name())
        where = "dead" if not me.is_alive() else at(me.position())
        team = ACTION_NAMES[self.action]
        if self.route and self.farm_slots:
            index = self.farm_index % len(self.farm_slots)
            slot = self.farm_slots[index]
            team += f" farm[{index}] s{slot}{at(self.centers[slot])}"
        if self.capture_slot != NO_CAPTURE and self.collector:
            team += f" cap s{self.capture_slot} c{self.collector - 1}"
        if self.ping_xy is not None and self.ping_score:
            team += f" ping {self.ping_score}{at(self.ping_xy)} e{self.ping_enemy}"
        line = f"r{cambc.round_number()} c{who} {where} {action} | {team}"
        if self.score_line:
            line += f" | {self.score_line}"
        print(line)

    # Turn

    def play_turn(self):
        me = cambc.me()
        self.fight_score = None
        self.utilities = None
        self.score_line = None
        self.channel_cut = False
        if not self.ready:
            self.setup()
        if not self.priority_set:
            me.set_priority(PRIORITY.get(me.champion_name(), 0))
            self.priority_set = True
        if me.is_alive():
            self.observe()
        self.update_emblems()
        self.sync_board(me)

        # Dead champions still get a turn to shop.
        if not me.is_alive():
            self.buy_items(me)
            self.log_turn(me, "shop")
            return

        # Stay on a channel an enemy cannot hit. One step out is safe when it ends next turn.
        if me.is_recalling() or me.is_submitting_gems() or me.is_teleporting():
            if me.is_recalling():
                action = "recall"
            elif me.is_submitting_gems():
                action = "tribute"
            else:
                action = "teleport"
            threat = None if action == "teleport" else enemy_reaches_channel(me)
            if threat is None or not stop_channel(me):
                self.log_turn(me, action)
                return
            self.channel_cut = True
            print(f"round {cambc.round_number()}: champion {int(me.champion_name())} "
                  f"aborts {action}, {unit_at(threat)} steps into range")

        # Placing a ward costs no move, attack or cast, so it is tried every turn.
        self.place_ward(me)

        # On spawn: buy, stay until healed (spawn heals at turn end), then head out.
        position = me.position()
        if game_map.spawn_region(Team.ALLY).is_set(position):
            self.tributed = False
            self.spawn_bait = False
            self.leash_bait = False
            self.buy_items(me)
            if me.health() < me.max_health():
                self.log_turn(me, "heal")
                return
            if self.route:
                self.log_turn(me, "walk out")
                move_towards_closest(self.spawns[self.my_farm_slot()])
            else:
                self.log_turn(me, "spawn")
            return

        if me.gem_count() > 0:
            self.tributed = False

        if self.leaving_spawn(me) or self.leaving_leash(me):
            return

        self.choose_intent(me)
        gems = me.gem_count()

        if self.action == CAPTURE:
            # Stay on a capture already in progress. On the walk in, step out of an attack range first.
            on_camp = (self.capture_slot < len(self.spawns)
                       and self.spawns[self.capture_slot].is_set(position))
            if not on_camp and self.dodge_enemies(me):
                return
            if self.collect(me):
                return

        if self.action != FIGHT and self.dodge_enemies(me):
            return

        if self.action == RECALL:
            if me.health() < me.max_health() * RECALL_HEALTH:
                zone = threat_zone(alive(visible_champions(Team.ENEMY)), include_camps=True)
                try:
                    hot = zone.is_set(position)
                except cambc.Error:
                    hot = False
                if hot:
                    hit = step_out(me, ~zone)
                    self.log_turn(me, f"dodge {unit_at(hit)}" if hit is not None else "dodge")
                    return
            if self.recall_to_shop(me):
                return
            self.retreat(me)
            return
        if self.action == FIGHT:
            if gems >= ESCORT_GEMS and me.health() < FRONT_HEALTH:
                self.note(f"hp {me.health()}<{FRONT_HEALTH}")
                self.retreat(me)
                return
            if alive(visible_champions(Team.ENEMY)):
                if self.fight(me):
                    return
                self.retreat(me)
                return
            if self.ping_xy is not None:
                self.log_turn(me, f"join {at(self.ping_xy)}")
                move_towards_closest(tiles_around(self.ping_xy, 0))
                return
            self.retreat(me)
            return
        if self.action == TRIBUTE:
            if self.do_tribute(me):
                return
        if self.action == RETREAT:
            self.retreat(me)
            return
        if self.action == FARM and self.ward_detour(me, gems):
            return
        self.farm(me, avoid=self.enemy_attack_zone(), slot=self.my_farm_slot())

    # Wards

    def my_ward_index(self, me):
        # One spot per champion, in draft body order. Extra bodies past the spot list do not ward.
        try:
            index = WARD_ORDER.index(me.champion_name())
        except ValueError:
            return None
        if index >= len(self.ward_spots):
            return None
        return index

    def spot_covered(self, me, spot):
        allied = cambc.vision.ward_tiles(Team.ALLY)
        for own in me.ward_positions():
            allied.set(own, False)
        return not (allied & tiles_around(spot, ANTI_WARD_RADIUS - 1)).is_empty()

    def place_ward(self, me):
        index = self.my_ward_index(me)
        if index is None:
            return False
        if me.ward_positions():
            self.ward_slot = index
            return False
        self.ward_slot = None
        try:
            if me.ward_cooldown() > 0:
                return False
        except cambc.Error:
            return False
        spot = self.ward_spots[index]
        if self.spot_covered(me, spot):
            return False
        if distance(me.position(), spot) > cambc.constants.WARD_PLACEMENT_RADIUS + WARD_SLACK:
            return False
        tiles = me.ward_placement_tiles() & tiles_around(spot, WARD_SLACK)
        tiles &= ~(game_map.walls() | game_map.holes() | cambc.vision.ward_tiles())
        for tile in sorted(tiles.positions(), key=lambda t: distance(t, spot)):
            try:
                if not me.place_ward(tile):
                    continue
            except cambc.Error:
                continue
            self.ward_slot = index
            print(f"round {cambc.round_number()}: champion {int(me.champion_name())} wards spot {index}")
            return True
        return False

    def ward_detour(self, me, gems):
        # Only a champion whose own spot is still free, carrying little and not mid-clear, walks to it.
        index = self.my_ward_index(me)
        if (index is None or me.ward_positions() or gems >= IDLE_BANK
                or self.group_camps() or under_threat(me)):
            return False
        spot = self.ward_spots[index]
        if self.spot_covered(me, spot):
            return False
        limit = AGGRESSIVE_WARD_DETOUR_RANGE if self.aggressive else WARD_DETOUR_RANGE
        if distance(me.position(), spot) > limit:
            return False
        if not move_towards_closest(tiles_around(spot, cambc.constants.WARD_PLACEMENT_RADIUS)):
            return False
        self.log_turn(me, f"ward {at(spot)}")
        return True

    def nearest_altar(self, origin, altars):
        # Each altar keeps one terrain field. The champion's distance is a single get.
        ranked = []
        for altar in altars:
            steps = steps_to(self.altar_walk(altar), [origin])
            ranked.append((steps is None, steps or 0, distance(origin, altar), altar.x, altar.y, altar, steps))
        if not ranked:
            return None, None
        ranked.sort()
        best = ranked[0]
        return best[-2], best[-1]

    def altar_walk(self, altar):
        # Terrain does not change, so the walk from each altar is kept.
        if not hasattr(self, "altar_fields"):
            self.altar_fields = {}
        key = (altar.x, altar.y)
        if key not in self.altar_fields:
            self.altar_fields[key] = walk_field(tiles_around(altar, cambc.constants.SUBMIT_GEMS_RANGE))
        return self.altar_fields[key]

    def camp_on_way(self, origin, altar):
        # Same detour the route allows: a few extra steps, or a camp close enough to turn for.
        if not self.route:
            return None
        to_altar = self.altar_walk(altar)
        altar_steps = steps_to(to_altar, [origin])
        if altar_steps is None:
            return None
        best = None
        for slot in self.visible_loot(self.capture_slot):
            if not self.allowed(slot) or not self.camps_at(slot):
                continue
            to_camp = self.steps_at(slot, origin)
            camp_to_altar = steps_to(to_altar, self.spawn_lists[slot])
            if to_camp is None or camp_to_altar is None:
                continue
            extra = to_camp + camp_to_altar - altar_steps
            if extra <= ON_WAY_SLACK or to_camp <= NEAR_LOOT:
                rank = (to_camp, -self.values[self.aggressive][slot], slot)
                if best is None or rank < best[0]:
                    best = (rank, slot)
        return None if best is None else best[1]

    def bank_gems(self, me, altars=None):
        # A locked altar is still tried: the engine turns the tribute into an attack on the lock.
        # Camps on the walk are cleared the same way as camps on the way to the route.
        origin = me.position()
        choices = list(altars or game_map.altars().positions())
        for altar in choices:
            if not self.channel_cut and distance(origin, altar) <= cambc.constants.SUBMIT_GEMS_RANGE:
                try:
                    me.submit_gems(altar)
                    self.tributed = True
                    self.log_turn(me, f"tribute {at(altar)}")
                    return True
                except cambc.Error:
                    continue
        nearest, _steps = self.nearest_altar(origin, choices)
        if nearest is None:
            return False
        detour = self.camp_on_way(origin, nearest)
        if detour is not None:
            self.farm(me, avoid=self.enemy_attack_zone(), action="bank", slot=detour)
            return True
        reach = me.attack_range()
        passing = []
        for camp in self.living_camps():
            try:
                if camp.strength() <= 0 and within(me, camp, reach) and self.can_take_camp(camp):
                    passing.append(camp)
            except cambc.Error:
                continue
        if passing:
            target = lowest(passing)
            if target is not None:
                self.log_turn(me, f"bank pass {camp_at(target)}")
                self.engage(me, target, [], is_camp=True)
                return True
        goals = tiles_around(nearest, cambc.constants.SUBMIT_GEMS_RANGE)
        if not move_towards_closest(goals):
            return False
        self.log_turn(me, f"bank {at(nearest)}")
        return True

    def tribute_altar(self, me):
        nearest, _ = self.nearest_altar(me.position(), self.legal_altars(me))
        return nearest

    def altar_is_solo(self, altar):
        return any(altar.x == a.x and altar.y == a.y for a in self.solo_altars)

    def do_tribute(self, me):
        altar = self.tribute_altar(me)
        if altar is None:
            return False
        return self.bank_gems(me, [altar])

    def dodge_enemies(self, me):
        # A champion who is not fighting steps out of every enemy attack range.
        zone = self.enemy_attack_zone()
        try:
            inside = zone.is_set(me.position())
        except cambc.Error:
            return False
        if not inside:
            return False
        hit = step_out(me, ~zone)
        self.log_turn(me, f"dodge {unit_at(hit)}" if hit is not None else "dodge")
        return True

    def recall_to_shop(self, me):
        position = me.position()
        home = min(distance(position, tile) for tile in game_map.spawn_region(Team.ALLY).positions())
        if home <= cambc.constants.RECALL_TIME:
            if not move_towards_closest(game_map.spawn_region(Team.ALLY)):
                return False
            self.log_turn(me, "shop walk")
            return True
        if self.channel_cut or threatened(position):
            return False
        try:
            me.recall()
        except cambc.Error:
            return False
        self.log_turn(me, "shop recall")
        return True

    def leaving_spawn(self, me):
        # Walk out to the far side of the buffer. The swing on the way still lands; the chase does not resume.
        if not self.spawn_bait:
            return False
        if not self.retreat_zone.is_set(me.position()):
            self.spawn_bait = False
            return False
        hit = step_out(me, ~self.retreat_zone)
        self.log_turn(me, f"bait {unit_at(hit)}" if hit is not None else "bait")
        return True

    def leaving_leash(self, me):
        # Same two-radius abort as the spawn chase, but the centre is the farm or altar objective.
        # Once baited, walk back inside the inner ring before a fight is allowed again.
        if not self.leash_bait:
            return False
        leash = self.leash
        if leash is None or distance(me.position(), leash) <= LEASH_INNER:
            self.leash_bait = False
            return False
        hit = step_out(me, tiles_around(leash, LEASH_INNER))
        self.log_turn(me, f"leash {unit_at(hit)}" if hit is not None else "leash")
        return True

    def home_steps(self, enemy):
        # Walking steps from this enemy back to their spawn.
        return self.spawn_steps.get(enemy.position())

    def chase_targets(self, me, enemies):
        # Outside the buffer, only an enemy who followed us out is worth turning around for.
        # Inside the pursue ring, only someone who dies before they reach spawn. The band between
        # the two is an exit: nobody is chased there.
        baited = [e for e in enemies if self.home_steps(e) > SPAWN_RETREAT_RANGE]
        if not self.retreat_zone.is_set(me.position()):
            return baited
        if self.spawn_zone.is_set(me.position()):
            allies = alive(visible_champions(Team.ALLY))
            return [e for e in enemies if self.catchable(me, e, allies)]
        return []

    def fight(self, me):
        enemies = alive(visible_champions(Team.ENEMY))
        if not enemies:
            self.note("no-enemy")
            return False
        if self.leash is not None:
            gap = distance(me.position(), self.leash)
            if gap > LEASH_OUTER:
                self.leash_bait = True
                self.note(f"past {gap}>{LEASH_OUTER}")
                return self.leaving_leash(me)
        enemies = self.chase_targets(me, enemies)
        if not enemies:
            if self.retreat_zone.is_set(me.position()):
                self.spawn_bait = True
                self.note("no-catch" if self.spawn_zone.is_set(me.position()) else "spawn-band")
                return self.leaving_spawn(me)
            self.note("no-follow")
            return False
        if self.hold_behind_tanks(me, enemies):
            self.log_turn(me, "behind")
            return True
        # A tributing enemy is stuck in place with its bag, which the kill steals.
        in_reach = [e for e in enemies if within(me, e, me.attack_range())]
        target = (lowest([e for e in in_reach if channelling(e)]) or lowest(in_reach)
                  or lowest([e for e in enemies if channelling(e)]) or lowest(enemies))
        if target is None:
            self.note("no-target")
            return False
        self.log_turn(me, f"attack {unit_at(target)}")
        self.engage(me, target, enemies, is_camp=False)
        return True

    def catchable(self, me, enemy, allies):
        # Rounds for us to close in and land enough basic attacks (allies in reach help), against its walk home.
        try:
            position = enemy.position()
            health = enemy.health()
        except cambc.Error:
            return False
        close = []
        for ally in allies:
            try:
                if distance(ally.position(), position) <= ally.attack_range() + 1:
                    close.append(ally)
            except cambc.Error:
                continue
        if me not in close:
            close.append(me)
        allies = close
        damage = 0
        for ally in allies:
            try:
                damage += ally.strength()
            except cambc.Error:
                continue
        if damage <= 0:
            return False
        approach = max(0, distance(me.position(), position) - me.attack_range())
        rounds = approach + -(-health // damage)
        return rounds <= self.spawn_steps.get(position)

    def retreat(self, me):
        # A camp is only taken from outside every enemy attack range. Inside one, just leave.
        zone = self.enemy_attack_zone()
        try:
            inside = zone.is_set(me.position())
        except cambc.Error:
            inside = False
        if not inside:
            self.farm(me, avoid=zone, action="retreat")
            return True
        hit = step_out(me, ~zone)
        self.log_turn(me, f"retreat {unit_at(hit)}" if hit is not None else "retreat")
        return True

    def group_camps(self):
        return self.camps_at(self.slot % len(self.route)) if self.route else []

    def indexed_camps(self):
        # One pass from the cached camp list. slot_at already maps a spawn tile to its route slot.
        _touch_sight()
        if self._camp_gen == _sight.gen:
            return
        by_slot = {}
        camps = alive(visible_camps())
        for camp in camps:
            try:
                slot = self.slot_at.get(camp.spawn_position())
            except cambc.Error:
                continue
            if slot is None:
                continue
            bucket = by_slot.get(slot)
            if bucket is None:
                by_slot[slot] = [camp]
            else:
                bucket.append(camp)
        self._camps_by_slot = by_slot
        self._camps_all = camps
        self._camp_gen = _sight.gen

    def living_camps(self):
        self.indexed_camps()
        return self._camps_all

    def camps_at(self, slot):
        self.indexed_camps()
        return self._camps_by_slot.get(slot, ())

    def farm(self, me, avoid=None, action="farm", slot=None):
        if not self.route:
            self.log_turn(me, action)
            return
        if slot is None:
            slot = self.slot % len(self.route)
        camps = self.camps_at(slot)
        reach = me.attack_range()
        # Retreat keeps the capture outside the attack zone passed in as avoid.
        if avoid is not None:
            camps = [c for c in camps if not avoid.is_set(c.position())]
        # Clusters have no strength, so a passing hit is free and should not be walked past.
        passing = []
        for camp in self.living_camps():
            try:
                if (camp.strength() <= 0 and within(me, camp, reach) and self.can_take_camp(camp)
                        and (avoid is None or not avoid.is_set(camp.position()))):
                    passing.append(camp)
            except cambc.Error:
                continue
        if passing and not any(within(me, c, reach) for c in camps):
            target = lowest(passing)
            if target is None:
                self.log_turn(me, f"{action} walk")
                return
            self.log_turn(me, f"{action} pass {camp_at(target)}")
            self.engage(me, target, [], is_camp=True, avoid=avoid)
            return
        if not camps:
            spawns = self.spawns[slot]
            if self.ready_at[slot] > cambc.round_number():
                self.log_turn(me, f"{action} wait")
                ring = cambc.Bitboard()
                for tile in spawns.positions():
                    ring.draw_chebyshev(tile, WAIT_RANGE)
                move_towards_closest(ring & ~spawns, avoid)
            else:
                self.log_turn(me, f"{action} walk")
                move_towards_closest(spawns, avoid)
            return
        in_reach = [c for c in camps if within(me, c, reach)]
        target = lowest(in_reach)
        if target is None:
            best = None
            best_gap = None
            for camp in camps:
                try:
                    gap = distance(me.position(), camp.position())
                except cambc.Error:
                    continue
                if best is None or gap < best_gap:
                    best = camp
                    best_gap = gap
            target = best
        if target is None:
            self.log_turn(me, f"{action} walk")
            return
        self.log_turn(me, f"{action} {camp_at(target)}")
        self.engage(me, target, [], is_camp=True, avoid=avoid)

    # Kits

    def engage(self, me, target, enemies, is_camp, avoid=None):
        if target is None or not alive([target]):
            return
        if me.champion_name() == Champ.VESPER_VALE:
            self.engage_vesper(me, target, enemies, is_camp, avoid)
        elif me.attack_range() > 1:
            self.engage_ranged(me, target, enemies, is_camp, avoid)
        else:
            self.engage_melee(me, target, is_camp, avoid)

    def engage_melee(self, me, target, is_camp, avoid):
        kind = me.champion_name()
        reach = me.attack_range()

        if kind == Champ.VARG and under_threat(me):
            cast_self(ability_of(me, Kind.SHIELD))

        # Ren's stun excludes walking, so it is cast before any step. Camps take the damage and the stun.
        if kind == Champ.REN:
            stun = ability_of(me, Kind.CC)
            if stun is not None and within(me, target, stun.info().range):
                if not is_camp or stun.can_target_camps():
                    cast(stun, target)
            if not alive([target]):
                return

        # Ryth's area stun is the same: one cast, and it spends the walk. The reset is the other cast.
        stunned = kind == Champ.RYTH and self.ryth_stun(me, target, is_camp)
        if stunned and not alive([target]):
            return

        place = spot(target)
        if place is None:
            return
        gap = distance(me.position(), place)
        if not is_camp and kind in (Champ.ISSAC, Champ.VENDETTA, Champ.REN) and gap - reach >= dash_span(me):
            dash_towards(me, place, reach)

        if not within(me, target, reach):
            move_towards_closest(tiles_around(place, reach), avoid, only_full=not is_camp)

        if attack_if_in_range(target) and not stunned and kind in (Champ.ISSAC, Champ.VENDETTA, Champ.RYTH):
            if alive([target]) and cast_self(ability_of(me, Kind.ATTACK_RESET)):
                attack_if_in_range(target)

    def engage_ranged(self, me, target, enemies, is_camp, avoid):
        kind = me.champion_name()
        reach = me.attack_range()
        place = spot(target)
        if place is None:
            return
        gap = distance(me.position(), place)

        # Healing and the attack reset share Willow's one cast per turn.
        resets = kind == Champ.WILLOW and not self.heal_lowest_ally(me)
        if kind == Champ.CASSI:
            # A champion in range is the better stun. Otherwise the camp loses its attack.
            threats = enemies or alive(visible_champions(Team.ENEMY))
            if not self.cassi_stun(me, threats) and is_camp:
                stun = ability_of(me, Kind.CC)
                if stun is not None and stun.can_target_camps() and within(me, target, stun.info().range):
                    cast(stun, target)
            damage = ability_of(me, Kind.DAMAGE)
            if damage is not None and gap <= damage.info().range:
                cast(damage, target)
            if not alive([target]):
                return

        # Stellar's spells exclude walking. Step out of a camp before spending the cast.
        if kind == Champ.STELLAR and not (
                is_camp and self.should_hold_back(me, target) and gap < RANGED_CAMP_STANDOFF):
            if self.stellar_cast(me, target, is_camp) and not alive([target]):
                return

        # Hold just outside camp range while no allied melee is closer to absorb the camp's attacks.
        if is_camp and self.should_hold_back(me, target):
            ring = tiles_around(place, RANGED_CAMP_STANDOFF)
            ring &= ~tiles_around(place, RANGED_CAMP_STANDOFF - 1)
            move_towards_closest(ring, avoid)
            return

        if gap <= reach:
            if attack_if_in_range(target) and resets and alive([target]):
                if cast_self(ability_of(me, Kind.ATTACK_RESET)):
                    attack_if_in_range(target)
        else:
            move_towards_closest(tiles_around(place, reach), avoid, only_full=not is_camp)

    def should_hold_back(self, me, camp):
        # Clusters have no strength, so there is nothing to absorb.
        try:
            if camp.strength() <= 0:
                return False
            my_gap = distance(me.position(), camp.position())
            camp_at_tile = camp.position()
        except cambc.Error:
            return False
        melees = [a for a in alive(visible_champions(Team.ALLY)) if a != me and is_melee(a)]
        if not melees:
            return False
        try:
            return all(distance(a.position(), camp_at_tile) >= my_gap for a in melees)
        except cambc.Error:
            return False

    def ryth_stun(self, me, target, is_camp):
        # Ground stun in radius one. One body is enough; a cluster is taken when the centre covers it.
        bodies = [] if is_camp else alive(visible_champions(Team.ENEMY))
        if all(body != target for body in bodies):
            bodies.append(target)
        if cast_aoe(me, ability_of(me, Kind.AOE_DAMAGE), bodies, target, 1):
            return True
        stun = ability_of(me, Kind.CC)
        if stun is None or not within(me, target, stun.info().range):
            return False
        if is_camp and not stun.can_target_camps():
            return False
        return cast(stun, target)

    def stellar_cast(self, me, target, is_camp):
        # Single hit is 2x Focus. The area is 1.4x, so it wins once two bodies are in it.
        single = ability_of(me, Kind.DAMAGE)
        area = ability_of(me, Kind.AOE_DAMAGE)
        bodies = alive(visible_champions(Team.ENEMY))
        if is_camp or all(body != target for body in bodies):
            bodies.append(target)
        single_ok = False
        if single is not None and single.is_ready() and (not is_camp or single.can_target_camps()):
            try:
                single_ok = within(me, target, single.info().range)
            except (cambc.Error, AttributeError, TypeError, ValueError):
                single_ok = False
        single_hit = spell_damage(single) if single_ok else 0.0
        center = None
        hits = 0
        area_hit = 0.0
        if area is not None and area.is_ready():
            try:
                info = area.info()
                center, hits = aoe_aim(me.position(), bodies, info.range, info.radius, target)
                area_hit = spell_damage(area)
            except (cambc.Error, AttributeError, TypeError, ValueError):
                center = None
        use_area = False
        if center is not None and hits > 0:
            if single_ok and single_hit > 0 and area_hit > 0:
                use_area = hits * area_hit > single_hit
            elif single_ok:
                use_area = hits >= 2
            else:
                use_area = True
        if use_area and cast(area, center):
            return True
        return single_ok and cast(single, target)

    def cassi_stun(self, me, enemies):
        stun = ability_of(me, Kind.CC)
        if stun is None or not stun.is_ready():
            return False
        near = [e for e in enemies if within(me, e, stun.info().range)]
        target = lowest([e for e in near if channelling(e)]) or lowest(near)
        return target is not None and cast(stun, target)

    def heal_lowest_ally(self, me):
        heal = ability_of(me, Kind.HEAL)
        if heal is None or not heal.is_ready():
            return False
        hurt = []
        for ally in alive(visible_champions(Team.ALLY)):
            if ally == me or not within(me, ally, heal.info().range):
                continue
            try:
                if ally.health() < ally.max_health():
                    hurt.append(ally)
            except cambc.Error:
                continue
        target = None
        missing = None
        for ally in hurt:
            try:
                ratio = ally.health() / ally.max_health()
            except cambc.Error:
                continue
            if target is None or ratio < missing:
                target = ally
                missing = ratio
        return target is not None and cast(heal, target)

    def engage_vesper(self, me, target, enemies, is_camp, avoid):
        # One ability per turn: attack reset doubles a hit; otherwise the dash closes in or kites out.
        reach = me.attack_range()
        place = spot(target)
        if place is None:
            return
        if is_camp and self.should_hold_back(me, target):
            ring = tiles_around(place, RANGED_CAMP_STANDOFF)
            ring &= ~tiles_around(place, RANGED_CAMP_STANDOFF - 1)
            move_towards_closest(ring, avoid)
            return

        # Ranged walking uses up the attack, so dash straight into range when the gap is at least the dash.
        gap = distance(me.position(), place)
        dashed = (not within(me, target, reach) and not is_camp and gap - reach >= dash_span(me)
                  and dash_towards(me, place, reach - 1))
        if not within(me, target, reach):
            if not dashed:
                move_towards_closest(tiles_around(place, reach), avoid, only_full=not is_camp)
            return

        if not attack_if_in_range(target) or dashed:
            return
        if alive([target]) and cast_self(ability_of(me, Kind.ATTACK_RESET)):
            attack_if_in_range(target)
        elif not is_camp:
            self.vesper_kite(me, enemies, target, reach)

    def vesper_kite(self, me, enemies, target, reach):
        # After firing, dash to the tile furthest outside enemy reach, preferring ones still in range.
        dash = ability_of(me, Kind.DASH)
        threats = []
        for enemy in alive(enemies):
            try:
                threats.append((enemy.position(), enemy.attack_range()))
            except cambc.Error:
                continue
        if dash is None or not dash.is_ready() or not threats:
            return False
        aim = spot(target) if alive([target]) else None

        def margin(tile):
            return min(distance(tile, spot) - reach_of for spot, reach_of in threats)

        position = me.position()
        if margin(position) > 0:
            return False
        free = tiles_around(position, int(dash.info().range)) & ~blocked_tiles()
        best = max(free.positions(), default=None,
                   key=lambda t: (margin(t) > 0, aim is not None and distance(t, aim) <= reach, margin(t)))
        return best is not None and margin(best) > margin(position) and cast(dash, best)

    # Shopping

    def vesper_recall_gold(self, me):
        # Recall once the purse covers what is left of this batch, or the rest of the build if smaller.
        if emblem_count(me) >= VESPER_CORE_EMBLEMS:
            return VESPER_RECALL_GOLD
        try:
            counts = me.item_count()
        except cambc.Error:
            return VESPER_RECALL_GOLD
        fallback = SHOP_FALLBACK[Champ.VESPER_VALE]
        extra = set(fallback) if isinstance(fallback, tuple) else {fallback}
        build = set(SHOP_ORDER[Champ.VESPER_VALE]) | extra
        loose = sum(counts[shard] * shard.cost() for shard in build)
        left = EMBLEM_WORTH * (VESPER_CORE_EMBLEMS - emblem_count(me)) - loose
        return max(Item.POWER_SHARD_OF_STRENGTH.cost(), min(VESPER_RECALL_GOLD - self.batch_spent, left))

    def buy_items(self, me):
        kind = me.champion_name()
        before = me.gold_count()
        self.shop(me, kind)
        if kind == Champ.VESPER_VALE:
            self.batch_spent = (self.batch_spent + before - me.gold_count()) % VESPER_RECALL_GOLD

    def shop(self, me, kind):
        # A family listed twice wants a second emblem, so its shards are bought again once the first is done.
        wanted = {}
        counts = None
        for shard in SHOP_ORDER.get(kind, ()):
            if counts is None:
                try:
                    counts = me.item_count()
                except cambc.Error:
                    return
            emblem = shard.result()
            if emblem is not None:
                wanted[emblem] = wanted.get(emblem, 0) + 1
                if counts[emblem] >= wanted[emblem]:
                    continue
            need = max(0, shard.combine_count() - counts[shard])
            afford = me.gold_count() // shard.cost()
            if buy(me, shard, min(need, afford)):
                counts = None
            if afford < need:
                return
        fallback = SHOP_FALLBACK.get(kind)
        if fallback is None:
            return
        if not isinstance(fallback, tuple):
            buy(me, fallback, me.gold_count() // fallback.cost())
            return
        while True:
            bought_any = False
            for shard in fallback:
                try:
                    counts = me.item_count()
                except cambc.Error:
                    return
                need = max(0, shard.combine_count() - counts[shard])
                if need == 0:
                    continue
                afford = me.gold_count() // shard.cost()
                if buy(me, shard, min(need, afford)):
                    bought_any = True
                if afford < need:
                    return
            if not bought_any:
                return
