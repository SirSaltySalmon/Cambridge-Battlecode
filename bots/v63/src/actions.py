"""Movement, attacks, casts, channels, and purchases."""
import cambc

from action_trace import _act, _attack_failed, _move_failed, _moved
from board import _no_step_reason, at, blocked_tiles, distance, goal_cover, tiles_around
from constants import Kind, Team


def move_towards_closest(goals, avoid=None):
    me = cambc.me()
    origin = me.position()
    if not me.can_move():
        _move_failed("cannot move")
        return False
    if goals.is_set(origin):
        _move_failed("already on goal")
        return False

    # Only free goals or occupied camp tiles can seed the field. Never mutate the caller's board.
    blocked = blocked_tiles()
    if avoid is not None:
        blocked |= avoid
        blocked.set(origin, False)
    raw = goals
    goals = goals & (~blocked | cambc.vision.camp_tiles())
    if goals.is_empty():
        _move_failed("no free goal; " + goal_cover(raw, blocked))
        return False
    dash = ability_of(me, Kind.DASH)
    if dash is not None and not dash.is_ready():
        dash = None
    try:
        if dash is None and avoid is None and goals.count() == 1 and not blocked.is_set(goals.positions()[0]):
            if me.move_towards(goals.positions()[0]):
                _moved("move")
                return True
            _move_failed("engine rejected move_towards")
            return False

        # Expand from the goals until our current tile is reached.
        field = cambc.DistanceField()
        reached = cambc.Bitboard()
        field.compute_to(blocked & ~goals, goals, tiles_around(origin, 0), reached)

        # Dash whenever it is ready (it leaves the walk free), then walk on from where it lands.
        dashed = dash is not None and dash_along(dash, origin, field, reached & ~blocked)
        if dashed:
            _moved("dash")
            origin = me.position()
            blocked = blocked_tiles() if avoid is None else (blocked_tiles() | avoid)
            blocked.set(origin, False)
            if goals.is_set(origin) or not me.can_move():
                return True

        # Choose a reachable step with a smaller distance to the goal.
        reachable = blocked.reachable_tiles(origin, me.base_stats().base_move_speed)
        step = min([origin, *(reachable & reached).positions()], key=field.get)
        if step != origin:
            if me.move_absolute(step):
                _moved("move")
                return True
            if dashed:
                return True
            _move_failed("engine rejected move_absolute; " + _no_step_reason(origin, reached))
            return False
        if dashed:
            return True
        _move_failed(_no_step_reason(origin, reached))
        return False
    except cambc.Error as error:
        _move_failed(f"move error {error}")
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
        if not me.can_attack():
            _attack_failed("attack on cooldown")
            return False
        if not target.is_visible() or target.health() <= 0:
            _attack_failed("target gone")
            return False
        if not me.attack_tiles().is_set(target.position()):
            _attack_failed("out of attack tiles")
            return False
        me.attack(target)
        _act("attack")
        return True
    except cambc.Error as error:
        _attack_failed(f"attack error {error}")
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
        used = ability.is_ready() and bool(ability.use(target))
    except cambc.Error:
        return False
    if used:
        _act("cast")
    return used


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
            _act("cast")
        return used
    return False


def spell_damage(ability):
    try:
        return float(ability.damage_amount())
    except (cambc.Error, TypeError, ValueError):
        return 0.0


def spell_places(target, is_camp):
    # Tiles an area spell can cover: the camp group, or the enemy champions, plus the focus.
    units = alive(cambc.vision.camps() if is_camp else cambc.vision.champions(Team.ENEMY))
    places = []
    seen = set()
    for unit in units:
        try:
            place = unit.position()
        except cambc.Error:
            continue
        key = (place.x, place.y)
        if key not in seen:
            seen.add(key)
            places.append(place)
    try:
        place = target.position()
    except cambc.Error:
        return places
    if (place.x, place.y) not in seen:
        places.append(place)
    return places


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


def aoe_aim(origin, places, reach, radius, focus):
    # More bodies first, then a centre that still covers the chosen target.
    if not places:
        return None, 0
    best = None
    best_key = None
    for center in aoe_centers(origin, places, reach, radius):
        hits = sum(1 for place in places if distance(center, place) <= radius)
        if hits <= 0:
            continue
        covers = focus is not None and distance(center, focus) <= radius
        key = (hits, covers, -distance(origin, center))
        if best_key is None or key > best_key:
            best = center
            best_key = key
    return best, (best_key[0] if best_key else 0)


def cast_aoe(ability, places, focus, require):
    if ability is None or not ability.is_ready():
        return False
    try:
        info = ability.info()
        center, hits = aoe_aim(cambc.me().position(), places, info.range, info.radius, focus)
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


def channel_left(me):
    try:
        return f"left {me.remaining_channel_time()}"
    except cambc.Error:
        return "left unread"


def _enemy_label(enemy):
    try:
        return f"c{int(enemy.champion_name())}{at(enemy.position())}"
    except (cambc.Error, AttributeError):
        try:
            return at(enemy.position())
        except (cambc.Error, AttributeError):
            return "enemy"


def channel_threat(me):
    # A basic move is one tile. An enemy already in range is hitting now.
    # One step out only matters when the stand outlasts next turn: a finish then
    # is gone by the time they arrive. The text is the reason for staying or aborting.
    try:
        left = me.remaining_channel_time()
        finishes_next_turn = left <= 1
    except cambc.Error:
        left = None
        finishes_next_turn = False
    left_text = "left unread" if left is None else f"left {left}"
    origin = me.position()
    nearest = None
    for enemy in alive(cambc.vision.champions(Team.ENEMY)):
        try:
            reach = enemy.attack_range()
            gap = distance(origin, enemy.position()) - reach
        except cambc.Error:
            continue
        if nearest is None or gap < nearest[0]:
            nearest = (gap, reach, enemy)
        if gap <= 0 or (gap <= 1 and not finishes_next_turn):
            who = _enemy_label(enemy)
            if gap <= 0:
                return enemy, f"{who} already in range, gap {gap} attack {reach}, {left_text}"
            return enemy, f"{who} one step out, gap {gap} attack {reach}, {left_text}"
    if nearest is None:
        return None, f"no visible enemy, {left_text}"
    gap, reach, enemy = nearest
    who = _enemy_label(enemy)
    if gap <= 1 and finishes_next_turn:
        return None, f"{who} gap {gap} attack {reach} arrives after finish, {left_text}"
    return None, f"nearest {who} gap {gap} attack {reach} needs more than a step, {left_text}"


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


def buy(me, item, count):
    if count <= 0:
        return False
    try:
        if me.can_buy(item, count):
            me.buy(item, count)
            _act("buy")
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
