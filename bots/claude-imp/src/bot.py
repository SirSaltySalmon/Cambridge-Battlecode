"""Champion decisions. main.py dispatches here after the draft."""
import struct

import cambc
from cambc import game_map

import draft

Team, Champ, Kind, Item = cambc.Team, cambc.ChampionType, cambc.AbilityKind, cambc.ItemType

# Turn thresholds.
RECALL_HEALTH = 0.35
GEM_BANK = 40
# Bank bar: this many gems per step to the nearest altar, between these bounds.
BANK_PER_STEP = 2
GEM_BANK_MIN = 30
MAX_BANK_BAR = 90
# Danger banking only walks to an altar this close.
DANGER_ALTAR_STEPS = 12
# Bank smaller bags when an altar is this many steps away, or when enemies are in sight.
NEAR_BANK_GEMS = 10
NEAR_ALTAR_STEPS = 6
DANGER_BANK_GEMS = 25
# An altar with a visible enemy this close is skipped while a safer one exists.
ALTAR_DANGER = 5
# A safe altar may be at most this much further than the nearest one.
SAFE_DETOUR = 10
# Allies within this range of a team-mate channelling at least this many gems stay to guard it.
GUARD_RANGE = 10
GUARD_GEMS = 50
THREAT_RANGE = 4
# While the target is still respawning, bank any bag at least this big.
IDLE_BANK = 10
# Recall to shop after a tribute when carrying this much gold.
RECALL_GOLD = 150
# Vesper collects most loot, so her purse fills fast; a higher bar keeps her with the team.
VESPER_RECALL_GOLD = 600

# Route targeting: a new planned goal must beat the current one by this factor.
SWITCH_MARGIN = 1.25
# Wait this far from a respawning group; standing on its spawn tiles delays it.
WAIT_RANGE = 2
# Escorts guarding a channelling carrier only farm groups of these kinds.
FREE_KINDS = (cambc.CampType.WOLF, cambc.CampType.GEM_CLUSTER, cambc.CampType.GOLD_CLUSTER)
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
# Vesper Vale earns bonus gold, so she collects loot first until she owns this many emblems.
VESPER_EMBLEM_CAP = 6
# Vesper takes over a capture claim herself when the cleared group is this close to her.
VESPER_CLAIM_RANGE = 8
CLAIM_ROUND_OFFSET = 6

# Escorted tributes: a bag this big is only carried to an altar with the team around it.
ESCORT_GEMS = 150
# Escorts within this range of the carrier count; this many (or every living ally if fewer) must be there.
ESCORT_RANGE = 3
ESCORT_MIN = 2
# Waiting carriers stand this far from the escort altar, and give up waiting after this many turns.
STAGING_RANGE = 5
ESCORT_WAIT = 15
# While the carrier channels, escorts farm free camps this close to the altar.
PROTECT_RANGE = 6
# A carrier below this health stays behind the team instead of fighting at the front.
FRONT_HEALTH = 800
# An escort is dropped after this many rounds, in case the carrier died out of everyone's sight.
ESCORT_TIMEOUT = 60
# Altars this close to the map centre are contested, so heavy bags only go there escorted.
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

# Fight decision: power ratio needed to start a fight, and to keep fighting one already started.
START_FIGHT = 1.0
KEEP_FIGHTING = 0.7
# Only champions this close to the fight count, on either side.
ENGAGE_RANGE = 9
# Each living enemy out of sight adds this share to the enemy's power.
HIDDEN_ENEMY_WEIGHT = 0.15
# Fighting on (or next to) a spawn heals that side every round.
SPAWN_FIGHT_BONUS = 2.0
# Focus counts this much towards damage per round (spells hit for ~2x Focus every 8 rounds).
FOCUS_DPS = 0.25

# Focus fire: candidates within reach + this, healers and channellers are worth this much health,
# and a target out of reach costs this much.
FOCUS_SLACK = 2
CHANNEL_FOCUS_BONUS = 300
HEALER_FOCUS_BONUS = 200
OUT_OF_REACH_COST = 250
# A retreat this close to home keeps running; further out, being chased means turning to fight.
SAFE_HOME_STEPS = 6

# Fight score.
FIGHT_THRESHOLD = 5
OUTNUMBER_BONUS = 10
MISSING_BODY_PENALTY = 5
# Within this many tiles of the enemy spawn, only enemies that die before reaching it are chased.
SPAWN_CHASE_RANGE = 8
HEALTH_EDGE = 5
POWER_EDGE = 5
EMBLEM_BONUS = 3
DISTANCE_PENALTY = 0.5
# A champion under this max health is squishy, and fights less while ahead of every melee ally.
SQUISHY_HEALTH = 900
OVEREXTEND_PENALTY = 10
# Standing on our own spawn is safe: it heals us, so a fight scores higher.
SPAWN_BONUS = 50

# Highest acts first, from the round after it is set.
PRIORITY = {
    Champ.CASSI: 6,
    Champ.ISSAC: 5,
    Champ.VENDETTA: 4,
    Champ.VARG: 3,
    Champ.VESPER_VALE: 2,
    Champ.REN: 1,
    Champ.WILLOW: 0,
}

# Camp value: gold is weighted at the draft's farming weight until this round, then falls
# linearly to the late weight.
GOLD_DECAY_START = 150
GOLD_DECAY_END = 450
LATE_GOLD_WEIGHT = 0.2

# Fuel: emblem counts are polled this often, and per-group distance fields are only built
# while this much of the turn's fuel is left.
EMBLEM_POLL = 10
FIELD_FUEL_RESERVE = 1_200_000

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
# Vesper Vale's kit is all basic attacks, so she builds Strength only
_VESPER_STRENGTH = (Item.TREASURE_SHARD_OF_STRENGTH, Item.MONSTER_SHARD_OF_STRENGTH, Item.DURABILITY_SHARD_OF_STRENGTH, Item.POWER_SHARD_OF_STRENGTH,)
SHOP_ORDER = {
    Champ.VENDETTA: _VENDETTA_STRENGTH,
    Champ.VARG: _MELEE_STRENGTH,
    Champ.ISSAC: _MELEE_STRENGTH,
    Champ.REN: _MELEE_FOCUS,
    Champ.CASSI: _RANGED_FOCUS,
    Champ.WILLOW: _RANGED_FOCUS,
    Champ.VESPER_VALE: _VESPER_STRENGTH,
}
SHOP_FALLBACK = {
    Champ.VENDETTA: Item.POWER_SHARD_OF_STRENGTH,
    Champ.VARG: Item.POWER_SHARD_OF_STRENGTH,
    Champ.ISSAC: Item.POWER_SHARD_OF_STRENGTH,
    Champ.REN: Item.DURABILITY_SHARD_OF_FOCUS,
    Champ.CASSI: Item.POWER_SHARD_OF_FOCUS,
    Champ.WILLOW: Item.POWER_SHARD_OF_FOCUS,
    Champ.VESPER_VALE: Item.POWER_SHARD_OF_STRENGTH,
}

FARM, FIGHT, RETREAT, TRIBUTE = 0, 1, 2, 3
MODE_NAMES = ("farm", "fight", "retreat", "tribute")

