"""Champion decisions. main.py dispatches here after the draft."""
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

# Route targeting: a new target must beat the current one by this factor.
SWITCH_MARGIN = 1.25
# Wait this far from a respawning group; standing on its spawn tiles delays it.
WAIT_RANGE = 2
# A capture claim is dropped after this many rounds so a dead or lost collector gets replaced.
CAPTURE_TIMEOUT = 15

# Fight score.
FIGHT_THRESHOLD = 1
OUTNUMBER_BONUS = 10
MISSING_BODY_PENALTY = 5
HEALTH_EDGE = 5
POWER_EDGE = 5
GEM_WEIGHT = 0.04
EMBLEM_BONUS = 1
DISTANCE_PENALTY = 0.5

# Highest acts first, from the round after it is set.
PRIORITY = {
    Champ.CASSI: 6,
    Champ.ISSAC: 5,
    Champ.VENDETTA: 4,
    Champ.VARG: 3,
    Champ.WILLOW: 2,
    Champ.REN: 1,
}

# Ranged bodies hold this far from a camp while they are the nearest champion to it.
RANGED_CAMP_STANDOFF = 4

# One family at a time until its emblem is done; the fallback shard is bought without limit.
_MELEE_STRENGTH = (Item.JUNGLE_SHARD_OF_STRENGTH, Item.MONSTER_SHARD_OF_STRENGTH,
                   Item.BRUISER_SHARD_OF_STRENGTH)
_RANGED_FOCUS = (Item.JUNGLE_SHARD_OF_FOCUS, Item.MONSTER_SHARD_OF_FOCUS,
                 Item.DURABILITY_SHARD_OF_FOCUS)
SHOP_ORDER = {
    Champ.VENDETTA: _MELEE_STRENGTH,
    Champ.VARG: _MELEE_STRENGTH,
    Champ.ISSAC: _MELEE_STRENGTH,
    Champ.REN: _MELEE_STRENGTH,
    Champ.CASSI: _RANGED_FOCUS,
    Champ.WILLOW: _RANGED_FOCUS,
}
SHOP_FALLBACK = {
    Champ.VENDETTA: Item.POWER_SHARD_OF_STRENGTH,
    Champ.VARG: Item.POWER_SHARD_OF_STRENGTH,
    Champ.ISSAC: Item.POWER_SHARD_OF_STRENGTH,
    Champ.REN: Item.POWER_SHARD_OF_STRENGTH,
    Champ.CASSI: Item.POWER_SHARD_OF_FOCUS,
    Champ.WILLOW: Item.POWER_SHARD_OF_FOCUS,
}

FARM, FIGHT, RETREAT = 0, 1, 2
MODE_NAMES = ("farm", "fight", "retreat")

# Shared memory: mode, target slot, capture slot, collector champion + 1, round stamp, capture claim round.
# The stamp is round + 1 so zeroed memory reads as unwritten; capture slot NO_CAPTURE means nobody is collecting.
SHARED_LAYOUT = "<BBBBHH"
NO_CAPTURE = 255
CAPTURE_SLOT_BYTE = 2

EMBLEMS = tuple(i for i in range(Item.COUNT) if "EMBLEM" in Item(i).name)


def distance(a, b):
    return a.chebyshev_distance(b)


def tiles_around(position, radius):
    tiles = cambc.Bitboard()
    tiles.draw_chebyshev(position, radius)
    return tiles


