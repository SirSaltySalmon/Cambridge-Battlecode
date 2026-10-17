"""Map setup, the shared route, and the fight-or-farm score."""
import cambc
from cambc import game_map

import draft
import seen
from actions import channelling, move_towards_closest
from board import distance, steps_to, tiles_around, walk_field
from constants import (
    CAPTURE_SLOT_BYTE,
    CAPTURE_TIMEOUT,
    CENTRAL_ALTAR_RANGE,
    CLAIM_ROUND_OFFSET,
    Champ,
    DISTANCE_PENALTY,
    EMBLEM_BONUS,
    ESCORT_TIMEOUT,
    FARM,
    FIELD_FUEL_RESERVE,
    FIGHT,
    FIGHT_HYSTERESIS,
    FIGHT_THRESHOLD,
    HEALTH_EDGE,
    KILL_WINDOW_DEAD_ENEMIES,
    KILL_WINDOW_FIGHT_PENALTY,
    MIN_HOLD,
    MISSING_BODY_PENALTY,
    MODE_NAMES,
    MONOLITH_EMBLEMS,
    NEAR_LOOT,
    NO_CAPTURE,
    ON_WAY_SLACK,
    OUTNUMBER_BONUS,
    OVEREXTEND_PENALTY,
    POWER_EDGE,
    RETREAT,
    SLOT_BYTE,
    SPAWN_BONUS,
    SPAWN_CHASE_RANGE,
    SQUISHY_HEALTH,
    SWITCH_MARGIN,
    TEAM_RANGE,
    TRIBUTE,
    Team,
    VESPER_CLAIM_RANGE,
    VESPER_CORE_EMBLEMS,
    WIN_GEMS,
)
from state import (
    emblem_count,
    gems_carried,
    item_worth,
    mark_vesper_done,
    read_carrier,
    read_shared,
    team_alive,
    under_threat,
    write_shared,
)
from warding import ward_spots