# Shared memory: mode, target slot, capture slot, collector champion + 1, round stamp, capture claim round
# (low byte only; it is just aged against CAPTURE_TIMEOUT), then the ward byte the leader never rewrites.
# The stamp is round + 1 so zeroed memory reads as unwritten; capture slot NO_CAPTURE means nobody is collecting.
# The mode byte's top bit records that Vesper has finished her build; only she can see her own items.
# The next bit records whether the leader put the team on the aggressive route this round.
# Bits 2-5 hold the escorted carrier's champion + 1 (0 for none); the carrier sets and clears it itself.
SHARED_LAYOUT = "<BBBBHB"
# Bit i is set once ward spot i is covered, so nobody else spends a ward inside its anti-ward radius.
WARD_MASK_BYTE = 7
NO_CAPTURE = 255
SLOT_BYTE = 1
CAPTURE_SLOT_BYTE = 2
MODE_BYTE = 0
VESPER_DONE_FLAG = 0x80
AGGRESSIVE_FLAG = 0x40
MODE_MASK = 0x03
CARRIER_SHIFT = 2
CARRIER_MASK = 0x3C

EMBLEMS = tuple(i for i in range(Item.COUNT) if "EMBLEM" in Item(i).name)
SHARDS = tuple(i for i in range(Item.COUNT) if "SHARD" in Item(i).name)
# An emblem is five 30-gold shards.
EMBLEM_WORTH = 150


def distance(a, b):
    return a.chebyshev_distance(b)


def tiles_around(position, radius):
    tiles = cambc.Bitboard()
    tiles.draw_chebyshev(position, radius)
    return tiles


def walk_field(roots):
    # Walking steps from the roots around terrain; camps and champions move or die, so they don't block.
    field = cambc.DistanceField()
    reached = cambc.Bitboard()
    field.compute(terrain(), roots, reached)
    return field, reached


def steps_to(walk, tiles):
    field, reached = walk
    steps = [field.get(t) for t in tiles if reached.is_set(t)]
    return min(steps) if steps else None


_STATIC = {}


def static_blocked():
    # Terrain never changes, so its union is built once.
    board = _STATIC.get("blocked")
    if board is None:
        board = _STATIC["blocked"] = (game_map.walls() | game_map.holes() | game_map.altars()
                                      | game_map.spawn_region(Team.ENEMY))
    return board


def terrain():
    board = _STATIC.get("terrain")
    if board is None:
        board = _STATIC["terrain"] = game_map.walls() | game_map.holes()
    return board


def blocked_tiles():
    # Combine terrain and visible occupants using native bitboard operations.
    blocked = (static_blocked() | cambc.vision.champion_tiles()
               | cambc.vision.camp_tiles() | cambc.vision.teleport_tiles())
    blocked.set(cambc.me().position(), False)
    return blocked


def move_towards_closest(goals, avoid=None):
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
    dash = ability_of(me, Kind.DASH)
    if dash is not None and not dash.is_ready():
        dash = None
    try:
        if dash is None and avoid is None and goals.count() == 1 and not blocked.is_set(goals.positions()[0]):
            moved = me.move_towards(goals.positions()[0])
            T.invalidate()
            return moved

        # Expand from the goals until our current tile is reached.
        field = cambc.DistanceField()
        reached = cambc.Bitboard()
        field.compute_to(blocked & ~goals, goals, tiles_around(origin, 0), reached)

        # Dash whenever it is ready (it leaves the walk free), then walk on from where it lands.
        dashed = dash is not None and dash_along(dash, origin, field, reached & ~blocked)
        if dashed:
            origin = me.position()
            blocked = blocked_tiles() if avoid is None else (blocked_tiles() | avoid)
            blocked.set(origin, False)
            if goals.is_set(origin) or not me.can_move():
                return True

        # Choose a reachable step with a smaller distance to the goal.
        reachable = blocked.reachable_tiles(origin, me.base_stats().base_move_speed)
        step = min([origin, *(reachable & reached).positions()], key=field.get)
        moved = step != origin and me.move_absolute(step)
        if moved:
            T.invalidate()
        return moved or dashed
    except cambc.Error:
        return False


def dash_along(dash, origin, field, landings):
    # Land on the tile that gains the most ground towards the goal; any gain is worth the cooldown.
    here = field.get(origin)
    tiles = tiles_around(origin, int(dash.info().range)) & landings
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
        T.damaged()
        return True
    except cambc.Error:
        return False


def alive(units):
    living = []
    for unit in units:
        try:
            if unit.health() > 0:
                unit.position()
                living.append(unit)
        except cambc.Error:
            pass
    return living


def safe_champions(team):
    # vision.champions can raise when a listed tile holds a unit the SDK can't resolve (e.g. mid-teleport).
    try:
        return cambc.vision.champions(team)
    except cambc.Error:
        found = []
        for champion in cambc.champions(team):
            try:
                if champion.is_visible():
                    found.append(champion)
            except cambc.Error:
                pass
        return found


def safe_camps():
    try:
        return cambc.vision.camps()
    except cambc.Error:
        return []


class TurnCache:
    # Vision lists are re-read many times per turn; the engine calls behind them are fuel-heavy.
    def __init__(self):
        self.round = -1
        self.reset()

    def reset(self):
        self._enemies = self._allies = self._camps = None

    def start(self, round_number):
        self.round = round_number
        self.reset()

    def enemies(self):
        if self._enemies is None:
            self._enemies = alive(safe_champions(Team.ENEMY))
        return self._enemies

    def allies(self):
        if self._allies is None:
            self._allies = alive(safe_champions(Team.ALLY))
        return self._allies

    def camps(self):
        if self._camps is None:
            self._camps = alive(safe_camps())
        return self._camps

    def damaged(self):
        # Anything we hit may have died: re-read lazily.
        self._enemies = self._camps = None

    def invalidate(self):
        # Moving changes what we can see.
        self.reset()


T = TurnCache()


def lowest(units):
    return min(units, key=lambda unit: unit.health(), default=None)


def within(me, unit, radius):
    return distance(me.position(), unit.position()) <= radius


def ability_of(me, kind):
    for ability in me.abilities():
        if ability.has(kind):
            return ability
    return None


def cast(ability, target=None):
    if ability is None:
        return False
    try:
        used = ability.is_ready() and bool(ability.use(target))
    except cambc.Error:
        return False
    if used:
        T.invalidate()
    return used


def cast_self(ability):
    # Self-cast is untargeted in the SDK; fall back to targeting ourselves.
    if ability is None or not ability.is_ready():
        return False
    for target in (None, cambc.me()):
        try:
            return bool(ability.use(target))
        except cambc.Error:
            continue
    return False


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


def fight_power(units):
    damage = health = 0.0
    for unit in units:
        try:
            damage += unit.strength() + FOCUS_DPS * unit.focus()
            health += unit.health()
        except cambc.Error:
            pass
    return damage * health


def threat_zone(enemies, include_camps):
    zone = cambc.Bitboard()
    for enemy in enemies:
        zone.draw_chebyshev(enemy.position(), THREAT_RANGE)
    if include_camps:
        for camp in T.camps():
            if camp.strength() > 0:
                zone.draw_chebyshev(camp.position(), camp.attack_range())
    return zone


def under_threat(me):
    # An enemy that can already hit us, or a damaged (awake) camp in its own range.
    position = me.position()
    for enemy in T.enemies():
        if distance(position, enemy.position()) <= enemy.attack_range():
            return True
    for camp in T.camps():
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


def melee_danger(enemies):
    # Tiles an enemy melee champion can hit next turn (one step, then a swing).
    zone = cambc.Bitboard()
    for enemy in enemies:
        try:
            if enemy.attack_range() <= 1:
                zone.draw_chebyshev(enemy.position(), 2)
        except cambc.Error:
            pass
    return zone