def blocked_tiles():
    # Combine terrain and visible occupants using native bitboard operations.
    blocked = (game_map.walls() | game_map.holes() | game_map.altars()
               | game_map.spawn_region(Team.ENEMY) | cambc.vision.champion_tiles()
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
    try:
        if avoid is None and goals.count() == 1 and not blocked.is_set(goals.positions()[0]):
            return me.move_towards(goals.positions()[0])

        # Expand from the goals until our current tile is reached.
        field = cambc.DistanceField()
        field.compute_to(blocked & ~goals, goals, tiles_around(origin, 0), cambc.Bitboard())

        # Choose a reachable step with a smaller distance to the goal.
        reachable = blocked.reachable_tiles(origin, me.base_stats().base_move_speed)
        step = min([origin, *reachable.positions()], key=field.get)
        return step != origin and me.move_absolute(step)
    except cambc.Error:
        return False


def attack_if_in_range(target):
    # Make one basic attack if the target is already in attack range.
    me = cambc.me()
    try:
        if (not me.can_attack() or not target.is_visible() or target.health() <= 0
                or not me.attack_tiles().is_set(target.position())):
            return False
        me.attack(target)
        return True
    except cambc.Error:
        return False


def alive(units):
    living = []
    for unit in units:
        try:
            if unit.health() > 0:
                living.append(unit)
        except cambc.Error:
            pass
    return living


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
        return ability.is_ready() and bool(ability.use(target))
    except cambc.Error:
        return False


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


def gems_carried(champion):
    try:
        return champion.gem_count()
    except cambc.Error:
        return 0


def threat_zone(enemies, include_camps):
    zone = cambc.Bitboard()
    for enemy in enemies:
        zone.draw_chebyshev(enemy.position(), THREAT_RANGE)
    if include_camps:
        for camp in alive(cambc.vision.camps()):
            if camp.strength() > 0:
                zone.draw_chebyshev(camp.position(), camp.attack_range())
    return zone


def under_threat(me):
    # An enemy that can already hit us, or a damaged (awake) camp in its own range.
    position = me.position()
    for enemy in alive(cambc.vision.champions(Team.ENEMY)):
        if distance(position, enemy.position()) <= enemy.attack_range():
            return True
    for camp in alive(cambc.vision.camps()):
        if (camp.strength() > 0 and camp.health() < camp.max_health()
                and distance(position, camp.position()) <= camp.attack_range()):
            return True
    return False


def read_shared():
    data = bytearray(struct.calcsize(SHARED_LAYOUT))
    cambc.shared_memory.read(0, data)
    mode, slot, capture_slot, collector, stamp, claimed = struct.unpack(SHARED_LAYOUT, data)
    if stamp == 0:
        capture_slot = NO_CAPTURE
    return mode, slot, capture_slot, collector, stamp, claimed


def write_shared(mode, slot, capture_slot, collector, claimed):
    cambc.shared_memory.write(0, struct.pack(SHARED_LAYOUT, mode, slot, capture_slot, collector,
                                             cambc.round_number() + 1, claimed))


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


def dash_towards(me, target_position, reach):
    # Dash to the free tile closest to the target, if it leaves at most one walk to reach it.
    dash = ability_of(me, Kind.DASH)
    if dash is None or not dash.is_ready():
        return False
    origin = me.position()
    gap = distance(origin, target_position)
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
        self.centers = []
        self.values = []
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

    def setup(self):
        groups = game_map.camp_groups()
        _, members = draft.decode_pocket(bytes(cambc.DraftMemoryBuffer()))
        if not members or any(index >= len(groups) for index in members):
            _, members = draft.compute_pocket()
        self.route = [groups[index] for index in members] or list(groups)[:255]
        self.centers = [draft.group_center(group) for group in self.route]
        self.values = [draft.group_value(group) for group in self.route]
        self.clears = [draft.clear_rounds(group) for group in self.route]
        self.ready_at = [0] * len(self.route)
        self.ready = True

    def current_group(self):
        return self.route[self.slot % len(self.route)] if self.route else None

    # Route memory

    def observe(self):
        # Vision is not shared, so each champion keeps its own respawn record.
        seen = cambc.vision.visible_tiles()
        now = cambc.round_number()
        for index, group in enumerate(self.route):
            if (seen & group.spawn_tiles()).is_empty():
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

    def route_rate(self, slot, origin):
        travel = distance(origin, self.centers[slot])
        cycle = travel + self.wait_at(slot, origin) + self.clears[slot] + cambc.constants.CAPTURE_TIME
        return self.values[slot] / max(1, cycle)

    def pick_target(self, slot, origin, skip):
        # The group being collected is skipped so everyone else moves on.
        options = [s for s in range(len(self.route)) if s != skip] or [slot]
        best = max(options, key=lambda s: self.route_rate(s, origin))
        if slot == skip or self.route_rate(best, origin) > SWITCH_MARGIN * self.route_rate(slot, origin):
            return best
        return slot

    def update_capture(self, me, capture_slot, collector, claimed):
        # Returns the capture claim after dropping finished or stale ones and claiming a fresh reward.
        now = cambc.round_number()
        if capture_slot != NO_CAPTURE and (capture_slot >= len(self.route) or self.ready_at[capture_slot] > now
                                           or now - claimed > CAPTURE_TIMEOUT):
            capture_slot = NO_CAPTURE
        if capture_slot != NO_CAPTURE or not self.rewards:
            return capture_slot, collector, claimed

        slot = min(self.rewards, key=lambda s: distance(me.position(), self.centers[s]))
        # Only a fresh sighting can claim this group again, so a stale record can't re-send a collector.
        self.rewards.discard(slot)
        spawns = self.route[slot].spawn_tiles()
        allies = [a for a in alive(cambc.vision.champions(Team.ALLY)) if not channelling(a)]
        if me not in allies:
            allies.append(me)
        nearest = min(allies, key=lambda a: min(distance(a.position(), t) for t in spawns.positions()))
        print(f"round {now}: champion {int(nearest.champion_name())} collects route slot {slot}")
        return slot, int(nearest.champion_name()) + 1, now

    # Shared decision

    def sync_decision(self, me):
        mode, slot, capture_slot, collector, stamp, claimed = read_shared()
        if stamp == cambc.round_number() + 1 or not me.is_alive():
            self.mode, self.slot = mode, slot
            self.capture_slot, self.collector, self.claimed = capture_slot, collector, claimed
            return

        # First living champion this round settles the capture, picks the target from its own respawn
        # record, then scores.
        slot = slot % len(self.route) if self.route else 0
        if self.route:
            capture_slot, collector, claimed = self.update_capture(me, capture_slot, collector, claimed)
            slot = self.pick_target(slot, me.position(), capture_slot)
        new_mode = self.score_mode(slot)
        if (new_mode, slot) != (self.mode, self.slot):
            print(f"round {cambc.round_number()}: {MODE_NAMES[new_mode]}, route slot {slot}")
        self.mode, self.slot = new_mode, slot
        self.capture_slot, self.collector, self.claimed = capture_slot, collector, claimed
        write_shared(self.mode, self.slot, capture_slot, collector, claimed)

    def is_collector(self, me):
        return self.capture_slot != NO_CAPTURE and self.collector == int(me.champion_name()) + 1

    def collect(self, me):
        # Stand on the cleared group until its reward lands, then release the claim for everyone.
        index = self.capture_slot
        group = self.route[index]
        if self.ready_at[index] > cambc.round_number():
            cambc.shared_memory[CAPTURE_SLOT_BYTE] = NO_CAPTURE
            self.capture_slot = NO_CAPTURE
            return False
        if self.group_camps_of(group):
            return False
        move_towards_closest(group.spawn_tiles())
        return True

    def score_mode(self, slot):
        enemies = alive(cambc.vision.champions(Team.ENEMY))
        if not enemies:
            return FARM
        allies = alive(cambc.vision.champions(Team.ALLY))

        score = 0.0
        if len(allies) > len(enemies):
            score += OUTNUMBER_BONUS
        else:
            score -= MISSING_BODY_PENALTY * (len(enemies) - len(allies))
        ally_health = sum(a.health() for a in allies)
        enemy_health = sum(e.health() for e in enemies)
        score += HEALTH_EDGE if ally_health > enemy_health else -HEALTH_EDGE
        ally_power = sum(a.strength() + a.focus() for a in allies)
        enemy_power = sum(e.strength() + e.focus() for e in enemies)
        score += POWER_EDGE if ally_power > enemy_power else -POWER_EDGE
        score += GEM_WEIGHT * sum(gems_carried(e) for e in enemies)
        score += EMBLEM_BONUS * sum(emblem_count(a) for a in allies)
        if self.centers:
            camp = self.centers[slot]
            score -= DISTANCE_PENALTY * min(distance(e.position(), camp) for e in enemies)
        return FIGHT if score >= FIGHT_THRESHOLD else RETREAT

    # Turn

    def play_turn(self):
        me = cambc.me()
        if not self.ready:
            self.setup()
        if not self.priority_set:
            me.set_priority(PRIORITY.get(me.champion_name(), 0))
            self.priority_set = True
        if me.is_alive():
            self.observe()
        self.sync_decision(me)

        # Dead champions still get a turn to shop.
        if not me.is_alive():
            self.buy_items(me)
            return

        # 1. Leave running channels alone.
        if me.is_recalling() or me.is_submitting_gems() or me.is_teleporting():
            return

        # 2. On spawn: buy, stay until healed (spawn heals at turn end), then head out.
        position = me.position()
        if game_map.spawn_region(Team.ALLY).is_set(position):
            self.tributed = False
            self.buy_items(me)
            if me.health() < me.max_health():
                return
            group = self.current_group()
            if group is not None:
                move_towards_closest(group.spawn_tiles())
            return

        # 3. Recall when hurt and nothing can interrupt it; otherwise step out of reach first.
        if me.health() < me.max_health() * RECALL_HEALTH:
            zone = threat_zone(alive(cambc.vision.champions(Team.ENEMY)), include_camps=True)
            if not zone.is_set(position):
                try:
                    me.recall()
                    return
                except cambc.Error:
                    pass
            move_towards_closest(~zone)
            return

        # 4. The one champion collecting a cleared group stays on it unless something can hit it.
        if self.is_collector(me) and not under_threat(me) and self.collect(me):
            return

        # 5. Bank a full bag, or a smaller one while the target is still respawning.
        gems = me.gem_count()
        if gems > 0:
            self.tributed = False
        idle = bool(self.route) and self.wait_at(self.slot % len(self.route), position) > 0
        if (gems >= GEM_BANK or (idle and gems >= IDLE_BANK)) and self.bank_gems(me):
            return

        # 6. Just tributed with a heavy purse: go shop if the walk home is longer than the recall.
        if self.tributed and gems == 0 and me.gold_count() >= RECALL_GOLD and self.recall_to_shop(me):
            return

        # 7-9. Follow the shared mode.
        if self.mode == FIGHT and self.fight(me):
            return
        if self.mode == RETREAT and self.retreat(me):
            return
        self.farm(me)

    def bank_gems(self, me):
        # A locked altar is still tried: the engine turns the tribute into an attack on the lock.
        origin = me.position()
        goals = cambc.Bitboard()
        for altar in game_map.altars().positions():
            if distance(origin, altar) <= cambc.constants.SUBMIT_GEMS_RANGE:
                try:
                    me.submit_gems(altar)
                    self.tributed = True
                    return True
                except cambc.Error:
                    continue
            goals |= tiles_around(altar, cambc.constants.SUBMIT_GEMS_RANGE)
        return move_towards_closest(goals)

    def recall_to_shop(self, me):
        position = me.position()
        home = min(distance(position, tile) for tile in game_map.spawn_region(Team.ALLY).positions())
        if home <= cambc.constants.RECALL_TIME:
            return move_towards_closest(game_map.spawn_region(Team.ALLY))
        if threat_zone(alive(cambc.vision.champions(Team.ENEMY)), include_camps=True).is_set(position):
            return False
        try:
            me.recall()
            return True
        except cambc.Error:
            return False

    def fight(self, me):
        enemies = alive(cambc.vision.champions(Team.ENEMY))
        if not enemies:
            return False
        in_reach = [e for e in enemies if within(me, e, me.attack_range())]
        target = lowest(in_reach) or lowest(enemies)
        self.engage(me, target, enemies, is_camp=False)
        return True

    def retreat(self, me):
        enemies = alive(cambc.vision.champions(Team.ENEMY))
        zone = threat_zone(enemies, include_camps=False)
        if not zone.is_set(me.position()):
            self.farm(me, avoid=zone)
            return True

        # Melee may swing at something already in range before stepping out; nobody dashes in.
        if me.attack_range() <= 1:
            for target in [*sorted(enemies, key=lambda e: e.health()), *self.group_camps()]:
                if attack_if_in_range(target):
                    break
        if not move_towards_closest(~zone):
            target = lowest([e for e in enemies if within(me, e, me.attack_range())])
            if target is not None:
                attack_if_in_range(target)
        return True

    def group_camps(self):
        group = self.current_group()
        return [] if group is None else self.group_camps_of(group)

    def group_camps_of(self, group):
        spawns = group.spawn_tiles()
        return [camp for camp in alive(cambc.vision.camps()) if spawns.is_set(camp.spawn_position())]

    def farm(self, me, avoid=None):
        group = self.current_group()
        if group is None:
            return
        camps = self.group_camps()
        if not camps:
            spawns = group.spawn_tiles()
            if self.ready_at[self.slot % len(self.route)] > cambc.round_number():
                ring = cambc.Bitboard()
                for tile in spawns.positions():
                    ring.draw_chebyshev(tile, WAIT_RANGE)
                move_towards_closest(ring & ~spawns, avoid)
            else:
                move_towards_closest(spawns, avoid)
            return
        in_reach = [c for c in camps if within(me, c, me.attack_range())]
        target = lowest(in_reach) or min(camps, key=lambda c: distance(me.position(), c.position()))
        self.engage(me, target, [], is_camp=True, avoid=avoid)

    # Kits

    def engage(self, me, target, enemies, is_camp, avoid=None):
        if me.attack_range() > 1:
            self.engage_ranged(me, target, enemies, is_camp, avoid)
        else:
            self.engage_melee(me, target, is_camp, avoid)

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

        if (not is_camp and kind in (Champ.ISSAC, Champ.VENDETTA, Champ.REN)
                and distance(me.position(), target.position()) > reach + 1):
            dash_towards(me, target.position(), reach)

        if not within(me, target, reach):
            move_towards_closest(tiles_around(target.position(), reach), avoid)

        if attack_if_in_range(target) and kind in (Champ.ISSAC, Champ.VENDETTA):
            if alive([target]) and cast_self(ability_of(me, Kind.ATTACK_RESET)):
                attack_if_in_range(target)

    def engage_ranged(self, me, target, enemies, is_camp, avoid):
        kind = me.champion_name()
        reach = me.attack_range()
        gap = distance(me.position(), target.position())

        if kind == Champ.WILLOW:
            self.heal_lowest_ally(me)
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
            attack_if_in_range(target)
        else:
            move_towards_closest(tiles_around(target.position(), reach), avoid)

    def should_hold_back(self, me, camp):
        my_gap = distance(me.position(), camp.position())
        melees = [a for a in alive(cambc.vision.champions(Team.ALLY)) if a != me and a.attack_range() <= 1]
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
        hurt = [a for a in alive(cambc.vision.champions(Team.ALLY))
                if a != me and a.health() < a.max_health() and within(me, a, heal.info().range)]
        target = min(hurt, key=lambda a: a.health() / a.max_health(), default=None)
        return target is not None and cast(heal, target)

    # Shopping

    def buy_items(self, me):
        kind = me.champion_name()
        for shard in SHOP_ORDER.get(kind, ()):
            try:
                counts = me.item_count()
            except cambc.Error:
                return
            emblem = shard.result()
            if emblem is not None and counts[emblem] > 0:
                continue
            need = max(0, shard.combine_count() - counts[shard])
            afford = me.gold_count() // shard.cost()
            buy(me, shard, min(need, afford))
            if afford < need:
                return
        fallback = SHOP_FALLBACK.get(kind)
        if fallback is not None:
            buy(me, fallback, me.gold_count() // fallback.cost())