class Route:
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
        in_plan = set(planned)
        extras = [index for index, group in enumerate(groups)
                  if index not in in_plan and group.camps()]
        indexes = (planned + extras)[:NO_CAPTURE]
        slot_of = {index: slot for slot, index in enumerate(indexes)}
        normal_slots = [slot_of[index] for index in normal if index in slot_of]
        aggressive_slots = [slot_of[index] for index in aggressive if index in slot_of] or normal_slots
        self.cores = (normal_slots, aggressive_slots)

        self.route = [groups[index] for index in indexes]
        self.spawns = [group.spawn_tiles() for group in self.route]
        self.spawn_lists = [spawns.positions() for spawns in self.spawns]
        self.slot_at = {tile: slot for slot, tiles in enumerate(self.spawn_lists) for tile in tiles}
        self.route_tiles = cambc.Bitboard()
        for spawns in self.spawns:
            self.route_tiles |= spawns
        self.monoliths = {slot for slot, group in enumerate(self.route)
                          if any(camp.kind() == cambc.CampType.MONOLITH for camp in group.camps())}
        self.centers = [draft.group_center(group) for group in self.route]
        self.values = (
            [draft.group_value(group, draft.FARM_GOLD_WEIGHT) for group in self.route],
            [draft.group_value(group) for group in self.route],
        )
        self.clears = [draft.clear_rounds(group) for group in self.route]
        self.ready_at = [0] * len(self.route)
        self.ward_spots = ward_spots()
        self.split_altars()
        # The tiles close enough to the enemy spawn to stop chasing; walking steps to it are built on first use.
        theirs = game_map.spawn_region(Team.ENEMY)
        self.spawn_steps = None
        self.spawn_zone = cambc.Bitboard()
        for tile in theirs.positions():
            self.spawn_zone.draw_chebyshev(tile, SPAWN_CHASE_RANGE)
        self.ally_spawn_list = list(game_map.spawn_region(Team.ALLY).positions())
        self.enemy_spawn_list = list(theirs.positions())
        self.spawn_gap = min((distance(a, b) for a in self.ally_spawn_list for b in self.enemy_spawn_list),
                             default=None)
        self.ally_in_ring = sum(1 for tile in self.ally_spawn_list if self.spawn_zone.is_set(tile))
        self.ready = True

    def steps_to_enemy_spawn(self, position):
        # Walking steps from a tile to the enemy spawn; the field costs ~0.7M fuel, so it waits for a chase.
        if self.spawn_steps is None:
            self.spawn_steps = cambc.DistanceField()
            self.spawn_steps.compute(seen.terrain(), game_map.spawn_region(Team.ENEMY), cambc.Bitboard())
        return self.spawn_steps.get(position)

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
        # One mask over every route spawn tile finds the groups in sight without a pass over all of them.
        visible = cambc.vision.visible_tiles() & self.route_tiles
        if visible.is_empty():
            return
        now = cambc.round_number()
        for index in {self.slot_at[tile] for tile in visible.positions()}:
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
        # A champion's type never changes, so each ally's key is read once.
        allies = seen.roster(Team.ALLY)
        keys = self.ally_keys
        if len(keys) != len(allies):
            keys[:] = [None] * len(allies)
        for index, ally in enumerate(allies):
            key = keys[index]
            if key is None:
                try:
                    key = keys[index] = int(ally.champion_name())
                except cambc.Error:
                    continue
            self.emblems[key] = max(self.emblems.get(key, 0), emblem_count(ally))

    def team_emblems(self):
        # Items are only readable in vision, so each champion keeps the most it has seen.
        # Vesper's done flag means she finished her six even if nobody else can see her bag.
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

    def group_field(self, slot):
        walk = self.goal_fields.get(slot)
        if walk is None:
            walk = self.goal_fields[slot] = walk_field(self.spawns[slot])
        return walk

    def build_fields(self):
        # Each route group's walking field costs up to ~0.7M fuel, so they are built one by one from fuel
        # left over at the end of a turn. Once all exist, travel() reads them instead of searching the map.
        if not self.ready or len(self.goal_fields) == len(self.route):
            return
        for slot in range(len(self.route)):
            if cambc.fuel_remaining() < FIELD_FUEL_RESERVE:
                return
            self.group_field(slot)

    def gap(self, start, goal):
        key = (start, goal)
        if key not in self.gaps:
            self.gaps[key] = steps_to(self.group_field(goal), self.spawn_lists[start])
        return self.gaps[key]

    def mid_clear(self, slot):
        return any(camp.health() < camp.max_health() for camp in self.camps_at(slot))

    def visible_loot(self, skip):
        # Groups whose camps or leftover reward are in our own sight.
        loot = set(self.rewards)
        for camp in seen.camps():
            slot = self.slot_at.get(camp.spawn_position())
            if slot is not None:
                loot.add(slot)
        loot.discard(skip)
        now = cambc.round_number()
        return [s for s in loot if self.ready_at[s] <= now or s in self.rewards]

    def route_rate_steps(self, slot, steps):
        wait = max(0, self.ready_at[slot] - cambc.round_number() - steps)
        cycle = steps + wait + self.clears[slot] + cambc.constants.CAPTURE_TIME
        return self.values[self.aggressive][slot] / max(1, cycle)

    def best_goal(self, travel):
        # Planned pocket: gold-weighted while farming, the centre pocket once aggressive.
        cores = [s for s in self.cores[self.aggressive] if s in travel] or list(travel)
        return max(cores, key=lambda s: self.route_rate_steps(s, travel[s]))

    def travel(self, origin, skip):
        # Walking steps from origin to every allowed route group except skip. Walking is symmetric, so once
        # every group has its own field a lookup at origin replaces a fresh search of the whole map.
        travel = {}
        if len(self.goal_fields) == len(self.route):
            for s in range(len(self.route)):
                field, reached = self.goal_fields[s]
                if s != skip and reached.is_set(origin) and self.allowed(s):
                    travel[s] = field.get(origin)
            return travel
        walk = walk_field(tiles_around(origin, 0))
        for s in range(len(self.route)):
            if s != skip and self.allowed(s):
                steps = steps_to(walk, self.spawn_lists[s])
                if steps is not None:
                    travel[s] = steps
        return travel

    def pick_target(self, slot, origin, skip, seen_only=False):
        # Walk toward the planned goal, but take every visible group that sits on that walk
        # (or close enough that turning aside is cheap). A group already under attack is finished.
        # The group being collected is skipped so everyone else moves on. Monoliths wait until
        # the team is strong enough. With seen_only, only loot this champion can see is offered.
        if slot != skip and self.mid_clear(slot):
            return slot
        seen_loot = self.visible_loot(skip)
        if seen_only and not seen_loot:
            return slot
        travel = self.travel(origin, skip)
        if not travel:
            return slot

        dest = self.best_goal(travel)
        loot = [s for s in seen_loot if s in travel]
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
            return min(on_way, key=lambda s: (travel[s], -self.values[self.aggressive][s], s))

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
        if seen.ally_spawn().is_set(position):
            self.rejoining = True
        elif self.rejoining and self.route and distance(position, self.centers[slot % len(self.route)]) <= TEAM_RANGE:
            self.rejoining = False

    def lead_block(self, me, stamp, now):
        # Only a champion with the team settles the target: one on spawn or walking back from it would
        # pick from its own position and drag the shared target away from where the team is farming.
        # If nobody led last round the whole team is away, so anyone may re-pick from where they are.
        # The stamp is round + 1, so it matches this round once someone has already led.
        if stamp == now + 1:
            return "already settled this round"
        if not me.is_alive():
            return "dead"
        if channelling(me):
            return "channelling"
        if me.is_teleporting():
            return "teleporting"
        if stamp >= now and self.rejoining:
            return "rejoining, last decision still fresh"
        return None

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
        self.note(f"spots loot at slot {self.slot}")
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
        allies = [a for a in seen.champions(Team.ALLY) if not channelling(a)]
        if me not in allies:
            allies.append(me)
        builders = [a for a in allies if a.champion_name() == Champ.VESPER_VALE
                and emblem_count(a) < VESPER_CORE_EMBLEMS]
        # A building Vesper collects first; otherwise the poorest visible ally, so the team catches up evenly.
        pool = builders or allies
        chosen = min(pool, key=lambda a: (item_worth(a),
                                          min(distance(a.position(), t) for t in spawns)))
        print(f"round {now}: champion {int(chosen.champion_name())} collects route slot {slot}")
        return slot, int(chosen.champion_name()) + 1, now

    # Shared decision

    def sync_decision(self, me):
        mode, slot, capture_slot, collector, stamp, claimed, vesper_done, aggressive = read_shared()
        if mode != self.mode:
            self.mode_changed = cambc.round_number()
        if (not vesper_done and me.champion_name() == Champ.VESPER_VALE
            and emblem_count(me) >= VESPER_CORE_EMBLEMS):
            vesper_done = True
            mark_vesper_done()
        self.vesper_done = vesper_done
        carrier = read_carrier()
        if carrier != self.carrier:
            self.carrier_since = cambc.round_number()
        self.carrier = carrier
        self.win_tribute = (
            cambc.global_submitted_gems(Team.ALLY)
            + sum(gems_carried(ally) for ally in seen.roster(Team.ALLY))
            >= WIN_GEMS
        )
        # Without a leader this round, last round's decision stands. The stamp is round + 1.
        now = cambc.round_number()
        self.update_rejoining(me, slot)
        block = self.lead_block(me, stamp, now)
        self.led = block is None
        if not self.led:
            self.mode, self.slot, self.aggressive = mode, slot, aggressive
            self.capture_slot, self.collector, self.claimed = capture_slot, collector, claimed
            self.shared_why = f"follow {MODE_NAMES[self.mode]} {self.slot_text()} ({block}) {self.shared_tags()}"
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
        new_mode = self.score_mode(self.centers[slot] if self.route else None, current=mode)
        # A fresh switch between fight and retreat holds for a few rounds before it can flip back.
        hyst = None
        if {new_mode, mode} == {FIGHT, RETREAT} and now - self.mode_changed < MIN_HOLD:
            hyst = f"hysteresis {now - self.mode_changed}/{MIN_HOLD} keeps {MODE_NAMES[mode]}"
            new_mode = mode
        if new_mode != mode:
            self.mode_changed = now
        escort_note = None
        if new_mode == FARM and self.carrier:
            escort_note = f"escort c{self.carrier - 1} lifts farm to tribute"
            new_mode = TRIBUTE
        if self.win_tribute and new_mode != FIGHT:
            new_mode = TRIBUTE
        if (new_mode, slot) != (self.mode, self.slot):
            print(f"round {cambc.round_number()}: {MODE_NAMES[new_mode]}, route slot {slot}")
        self.mode, self.slot = new_mode, slot
        self.capture_slot, self.collector, self.claimed = capture_slot, collector, claimed
        write_shared(self.mode, self.slot, capture_slot, collector, claimed, vesper_done, aggressive,
                     self.carrier)
        bits = [f"lead {MODE_NAMES[self.mode]} {self.slot_text()}", self.score_why]
        if hyst:
            bits.append(hyst)
        if escort_note:
            bits.append(escort_note)
        bits.append(self.shared_tags())
        self.shared_why = " ".join(bits)

    def is_collector(self, me):
        return self.capture_slot != NO_CAPTURE and self.collector == int(me.champion_name()) + 1

    def vesper_claim(self, me):
        # The leader may not see Vesper, so she claims nearby loot herself; the old collector moves on.
        if (me.champion_name() != Champ.VESPER_VALE or emblem_count(me) >= VESPER_CORE_EMBLEMS
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
        self.note(f"vesper claims slot {slot}")
        print(f"round {now}: vesper takes route slot {slot}")

    def collect(self, me):
        # Stand on the cleared group until its reward lands, then release the claim for everyone.
        index = self.capture_slot
        if index >= len(self.route):
            return False
        if self.ready_at[index] > cambc.round_number():
            cambc.shared_memory[CAPTURE_SLOT_BYTE] = NO_CAPTURE
            self.capture_slot = NO_CAPTURE
            self.note(f"claim slot {index} cleared, group respawning")
            return False
        if self.camps_at(index):
            self.note(f"collector of slot {index} but camps still up, fall through")
            return False
        self.branch("collect.loot", f"claim slot {index}, walk onto the reward")
        move_towards_closest(self.spawns[index])
        return True

    def score_mode(self, origin, current=FARM):
        # Enemies further from origin (the target, or our own tile) count for less.
        enemies = seen.champions(Team.ENEMY)
        if not enemies:
            self.score_why = "no enemies -> farm"
            return FARM
        allies = seen.champions(Team.ALLY)

        score = 0.0
        parts = []
        if len(allies) > len(enemies):
            score += OUTNUMBER_BONUS
            parts.append(f"nums +{OUTNUMBER_BONUS} {len(allies)}v{len(enemies)}")
        else:
            pen = MISSING_BODY_PENALTY * (len(enemies) - len(allies))
            score -= pen
            parts.append(f"nums -{pen} {len(allies)}v{len(enemies)}")
        ally_health = sum(a.health() for a in allies)
        enemy_health = sum(e.health() for e in enemies)
        hp_edge = HEALTH_EDGE if ally_health > enemy_health else -HEALTH_EDGE
        score += hp_edge
        parts.append(f"hp {hp_edge:+} {ally_health:.0f}/{enemy_health:.0f}")
        ally_power = sum(a.strength() + a.focus() for a in allies)
        enemy_power = sum(e.strength() + e.focus() for e in enemies)
        power_edge = POWER_EDGE if ally_power > enemy_power else -POWER_EDGE
        score += power_edge
        parts.append(f"power {power_edge:+} {ally_power:.0f}/{enemy_power:.0f}")
        emblems = EMBLEM_BONUS * sum(emblem_count(a) for a in allies)
        score += emblems
        parts.append(f"emblems +{emblems}")
        if origin is not None:
            dist = DISTANCE_PENALTY * min(distance(e.position(), origin) for e in enemies)
            score -= dist
            parts.append(f"dist -{dist:.1f}")
        if self.squishy_overextended(allies, enemies):
            score -= OVEREXTEND_PENALTY
            parts.append(f"overextend -{OVEREXTEND_PENALTY}")
        if seen.ally_spawn().is_set(cambc.me().position()):
            score += SPAWN_BONUS
            parts.append(f"ally-spawn +{SPAWN_BONUS}")
        enemy_roster = seen.roster(Team.ENEMY)
        dead_enemies = sum(not enemy.is_alive() for enemy in enemy_roster)
        kill_window = KILL_WINDOW_DEAD_ENEMIES.get(len(enemy_roster))
        if kill_window is not None and dead_enemies >= kill_window:
            score -= KILL_WINDOW_FIGHT_PENALTY
            parts.append(f"kill-window -{KILL_WINDOW_FIGHT_PENALTY} ({dead_enemies}/{len(enemy_roster)} dead)")
        bar = FIGHT_THRESHOLD - FIGHT_HYSTERESIS if current == FIGHT else FIGHT_THRESHOLD
        choice = FIGHT if score >= bar else RETREAT
        self.score_why = f"{score:.1f}/{bar:g} {MODE_NAMES[choice]} (" + ", ".join(parts) + ")"
        return choice

    def squishy_overextended(self, allies, enemies):
        # Melee allies are the tanks. A squishy with none of them closer to the nearest enemy is in front.
        tanks = [ally for ally in allies if ally.attack_range() <= 1]
        for squishy in allies:
            if squishy.max_health() > SQUISHY_HEALTH:
                continue
            nearest = min(enemies, key=lambda enemy: distance(squishy.position(), enemy.position()))
            gap = distance(squishy.position(), nearest.position())
            if not any(distance(tank.position(), nearest.position()) < gap for tank in tanks):
                return True
        return False