def dash_towards(me, target_position, reach, avoid=None):
    # Dash to the free tile closest to the target, if it leaves at most one walk to reach it.
    dash = ability_of(me, Kind.DASH)
    if dash is None or not dash.is_ready():
        return False
    origin = me.position()
    gap = distance(origin, target_position)
    free = tiles_around(origin, int(dash.info().range)) & ~blocked_tiles()
    if avoid is not None:
        free &= ~avoid
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
        self.spawns = []
        # Planned route slots for the normal and aggressive plans; every other group is a stop on the way.
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
        self.mode = FARM
        self.slot = 0
        self.capture_slot = NO_CAPTURE
        self.collector = 0
        self.claimed = 0
        self.tributed = False
        self.vesper_done = False
        self.aggressive = False
        # Gold Vesper has spent toward the current VESPER_RECALL_GOLD batch.
        self.batch_spent = 0
        self.ward_spots = []
        # Index of the ward spot our own ward covers, or None while it is free to move.
        self.ward_slot = None
        # Set from spawn until we reach the shared target again; everyone starts on spawn.
        self.rejoining = True
        # Whether this champion settled the shared decision this round.
        self.led = False
        # Champion + 1 of the escorted carrier, 0 for none, and how long we have waited for escorts.
        self.carrier = 0
        self.local_mode = FARM
        self.escort_wait = 0
        self.carrier_since = 0
        # Altars a heavy bag may visit alone (far from the centre, on our side) and the escorted rest.
        self.solo_altars = []
        self.escort_altars = []

    def setup(self):
        groups = game_map.camp_groups()
        data = bytes(cambc.DraftMemoryBuffer())
        _, normal, offset = draft.decode_pocket(data)
        _, aggressive, _ = draft.decode_pocket(data, offset)
        if not normal or any(index >= len(groups) for index in normal):
            _, normal = draft.compute_pocket()
            _, aggressive = draft.compute_pocket(aggressive=True)
        if any(index >= len(groups) for index in aggressive):
            aggressive = []
        if not normal:
            normal = list(range(len(groups)))

        # Every champion builds the same slot order, so shared slots agree; both plans share one slot space.
        planned = list(dict.fromkeys(normal + aggressive))
        seen = set(planned)
        extras = [index for index, group in enumerate(groups)
                  if index not in seen and group.camps()]
        indexes = (planned + extras)[:NO_CAPTURE]
        slot_of = {index: slot for slot, index in enumerate(indexes)}
        normal_slots = [slot_of[index] for index in normal if index in slot_of]
        aggressive_slots = [slot_of[index] for index in aggressive if index in slot_of] or normal_slots
        self.cores = (normal_slots, aggressive_slots)

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
        self.gem_values = [group.gem_value() for group in self.route]
        self.gold_values = [group.gold_value() for group in self.route]
        self.damage_costs = [draft.group_value(group, 0) - group.gem_value() for group in self.route]
        self.damage_costs = [-c if c < 0 else 0 for c in self.damage_costs]
        self.ready_at = [0] * len(self.route)
        self.all_spawns = cambc.Bitboard()
        for spawns in self.spawns:
            self.all_spawns |= spawns
        # Walking distance from each group, built lazily within the fuel budget and reused every turn.
        self.group_fields = [None] * len(self.route)
        self.home = walk_field(game_map.spawn_region(Team.ALLY))
        # Steps from any tile to the nearest altar (1 means adjacent, close enough to tribute).
        self.altar_steps = walk_field(game_map.altars())
        self.ward_spots = ward_spots()
        self.split_altars()
        # Walking steps from every tile to the enemy spawn, and the tiles close enough to it to stop chasing.
        theirs = game_map.spawn_region(Team.ENEMY)
        self.spawn_steps = cambc.DistanceField()
        self.spawn_steps.compute(terrain(), theirs, cambc.Bitboard())
        self.spawn_zone = cambc.Bitboard()
        for tile in theirs.positions():
            self.spawn_zone.draw_chebyshev(tile, SPAWN_CHASE_RANGE)
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
        seen = cambc.vision.visible_tiles() & self.all_spawns
        now = cambc.round_number()
        slots = {self.slot_at[tile] for tile in seen.positions() if tile in self.slot_at}
        for index in slots:
            group = self.route[index]
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
        # Item reads are costly and emblems change rarely, so look every few rounds.
        if cambc.round_number() % EMBLEM_POLL != 0:
            return
        for ally in T.allies():
            try:
                key = int(ally.champion_name())
            except cambc.Error:
                continue
            self.emblems[key] = max(self.emblems.get(key, 0), emblem_count(ally))
        self._team_emblems = None

    def team_emblems(self):
        # Items are only readable in vision, so each champion keeps the most it has seen.
        # Vesper's done flag means she finished her six even if nobody else can see her bag.
        if getattr(self, "_team_emblems", None) is not None:
            return self._team_emblems
        total = sum(self.emblems.values())
        if self.vesper_done:
            have = self.emblems.get(int(Champ.VESPER_VALE), 0)
            if have < VESPER_EMBLEM_CAP:
                total += VESPER_EMBLEM_CAP - have
        self._team_emblems = total
        return total

    def group_field(self, slot, build=True):
        entry = self.group_fields[slot]
        if entry is None and build and cambc.fuel_remaining() > FIELD_FUEL_RESERVE:
            entry = self.group_fields[slot] = walk_field(self.spawns[slot])
        return entry

    def travel_steps(self, slot, origin):
        # Walking steps from origin to the group, or a straight-line guess until its field is built.
        entry = self.group_field(slot)
        if entry is None:
            return max(0, distance(origin, self.centers[slot]) - 1)
        field, reached = entry
        if not reached.is_set(origin):
            return None
        return field.get(origin)

    def allowed(self, slot):
        return slot not in self.monoliths or self.team_emblems() >= MONOLITH_EMBLEMS

    def can_take_camp(self, camp):
        slot = self.slot_at.get(camp.spawn_position())
        return slot is None or self.allowed(slot)

    def gap(self, start, goal):
        key = (start, goal)
        if key not in self.gaps:
            entry = self.group_field(goal)
            if entry is None:
                return max(0, distance(self.centers[start], self.centers[goal]) - 1)
            self.gaps[key] = steps_to(entry, self.spawn_lists[start])
        return self.gaps[key]

    def mid_clear(self, slot):
        return any(camp.health() < camp.max_health() for camp in self.camps_at(slot))

    def visible_loot(self, skip):
        # Groups whose camps or leftover reward are in our own sight.
        loot = set(self.rewards)
        for camp in T.camps():
            slot = self.slot_at.get(camp.spawn_position())
            if slot is not None:
                loot.add(slot)
        loot.discard(skip)
        now = cambc.round_number()
        return [s for s in loot if self.ready_at[s] <= now or s in self.rewards]

    def gold_weight(self):
        # Gold buys stats, which pay off over the rest of the game: worth a lot early, little late.
        if self.aggressive:
            return draft.GOLD_WEIGHT
        now = cambc.round_number()
        if now <= GOLD_DECAY_START:
            return draft.FARM_GOLD_WEIGHT
        if now >= GOLD_DECAY_END:
            return LATE_GOLD_WEIGHT
        share = (now - GOLD_DECAY_START) / (GOLD_DECAY_END - GOLD_DECAY_START)
        return draft.FARM_GOLD_WEIGHT + share * (LATE_GOLD_WEIGHT - draft.FARM_GOLD_WEIGHT)

    def value(self, slot):
        if getattr(self, "_value_round", None) != cambc.round_number():
            weight = self.gold_weight()
            self._value_cache = [max(0.0, gem + weight * gold - cost)
                                 for gem, gold, cost in zip(self.gem_values, self.gold_values, self.damage_costs)]
            self._value_round = cambc.round_number()
        return self._value_cache[slot]

    def route_rate_steps(self, slot, steps):
        wait = max(0, self.ready_at[slot] - cambc.round_number() - steps)
        cycle = steps + wait + self.clears[slot] + cambc.constants.CAPTURE_TIME
        return self.value(slot) / max(1, cycle)

    def best_goal(self, travel):
        # Planned pocket: gold-weighted while farming, the centre pocket once aggressive.
        cores = [s for s in self.cores[self.aggressive] if s in travel] or list(travel)
        return max(cores, key=lambda s: self.route_rate_steps(s, travel[s]))

    def travel_from(self, origin, skip):
        # Steps to every allowed group, memoised for the turn (leaders and followers both ask).
        key = (cambc.round_number(), origin, skip)
        if getattr(self, "_travel_key", None) == key:
            return self._travel
        travel = {}
        for s in range(len(self.route)):
            if s != skip and self.allowed(s):
                steps = self.travel_steps(s, origin)
                if steps is not None:
                    travel[s] = steps
        self._travel_key, self._travel = key, travel
        return travel

    def pick_target(self, slot, origin, skip, seen_only=False):
        # Walk toward the planned goal, but take every visible group that sits on that walk
        # (or close enough that turning aside is cheap). A group already under attack is finished.
        # The group being collected is skipped so everyone else moves on. Monoliths wait until
        # the team is strong enough. With seen_only, only loot this champion can see is offered.
        if slot != skip and self.mid_clear(slot):
            return slot
        travel = self.travel_from(origin, skip)
        if not travel:
            return slot

        dest = self.best_goal(travel)
        loot = [s for s in self.visible_loot(skip) if s in travel]
        if seen_only and not loot:
            return slot

        def extra(s):
            if s == dest:
                return 0
            gap = self.gap(s, dest)
            if gap is None:
                return 255
            return travel[s] + gap - travel.get(dest, 0)

        on_way = [s for s in loot if extra(s) <= ON_WAY_SLACK or travel[s] <= NEAR_LOOT]
        if on_way:
            return min(on_way, key=lambda s: (travel[s], -self.value(s), s))

        if seen_only:
            return slot

        wait = max(0, self.ready_at[dest] - cambc.round_number() - travel[dest])
        if wait == 0:
            if slot in travel and slot != skip:
                here = self.route_rate_steps(slot, travel[slot])
                if here * SWITCH_MARGIN >= self.route_rate_steps(dest, travel[dest]):
                    return slot
            return dest
        # The pocket is down, so spend the wait on the best group from here.
        return max(travel, key=lambda s: self.route_rate_steps(s, travel[s]))

    def update_rejoining(self, me, slot):
        if not me.is_alive():
            return
        position = me.position()
        if game_map.spawn_region(Team.ALLY).is_set(position):
            self.rejoining = True
        elif self.rejoining and self.route and distance(position, self.centers[slot % len(self.route)]) <= TEAM_RANGE:
            self.rejoining = False

    def can_lead(self, me, stale):
        # Only a champion with the team settles the target: one on spawn or walking back from it would
        # pick from its own position and drag the shared target away from where the team is farming.
        # If nobody led last round the whole team is away, so anyone may re-pick from where they are.
        if not me.is_alive() or channelling(me) or me.is_teleporting():
            return False
        return stale or not self.rejoining

    def share_loot(self, me):
        # Vision isn't shared, so a follower that sees on-the-way loot the leader can't points the team at it.
        # Loot behind the follower (further from the target than it is) would drag the team back.
        if (self.led or self.rejoining or self.mode != FARM or not self.route
                or not me.is_alive() or channelling(me) or under_threat(me)):
            return
        slot = self.slot % len(self.route)
        position = me.position()
        pick = self.pick_target(slot, position, self.capture_slot, seen_only=True)
        if pick == slot or distance(self.centers[pick], self.centers[slot]) > distance(position, self.centers[slot]):
            return
        self.slot = pick
        cambc.shared_memory[SLOT_BYTE] = self.slot
        print(f"round {cambc.round_number()}: champion {int(me.champion_name())} spots loot at route slot {self.slot}")

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
        allies = [a for a in T.allies() if not channelling(a)]
        if me not in allies:
            allies.append(me)
        builders = [a for a in allies if a.champion_name() == Champ.VESPER_VALE
                    and emblem_count(a) < VESPER_EMBLEM_CAP]
        # A building Vesper collects first; otherwise the poorest visible ally, so the team catches up evenly.
        pool = builders or allies
        chosen = min(pool, key=lambda a: (item_worth(a),
                                          min(distance(a.position(), t) for t in spawns)))
        print(f"round {now}: champion {int(chosen.champion_name())} collects route slot {slot}")
        return slot, int(chosen.champion_name()) + 1, now

    # Shared decision

    def sync_decision(self, me):
        mode, slot, capture_slot, collector, stamp, claimed, vesper_done, aggressive = read_shared()
        if (not vesper_done and me.champion_name() == Champ.VESPER_VALE
                and emblem_count(me) >= VESPER_EMBLEM_CAP):
            vesper_done = True
            mark_vesper_done()
        self.vesper_done = vesper_done
        carrier = read_carrier()
        if carrier != self.carrier:
            self.carrier_since = cambc.round_number()
        self.carrier = carrier
        # Without a leader this round, last round's decision stands. The stamp is round + 1.
        now = cambc.round_number()
        self.update_rejoining(me, slot)
        self.led = stamp != now + 1 and self.can_lead(me, stale=stamp < now)
        if not self.led:
            self.mode, self.slot, self.aggressive = mode, slot, aggressive
            self.capture_slot, self.collector, self.claimed = capture_slot, collector, claimed
            return

        # First champion with the team this round settles the capture, picks the target from its own
        # respawn record, then scores. A dead carrier's escort is called off, as is one running too long.
        if self.carrier and (not self.carrier_alive() or now - self.carrier_since > ESCORT_TIMEOUT):
            print(f"round {now}: escort for champion {self.carrier - 1} called off")
            self.carrier = 0
        aggressive = vesper_done and team_alive()
        if aggressive != self.aggressive:
            print(f"round {cambc.round_number()}: aggressive mode {'on' if aggressive else 'off'}")
        self.aggressive = aggressive
        slot = slot % len(self.route) if self.route else 0
        if self.route:
            capture_slot, collector, claimed = self.update_capture(me, capture_slot, collector, claimed)
            # The escort altar is picked from the target, so the target holds while an escort runs.
            if not self.carrier:
                slot = self.pick_target(slot, me.position(), capture_slot)
        # Fighting outranks the escort; the escort outranks farming.
        new_mode = self.score_mode(self.centers[slot] if self.route else None)
        if new_mode == FARM and self.carrier:
            new_mode = TRIBUTE
        if (new_mode, slot) != (self.mode, self.slot):
            print(f"round {cambc.round_number()}: {MODE_NAMES[new_mode]}, route slot {slot}")
        self.mode, self.slot = new_mode, slot
        self.capture_slot, self.collector, self.claimed = capture_slot, collector, claimed
        write_shared(self.mode, self.slot, capture_slot, collector, claimed, vesper_done, aggressive,
                     self.carrier)

    def is_collector(self, me):
        return self.capture_slot != NO_CAPTURE and self.collector == int(me.champion_name()) + 1

    def vesper_claim(self, me):
        # The leader may not see Vesper, so she claims nearby loot herself; the old collector moves on.
        if (me.champion_name() != Champ.VESPER_VALE or emblem_count(me) >= VESPER_EMBLEM_CAP
                or self.is_collector(me) or under_threat(me) or not self.route):
            return
        position = me.position()
        slot = self.capture_slot
        if slot == NO_CAPTURE or slot >= len(self.route):
            slot = min(self.rewards, key=lambda s: distance(position, self.centers[s]), default=NO_CAPTURE)
        if slot == NO_CAPTURE or distance(position, self.centers[slot]) > VESPER_CLAIM_RANGE:
            return
        self.rewards.discard(slot)
        now = cambc.round_number()
        self.capture_slot, self.collector, self.claimed = slot, int(Champ.VESPER_VALE) + 1, now
        cambc.shared_memory.write(CAPTURE_SLOT_BYTE, bytes([slot, self.collector]))
        cambc.shared_memory[CLAIM_ROUND_OFFSET] = now & 0xFF
        print(f"round {now}: vesper takes route slot {slot}")

    def collect(self, me):
        # Stand on the cleared group until its reward lands, then release the claim for everyone.
        index = self.capture_slot
        if index >= len(self.route):
            return False
        if self.ready_at[index] > cambc.round_number():
            cambc.shared_memory[CAPTURE_SLOT_BYTE] = NO_CAPTURE
            self.capture_slot = NO_CAPTURE
            return False
        if self.camps_at(index):
            return False
        move_towards_closest(self.spawns[index])
        return True

    def score_mode(self, origin):
        # Lanchester-style comparison of the fight around the nearest enemies: each side's total damage
        # per round times its total health. Allies too far from the fight don't count (no staggered
        # engages), unseen living enemies are assumed nearby, and the bar to start a fight is higher
        # than the bar to keep one going so the team stops flip-flopping between fight and retreat.
        ratio = self.fight_ratio(origin)
        if ratio is None:
            return FARM
        engaged = self.mode == FIGHT or (origin is None and self.local_mode == FIGHT)
        if not engaged and origin is None:
            # Already within an enemy's reach: running gives free hits, so the lower bar applies.
            position = cambc.me().position()
            engaged = any(distance(e.position(), position) <= e.attack_range() + 1 for e in T.enemies())
        bar = KEEP_FIGHTING if engaged else START_FIGHT
        return FIGHT if ratio >= bar else RETREAT

    def fight_ratio(self, origin):
        me = cambc.me()
        anchor = origin if origin is not None else me.position()
        enemies = [e for e in T.enemies() if distance(e.position(), anchor) <= ENGAGE_RANGE]
        if not enemies:
            return None
        front = min(enemies, key=lambda e: distance(e.position(), anchor)).position()
        allies = [a for a in T.allies() if distance(a.position(), front) <= ENGAGE_RANGE
                  and not channelling(a)]
        if me not in allies and not channelling(me) and distance(me.position(), front) <= ENGAGE_RANGE:
            allies.append(me)
        ours = fight_power(allies)
        theirs = fight_power(enemies)
        # Living enemies we can't see may be in the bushes next to this fight.
        hidden = max(0, self.enemies_alive() - len(T.enemies()))
        theirs *= 1 + HIDDEN_ENEMY_WEIGHT * hidden
        if game_map.spawn_region(Team.ALLY).is_set(me.position()):
            ours *= SPAWN_FIGHT_BONUS
        if self.spawn_zone.is_set(front):
            theirs *= SPAWN_FIGHT_BONUS
        if theirs <= 0:
            return 99.0
        return ours / theirs

    def enemies_alive(self):
        if getattr(self, "_alive_round", None) == cambc.round_number():
            return self._alive_count
        count = 0
        for enemy in cambc.champions(Team.ENEMY):
            try:
                count += bool(enemy.is_alive())
            except cambc.Error:
                count += 1
        self._alive_round, self._alive_count = cambc.round_number(), count
        return count

    def retreat_step(self, me, enemies, zone=None):
        # Get out of reach heading home; a ready dash is spent when a single step can't escape.
        origin = me.position()
        threats = []
        for e in enemies:
            try:
                threats.append((e.position(), e.attack_range() + 1))
            except cambc.Error:
                pass
        field, reached = self.home

        def danger(tile):
            hits = sum(1 for spot, reach in threats if distance(tile, spot) <= reach)
            if zone is not None and zone.is_set(tile):
                hits += 1
            return hits

        def key(tile):
            home = field.get(tile) if reached.is_set(tile) else 999
            return (danger(tile), home)

        blocked = blocked_tiles()
        here = key(origin)
        dash = ability_of(me, Kind.DASH)
        if here[0] > 0 and dash is not None and dash.is_ready():
            landings = tiles_around(origin, int(dash.info().range)) & ~blocked
            for tile in sorted(landings.positions(), key=key):
                if key(tile) >= here:
                    break
                if cast(dash, tile):
                    origin = me.position()
                    here = key(origin)
                    blocked = blocked_tiles()
                    break
        if not me.can_move():
            return False
        steps = (blocked.reachable_tiles(origin, me.base_stats().base_move_speed)).positions()
        best = min(steps, key=key, default=None)
        if best is None or key(best) >= here:
            return False
        try:
            moved = me.move_absolute(best)
        except cambc.Error:
            return False
        if moved:
            T.invalidate()
        return moved

    # Turn

    def play_turn(self):
        T.start(cambc.round_number())
        me = cambc.me()
        if not self.ready:
            self.setup()
        if not self.priority_set:
            me.set_priority(PRIORITY.get(me.champion_name(), 0))
            self.priority_set = True
        if me.is_alive():
            self.observe()
        self.update_emblems()
        self.sync_decision(me)

        # Dead champions still get a turn to shop.
        if not me.is_alive():
            self.buy_items(me)
            return
        self.update_carrier(me)

        # 1. Leave running channels alone.
        if me.is_recalling() or me.is_submitting_gems() or me.is_teleporting():
            return

        # Placing a ward costs no move, attack or cast, so it is tried every turn.
        self.place_ward(me)
        # An altar lock on an altar an enemy is tributing at cancels the channel; their bag stays exposed.
        self.use_altar_lock(me)

        # 2. On spawn: buy, stay until healed (spawn heals at turn end), then head out.
        position = me.position()
        if game_map.spawn_region(Team.ALLY).is_set(position):
            self.tributed = False
            self.buy_items(me)
            if me.health() < me.max_health():
                return
            # Healed: carry on with the normal decisions. A fixed "walk to the target" here fought the
            # banking walk whenever the way to the altar crossed our own spawn, trapping the champion.

        # 3. Recall when hurt and nothing can interrupt it; otherwise step out of reach first.
        if me.health() < me.max_health() * RECALL_HEALTH:
            zone = threat_zone(T.enemies(), include_camps=True)
            if not zone.is_set(position):
                try:
                    me.recall()
                    return
                except cambc.Error:
                    pass
            self.retreat_step(me, T.enemies(), zone)
            return

        # 4. The one champion collecting a cleared group stays on it unless something can hit it.
        self.vesper_claim(me)
        self.share_loot(me)
        fight_near = any(distance(e.position(), me.position()) <= ENGAGE_RANGE for e in T.enemies())
        if self.is_collector(me) and not under_threat(me) and not fight_near and self.collect(me):
            return

        # 5. Engaging outranks any tribute. The shared mode comes from the leader's vision, so enemies
        # in our own sight are scored here too. A carrier too hurt for the front line stays behind.
        gems = me.gem_count()
        if gems > 0:
            self.tributed = False
        mode = self.mode
        enemies = T.enemies()
        if enemies:
            # Our own sight decides; the leader may be too far away to see this fight.
            local = self.score_mode(None)
            if local != FARM or mode == FIGHT:
                mode = local
        self.local_mode = mode
        if gems >= ESCORT_GEMS and me.health() < FRONT_HEALTH and mode in (FIGHT, RETREAT):
            if self.retreat(me):
                return
        elif mode == FIGHT and self.fight(me):
            return
        # 6. A heavy bag goes to the altar with the team; everyone else escorts it there.
        mine = int(me.champion_name()) + 1
        if self.carrier == mine and self.carry(me):
            return
        if mode == TRIBUTE and self.carrier != mine and self.escort(me):
            return

        # 6b. Stay near an ally channelling a big tribute instead of wandering off without them.
        if mode == FARM and gems < GEM_BANK and self.guard_tribute(me):
            return

        # 7. Bank early and often: carried gems are lost on death and stolen by the killer, and a big
        # bag tributes no faster than several small ones. Vesper banks like everyone else.
        vesper = me.champion_name() == Champ.VESPER_VALE
        building = vesper and emblem_count(me) < VESPER_EMBLEM_CAP
        if gems > 0 and self.carrier != mine and self.should_bank(me, gems, position, mode):
            altars = self.safe_altars(me, gems)
            if altars != [] and self.bank_gems(me, altars):
                return

        # 8. Just tributed with a heavy purse: go shop if the walk home is longer than the recall.
        # A building Vesper never tributes, so she shops on gold alone; recall keeps carried gems.
        recall_gold = self.vesper_recall_gold(me) if vesper else RECALL_GOLD
        ready_to_shop = building or (self.tributed and gems == 0)
        if ready_to_shop and me.gold_count() >= recall_gold and self.recall_to_shop(me):
            return

        # 9-10. Follow the mode; a quiet farm turn may walk over to an uncovered ward spot.
        if mode == RETREAT and self.retreat(me):
            return
        if mode == FARM and self.ward_detour(me, gems):
            return
        self.farm(me)

    # Wards

    def open_ward_spots(self, me):
        # Uncovered spot indexes; any spot a visible allied ward already covers is marked for everyone.
        if getattr(self, "_ward_round", None) == cambc.round_number():
            return self._ward_open
        self._ward_round = cambc.round_number()
        self._ward_open = self._open_ward_spots(me)
        return self._ward_open

    def _open_ward_spots(self, me):
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
            tiles &= ~(terrain() | cambc.vision.ward_tiles())
            for tile in sorted(tiles.positions(), key=lambda t: distance(t, spot)):
                try:
                    if not me.place_ward(tile):
                        continue
                except cambc.Error:
                    continue
                self.ward_slot = index
                cambc.shared_memory[WARD_MASK_BYTE] = cambc.shared_memory[WARD_MASK_BYTE] | 1 << index
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
        return move_towards_closest(tiles_around(spot, cambc.constants.WARD_PLACEMENT_RADIUS))

    def altar_gap(self, position):
        field, reached = self.altar_steps
        return field.get(position) if reached.is_set(position) else 255

    def altar_gap_to(self, origin, altar):
        return distance(origin, altar)

    def should_bank(self, me, gems, position, mode):
        # Win now if this bag finishes the game.
        if cambc.global_submitted_gems(Team.ALLY) + gems >= cambc.constants.WINNING_GEMS:
            return True
        # The trip to the nearest altar is the price of banking: small bags only go when it is short.
        near = self.altar_gap(position)
        bar = min(MAX_BANK_BAR, max(GEM_BANK_MIN, BANK_PER_STEP * near))
        if gems >= bar:
            return True
        if gems >= NEAR_BANK_GEMS and near <= NEAR_ALTAR_STEPS:
            return True
        # Enemies about and not fighting them: secure what we carry if an altar is within reach.
        if gems >= DANGER_BANK_GEMS and mode != FIGHT and near <= DANGER_ALTAR_STEPS and T.enemies():
            return True
        idle = bool(self.route) and self.wait_at(self.slot % len(self.route), position) > 0
        return idle and gems >= max(IDLE_BANK, bar // 2)

    def safe_altars(self, me, gems):
        # Altars with no visible enemy close by and no enemy lock; big bags prefer our side of the map.
        enemies = T.enemies()
        try:
            locked = cambc.vision.altar_lock_tiles(Team.ENEMY)
        except cambc.Error:
            locked = cambc.Bitboard()
        origin = me.position()
        safe = []
        for altar in game_map.altars().positions():
            if locked.is_set(altar):
                continue
            if any(distance(e.position(), altar) <= ALTAR_DANGER for e in enemies):
                continue
            safe.append(altar)
        if gems >= ESCORT_GEMS and self.solo_altars:
            preferred = [a for a in safe if a in self.solo_altars]
            if preferred:
                return preferred
        if safe:
            # A "safe" altar much further than the nearest one costs more than it saves.
            nearest = min(self.altar_gap_to(origin, a) for a in game_map.altars().positions())
            close_safe = [a for a in safe if self.altar_gap_to(origin, a) <= nearest + SAFE_DETOUR]
            if close_safe:
                return close_safe
            # Only contested altars nearby: hold the bag unless it is too big to keep risking.
            return None if gems >= MAX_BANK_BAR else []
        # Nowhere is safe: the furthest altar from the enemies we see.
        altars = game_map.altars().positions()
        if not altars or not enemies:
            return None
        far = max(altars, key=lambda a: min(distance(a, e.position()) for e in enemies))
        return [far]

    def guard_tribute(self, me):
        # A visible ally channelling a big tribute nearby: hold close to its altar until it is banked.
        position = me.position()
        for ally in T.allies():
            if ally == me or distance(ally.position(), position) > GUARD_RANGE:
                continue
            try:
                if not ally.is_submitting_gems() or ally.gem_count() < GUARD_GEMS:
                    continue
            except cambc.Error:
                continue
            altars = [a for a in game_map.altars().positions() if distance(a, ally.position()) <= 1]
            if altars:
                return self.guard(me, altars[0])
        return False

    def use_altar_lock(self, me):
        # Cancel an enemy tribute by locking the altar it is channelling at.
        try:
            lock = me.consumables()[cambc.ConsumableType.ALTAR_LOCK]
            if lock.count() <= 0:
                return False
        except cambc.Error:
            return False
        position = me.position()
        reach = 2
        for enemy in T.enemies():
            try:
                if not enemy.is_submitting_gems():
                    continue
            except cambc.Error:
                continue
            for altar in game_map.altars().positions():
                if distance(altar, enemy.position()) > 1 or distance(altar, position) > reach:
                    continue
                try:
                    if cambc.vision.has_altar_lock(altar):
                        continue
                    if lock.use(altar):
                        print(f"round {cambc.round_number()}: champion {int(me.champion_name())} locks altar {altar.x},{altar.y}")
                        return True
                except cambc.Error:
                    continue
        return False

    def bank_gems(self, me, altars=None):
        # A locked altar is still tried: the engine turns the tribute into an attack on the lock.
        origin = me.position()
        goals = cambc.Bitboard()
        for altar in altars or game_map.altars().positions():
            if distance(origin, altar) <= cambc.constants.SUBMIT_GEMS_RANGE:
                try:
                    me.submit_gems(altar)
                    self.tributed = True
                    return True
                except cambc.Error:
                    continue
            goals |= tiles_around(altar, cambc.constants.SUBMIT_GEMS_RANGE)
        return move_towards_closest(goals)

    # Escorted tribute

    def carrier_alive(self):
        # Out-of-sight allies may not reveal their type; an unknown carrier is assumed alive.
        for ally in cambc.champions(Team.ALLY):
            try:
                if int(ally.champion_name()) + 1 == self.carrier:
                    return ally.is_alive()
            except cambc.Error:
                continue
        return True

    def update_carrier(self, me):
        # A heavy bag claims the single carrier field only when every altar is a contested one;
        # the carrier gives it up once home or banked.
        mine = int(me.champion_name()) + 1
        gems = me.gem_count()
        home = game_map.spawn_region(Team.ALLY).is_set(me.position())
        if self.carrier == mine:
            if gems == 0 or home or me.is_recalling():
                self.carrier, self.escort_wait = 0, 0
                write_carrier(0)
                print(f"round {cambc.round_number()}: champion {mine - 1} escort over")
            return
        building = me.champion_name() == Champ.VESPER_VALE and emblem_count(me) < VESPER_EMBLEM_CAP
        if (not self.carrier and gems >= ESCORT_GEMS and not building and not home
                and not self.rejoining and not me.is_recalling() and not self.solo_altars):
            self.carrier, self.escort_wait = mine, 0
            write_carrier(mine)
            print(f"round {cambc.round_number()}: champion {mine - 1} asks for an escort with {gems} gems")

    def escort_altar(self):
        # Every champion derives the same altar from the shared target, which holds during an escort.
        if not self.escort_altars or not self.route:
            return None
        spot = self.centers[self.slot % len(self.route)]
        return min(self.escort_altars, key=lambda a: (distance(a, spot), a.x, a.y))

    def carry(self, me):
        # Regroup first: walk to the nearest ally, or wait beside the altar where escorts gather.
        altar = self.escort_altar()
        if altar is None:
            return False
        others = [a for a in T.allies() if a != me and not channelling(a)]
        living = sum(1 for a in cambc.champions(Team.ALLY) if a != me and a.is_alive())
        escorts = sum(1 for a in others if within(me, a, ESCORT_RANGE))
        if escorts >= min(ESCORT_MIN, living) or self.escort_wait >= ESCORT_WAIT:
            return self.bank_gems(me, [altar])
        self.escort_wait += 1
        if others:
            nearest = min(others, key=lambda a: distance(me.position(), a.position()))
            move_towards_closest(tiles_around(nearest.position(), 1))
        else:
            move_towards_closest(tiles_around(altar, STAGING_RANGE))
        return True

    def escort(self, me):
        # Stay beside the walking carrier; once it channels (or is out of sight) hold the altar.
        altar = self.escort_altar()
        if altar is None:
            return False
        carrier = next((a for a in T.allies()
                        if int(a.champion_name()) + 1 == self.carrier), None)
        if carrier is not None and not carrier.is_submitting_gems():
            if not within(me, carrier, ESCORT_RANGE - 1):
                move_towards_closest(tiles_around(carrier.position(), ESCORT_RANGE - 1))
            return True
        building = me.champion_name() == Champ.VESPER_VALE and emblem_count(me) < VESPER_EMBLEM_CAP
        if (not building and me.gem_count() >= GEM_BANK
                and distance(me.position(), altar) <= PROTECT_RANGE and self.bank_gems(me, [altar])):
            return True
        return self.guard(me, altar)

    def guard(self, me, altar):
        # Farm free camps near the channelling carrier, never leaving protecting range of the altar.
        position = me.position()
        camps = [c for c in T.camps()
                 if c.kind() in FREE_KINDS and distance(c.position(), altar) <= PROTECT_RANGE]
        if camps:
            in_reach = [c for c in camps if within(me, c, me.attack_range())]
            target = lowest(in_reach) or min(camps, key=lambda c: distance(position, c.position()))
            self.engage(me, target, [], is_camp=True)
        elif distance(position, altar) > STAGING_RANGE:
            move_towards_closest(tiles_around(altar, STAGING_RANGE))
        return True

    def recall_to_shop(self, me):
        position = me.position()
        home = min(distance(position, tile) for tile in game_map.spawn_region(Team.ALLY).positions())
        if home <= cambc.constants.RECALL_TIME:
            return move_towards_closest(game_map.spawn_region(Team.ALLY))
        if threat_zone(T.enemies(), include_camps=True).is_set(position):
            return False
        try:
            me.recall()
            return True
        except cambc.Error:
            return False

    def fight(self, me):
        enemies = T.enemies()
        if not enemies:
            return False
        # Near their spawn, only chase what dies before it gets home (anything already in reach still gets
        # hit); with nothing left, leave.
        if self.spawn_zone.is_set(me.position()):
            enemies = [e for e in enemies if within(me, e, me.attack_range()) or self.catchable(me, e)]
            if not enemies:
                move_towards_closest(~self.spawn_zone)
                return True
        target = self.focus_target(me, enemies)
        self.engage(me, target, enemies, is_camp=False)
        return True

    def focus_target(self, me, enemies):
        # The whole team should hit the same enemy: the one that dies fastest among those we can reach soon.
        # Channelling enemies are stuck (and drop their bag), healers keep everyone else alive.
        reach = me.attack_range()
        near = [e for e in enemies if distance(me.position(), e.position()) <= reach + FOCUS_SLACK] or enemies

        def key(enemy):
            score = enemy.health()
            if channelling(enemy):
                score -= CHANNEL_FOCUS_BONUS
            try:
                if enemy.champion_name() == Champ.WILLOW:
                    score -= HEALER_FOCUS_BONUS
            except cambc.Error:
                pass
            # Already in reach beats a step away; position breaks ties so allies agree.
            return (score + (0 if within(me, enemy, reach) else OUT_OF_REACH_COST),
                    enemy.position().x, enemy.position().y)
        return min(near, key=key)

    def catchable(self, me, enemy):
        # Rounds for us to close in and land enough basic attacks (allies in reach help), against its walk home.
        position = enemy.position()
        allies = [a for a in T.allies()
                  if distance(a.position(), position) <= a.attack_range() + 1]
        if me not in allies:
            allies.append(me)
        damage = sum(a.strength() for a in allies)
        if damage <= 0:
            return False
        approach = max(0, distance(me.position(), position) - me.attack_range())
        rounds = approach + -(-enemy.health() // damage)
        return rounds <= self.spawn_steps.get(position)

    def retreat(self, me):
        enemies = T.enemies()
        zone = threat_zone(enemies, include_camps=False)
        if not zone.is_set(me.position()):
            self.farm(me, avoid=zone)
            return True
        if self.cornered(me, enemies):
            # Running only hands them free hits: trade back at the weakest enemy in reach.
            in_reach = [e for e in enemies if within(me, e, me.attack_range())]
            if in_reach:
                self.engage(me, self.focus_target(me, in_reach), enemies, is_camp=False)
                return True

        # Melee may swing at something already in range before stepping out.
        if me.attack_range() <= 1:
            for target in [*sorted(enemies, key=lambda e: e.health()), *self.group_camps()]:
                if attack_if_in_range(target):
                    break
        if not self.retreat_step(me, enemies, zone):
            target = lowest([e for e in enemies if within(me, e, me.attack_range())])
            if target is not None:
                attack_if_in_range(target)
        return True

    def cornered(self, me, enemies):
        # Being hit by something that keeps pace with us, with home still far away.
        field, reached = self.home
        position = me.position()
        home = field.get(position) if reached.is_set(position) else 999
        if home <= SAFE_HOME_STEPS:
            return False
        chasers = [e for e in enemies if distance(e.position(), position) <= e.attack_range()]
        if not chasers:
            return False
        dash = ability_of(me, Kind.DASH)
        if dash is not None and dash.is_ready():
            return False
        return True

    def group_camps(self):
        return self.camps_at(self.slot % len(self.route)) if self.route else []

    def camps_at(self, slot):
        spawns = self.spawns[slot]
        return [camp for camp in T.camps() if spawns.is_set(camp.spawn_position())]

    def farm(self, me, avoid=None):
        if not self.route:
            return
        slot = self.slot % len(self.route)
        camps = self.camps_at(slot)
        reach = me.attack_range()
        # Clusters have no strength, so a passing hit is free and should not be walked past.
        passing = [c for c in T.camps()
                   if c.strength() <= 0 and within(me, c, reach) and self.can_take_camp(c)]
        if passing and not any(within(me, c, reach) for c in camps):
            self.engage(me, lowest(passing), [], is_camp=True, avoid=avoid)
            return
        if not camps:
            spawns = self.spawns[slot]
            if self.ready_at[slot] > cambc.round_number():
                ring = cambc.Bitboard()
                for tile in spawns.positions():
                    ring.draw_chebyshev(tile, WAIT_RANGE)
                move_towards_closest(ring & ~spawns, avoid)
            else:
                move_towards_closest(spawns, avoid)
            return
        in_reach = [c for c in camps if within(me, c, reach)]
        target = lowest(in_reach) or min(camps, key=lambda c: distance(me.position(), c.position()))
        self.engage(me, target, [], is_camp=True, avoid=avoid)

    # Kits

    def engage(self, me, target, enemies, is_camp, avoid=None):
        # A target can leave our sight mid-turn (we moved, it was stunned in a bush...); stop quietly.
        try:
            if me.champion_name() == Champ.VESPER_VALE:
                self.engage_vesper(me, target, enemies, is_camp, avoid)
            elif me.attack_range() > 1:
                self.engage_ranged(me, target, enemies, is_camp, avoid)
            else:
                self.engage_melee(me, target, is_camp, avoid)
        except cambc.Error:
            pass

    def engage_melee(self, me, target, is_camp, avoid):
        kind = me.champion_name()
        reach = me.attack_range()

        if kind == Champ.VARG and under_threat(me):
            cast_self(ability_of(me, Kind.SHIELD))

        # Ren's stun excludes walking, so it is cast before any step and only on champions.
        if kind == Champ.REN and not is_camp:
            stun = ability_of(me, Kind.CC)
            if stun is not None and within(me, target, stun.info().range):
                cast(stun, target)
            if not alive([target]):
                return

        if (not is_camp and kind in (Champ.ISSAC, Champ.VENDETTA, Champ.REN)
                and distance(me.position(), target.position()) > reach + 1
                and self.ally_engaged(me, target)):
            dash_towards(me, target.position(), reach)

        if not within(me, target, reach):
            move_towards_closest(tiles_around(target.position(), reach), avoid)

        if attack_if_in_range(target) and kind in (Champ.ISSAC, Champ.VENDETTA):
            if alive([target]) and cast_self(ability_of(me, Kind.ATTACK_RESET)):
                attack_if_in_range(target)

    def ally_engaged(self, me, target):
        # Some other ally is already close enough to be hitting this target, or it is nearly dead.
        if target.health() < target.max_health() * 0.3:
            return True
        spot = target.position()
        return any(a != me and distance(a.position(), spot) <= a.attack_range() + 1 for a in T.allies())

    def engage_ranged(self, me, target, enemies, is_camp, avoid):
        kind = me.champion_name()
        reach = me.attack_range()
        gap = distance(me.position(), target.position())

        # Healing and the attack reset share Willow's one cast per turn.
        resets = kind == Champ.WILLOW and not self.heal_lowest_ally(me)
        if kind == Champ.CASSI:
            if not is_camp:
                self.cassi_stun(me, enemies)
            damage = ability_of(me, Kind.DAMAGE)
            if damage is not None and gap <= damage.info().range:
                cast(damage, target)
            if not alive([target]):
                return

        # Hold just outside camp range while no allied melee is closer to absorb the camp's attacks.
        if is_camp and self.should_hold_back(me, target):
            ring = tiles_around(target.position(), RANGED_CAMP_STANDOFF)
            ring &= ~tiles_around(target.position(), RANGED_CAMP_STANDOFF - 1)
            move_towards_closest(ring, avoid)
            return

        if gap <= reach:
            if attack_if_in_range(target) and resets and alive([target]):
                if cast_self(ability_of(me, Kind.ATTACK_RESET)):
                    attack_if_in_range(target)
        else:
            move_towards_closest(tiles_around(target.position(), reach), avoid)

    def should_hold_back(self, me, camp):
        # Clusters have no strength, so there is nothing to absorb.
        if camp.strength() <= 0:
            return False
        my_gap = distance(me.position(), camp.position())
        melees = [a for a in T.allies() if a != me and a.attack_range() <= 1]
        if not melees:
            return False
        return all(distance(a.position(), camp.position()) >= my_gap for a in melees)

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
        hurt = [a for a in T.allies()
                if a != me and a.health() < a.max_health() and within(me, a, heal.info().range)]
        target = min(hurt, key=lambda a: a.health() / a.max_health(), default=None)
        return target is not None and cast(heal, target)

    def engage_vesper(self, me, target, enemies, is_camp, avoid):
        # One ability per turn: attack reset doubles a hit; otherwise the dash closes in or kites out.
        reach = me.attack_range()
        if is_camp and self.should_hold_back(me, target):
            ring = tiles_around(target.position(), RANGED_CAMP_STANDOFF)
            ring &= ~tiles_around(target.position(), RANGED_CAMP_STANDOFF - 1)
            move_towards_closest(ring, avoid)
            return

        # Ranged walking uses up the attack, so dash straight into range when it is close enough.
        # Against champions she only dashes to tiles at full range that no enemy melee can reach.
        if is_camp:
            dashed = not within(me, target, reach) and dash_towards(me, target.position(), reach - 1)
        else:
            dashed = (not within(me, target, reach)
                      and dash_towards(me, target.position(), reach, avoid=melee_danger(enemies)))
        if not within(me, target, reach):
            if not dashed:
                goal_avoid = avoid
                if not is_camp:
                    danger = melee_danger(enemies)
                    goal_avoid = danger if avoid is None else (avoid | danger)
                move_towards_closest(tiles_around(target.position(), reach), goal_avoid)
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
        threats = [(e.position(), e.attack_range()) for e in alive(enemies)]
        if dash is None or not dash.is_ready() or not threats:
            return False
        aim = target.position() if alive([target]) else None

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
        if emblem_count(me) >= VESPER_EMBLEM_CAP:
            return VESPER_RECALL_GOLD
        try:
            counts = me.item_count()
        except cambc.Error:
            return VESPER_RECALL_GOLD
        build = set(SHOP_ORDER[Champ.VESPER_VALE]) | {SHOP_FALLBACK[Champ.VESPER_VALE]}
        loose = sum(counts[shard] * shard.cost() for shard in build)
        left = EMBLEM_WORTH * (VESPER_EMBLEM_CAP - emblem_count(me)) - loose
        return max(Item.POWER_SHARD_OF_STRENGTH.cost(), min(VESPER_RECALL_GOLD - self.batch_spent, left))

    def buy_items(self, me):
        kind = me.champion_name()
        before = me.gold_count()
        self.buy_lock(me)
        self.shop(me, kind)
        if kind == Champ.VESPER_VALE:
            self.batch_spent = (self.batch_spent + before - me.gold_count()) % VESPER_RECALL_GOLD

    def buy_lock(self, me):
        # 10 gold to cancel an enemy tribute; carried once the first emblem is done.
        try:
            if emblem_count(me) < 1 or me.consumables()[cambc.ConsumableType.ALTAR_LOCK].count() > 0:
                return False
            if me.can_buy(cambc.ConsumableType.ALTAR_LOCK):
                me.buy(cambc.ConsumableType.ALTAR_LOCK)
                return True
        except cambc.Error:
            pass
        return False

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
        if fallback is not None:
            buy(me, fallback, me.gold_count() // fallback.cost())
