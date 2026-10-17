"""Champion kits: when to cast, dash, kite, or hold range."""
import cambc

import seen
from actions import (
    ability_of,
    alive,
    aoe_aim,
    attack_if_in_range,
    cast,
    cast_aoe,
    cast_self,
    channelling,
    dash_towards,
    lowest,
    move_towards_closest,
    spell_damage,
    spell_places,
    within,
)
from board import at, blocked_tiles, distance, tiles_around
from constants import NO_CAPTURE, RECALL_HEALTH, Champ, Kind, RANGED_CAMP_STANDOFF, Team
from state import under_threat


class Kits:
    # Kits

    def engage(self, me, target, enemies, is_camp, avoid=None):
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

        # Ren's stun excludes walking, so it is cast before any step and only on champions.
        if kind == Champ.REN and not is_camp:
            stun = ability_of(me, Kind.CC)
            if stun is not None and within(me, target, stun.info().range):
                cast(stun, target)
            if not alive([target]):
                return

        # Ryth's area stun excludes walking, so it is cast before the step. The reset is the other cast.
        stunned = kind == Champ.RYTH and self.ryth_stun(me, target, is_camp)
        if stunned and not alive([target]):
            return

        if (not is_camp and kind in (Champ.ISSAC, Champ.VENDETTA, Champ.REN)
                and distance(me.position(), target.position()) > reach + 1):
            dash_towards(me, target.position(), reach)

        if not within(me, target, reach) and not stunned:
            move_towards_closest(tiles_around(target.position(), reach), avoid)

        if (attack_if_in_range(target) and not stunned
                and kind in (Champ.ISSAC, Champ.VENDETTA, Champ.VARG, Champ.RYTH)):
            if alive([target]) and cast_self(ability_of(me, Kind.ATTACK_RESET)):
                attack_if_in_range(target)

    def engage_ranged(self, me, target, enemies, is_camp, avoid):
        kind = me.champion_name()
        reach = me.attack_range()
        gap = distance(me.position(), target.position())

        # Healing and the attack reset share Willow's one cast per turn.
        resets = kind == Champ.WILLOW and not self.heal_lowest_ally(me)
        if kind == Champ.CASSI:
            if not is_camp:
                self.cassi_stun(me, enemies)
            # The damage spell excludes walking, so it waits until we are already outside camp reach.
            if not self.backing_off_camp(me, target, is_camp, gap):
                damage = ability_of(me, Kind.DAMAGE)
                if damage is not None and gap <= damage.info().range:
                    cast(damage, target)
                if not alive([target]):
                    return

        # Stellar's spells exclude walking. Step out of a camp before spending the cast.
        cast_spell = False
        if kind == Champ.STELLAR and not self.backing_off_camp(me, target, is_camp, gap):
            cast_spell = self.stellar_cast(me, target, is_camp)
            if cast_spell and not alive([target]):
                return

        # Hold outside camp reach until an allied melee is closer and taking the hits.
        if is_camp and self.should_hold_back(me, target):
            self.hold_camp(me, target, avoid)
            return

        if gap <= reach:
            # The attack is attempted before the reset check, so a spell that already spent the turn skips it.
            if not cast_spell and attack_if_in_range(target) and (resets or kind == Champ.STELLAR) and alive([target]):
                if resets and cast_self(ability_of(me, Kind.ATTACK_RESET)):
                    attack_if_in_range(target)
        elif not cast_spell:
            move_towards_closest(tiles_around(target.position(), reach), avoid)

    def backing_off_camp(self, me, target, is_camp, gap):
        return is_camp and self.should_hold_back(me, target) and gap < self.camp_standoff(target)

    def camp_standoff(self, camp):
        # Just outside this camp's own reach, and never closer than the usual gap.
        try:
            reach = int(camp.attack_range())
        except (cambc.Error, AttributeError, TypeError, ValueError):
            reach = RANGED_CAMP_STANDOFF - 1
        return max(RANGED_CAMP_STANDOFF, reach + 1)

    def should_hold_back(self, me, camp):
        # Clusters have no strength, so there is nothing to absorb. Willow still takes those.
        if camp.strength() <= 0:
            return False
        if any(self.coming_to_camp(ally, camp) for ally in self.closer_melees(me, camp)):
            return False
        # Other ranged may open a camp with no tank in sight. A melee that then appears,
        # still further out or clearly passing by, pulls them back. Willow never opens one.
        melees = self.visible_melees(me)
        return me.champion_name() == Champ.WILLOW or bool(melees)

    def visible_melees(self, me):
        return [a for a in seen.champions(Team.ALLY) if a != me and a.attack_range() <= 1]

    def closer_melees(self, me, camp):
        my_gap = distance(me.position(), camp.position())
        return [a for a in self.visible_melees(me) if distance(a.position(), camp.position()) < my_gap]

    def camp_slot(self, camp):
        try:
            return self.slot_at.get(camp.spawn_position())
        except (cambc.Error, AttributeError, TypeError):
            return None

    def coming_to_camp(self, ally, camp):
        # Closer is not enough: a carrier, a collector, or a recall is going somewhere else,
        # and a last step that increased the gap is leaving. The first sighting has no step yet.
        if channelling(ally):
            return False
        try:
            if ally.is_teleporting():
                return False
            ident = int(ally.champion_name()) + 1
        except cambc.Error:
            return False
        if self.carrier and ident == self.carrier:
            return False
        slot = self.camp_slot(camp)
        if self.capture_slot != NO_CAPTURE and ident == self.collector and slot != self.capture_slot:
            return False
        prev = self.ally_at.get(ident)
        now = cambc.round_number()
        try:
            here = ally.position()
            camp_at = camp.position()
        except cambc.Error:
            return False
        if prev is not None and now - prev[0] <= 1:
            if distance(here, camp_at) > distance(prev[1], camp_at):
                return False
            try:
                hurt = ally.health() < ally.max_health() * RECALL_HEALTH
            except cambc.Error:
                hurt = False
            if hurt and self.ally_spawn_list:
                spawn_now = min(distance(here, tile) for tile in self.ally_spawn_list)
                spawn_then = min(distance(prev[1], tile) for tile in self.ally_spawn_list)
                if spawn_now < spawn_then:
                    return False
        elif slot is not None and self.route and slot != self.slot % len(self.route):
            # A side camp is not the shared target, so a first glance does not make them its tank.
            try:
                reach = int(camp.attack_range())
            except (cambc.Error, AttributeError, TypeError, ValueError):
                reach = RANGED_CAMP_STANDOFF - 1
            if distance(here, camp_at) > reach:
                return False
        return True

    def remember_allies(self):
        # Kept across the turn, then refreshed. Melees act first, so the new point is where they stepped.
        now = cambc.round_number()
        kept = {ident: seen for ident, seen in self.ally_at.items() if now - seen[0] <= 1}
        try:
            mine = int(cambc.me().champion_name()) + 1
        except cambc.Error:
            mine = None
        for ally in seen.champions(Team.ALLY):
            try:
                ident = int(ally.champion_name()) + 1
                if ident == mine:
                    continue
                pos = ally.position()
                kept[ident] = (now, cambc.Vec2(pos.x, pos.y))
            except (cambc.Error, AttributeError, TypeError, ValueError):
                continue
        self.ally_at = kept

    def hold_camp(self, me, camp, avoid):
        gap = self.camp_standoff(camp)
        danger = tiles_around(camp.position(), gap - 1)
        if me.champion_name() == Champ.WILLOW and self.willow_follow(me, camp, danger, avoid):
            return
        closer = self.closer_melees(me, camp)
        if closer and not any(self.coming_to_camp(ally, camp) for ally in closer):
            who = "willow" if me.champion_name() == Champ.WILLOW else "ranged"
            self.note(f"{who} standoff {gap} from camp, closer melee is going elsewhere")
        elif me.champion_name() == Champ.WILLOW:
            self.note(f"willow waits {gap} from camp, melee is not closer")
        else:
            self.note(f"ranged standoff {gap} from camp, melee is not closer")
        ring = tiles_around(camp.position(), gap) & ~danger
        move_towards_closest(ring, avoid)

    def willow_follow(self, me, camp, danger, avoid):
        # A visible ally still behind us is the team, so meet them. Someone already as close
        # as we are is arriving at this camp, and the ring is the regroup point.
        allies = [a for a in seen.champions(Team.ALLY) if a != me]
        if not allies:
            return False
        melees = [a for a in allies if a.attack_range() <= 1]
        anchor = min(melees or allies, key=lambda a: distance(me.position(), a.position()))
        if distance(anchor.position(), camp.position()) <= distance(me.position(), camp.position()):
            return False
        near = tiles_around(anchor.position(), 1) & ~danger
        if near.is_empty():
            return False
        try:
            name = int(anchor.champion_name())
        except cambc.Error:
            name = "?"
        self.note(f"willow follows c{name}{at(anchor.position())}, stays outside camp")
        move_towards_closest(near, avoid)
        return True

    def ryth_stun(self, me, target, is_camp):
        # Ground stun in radius one, range 4. One body is enough; a pack is taken when the centre covers it.
        places = spell_places(target, is_camp)
        focus = target.position()
        if cast_aoe(ability_of(me, Kind.AOE_DAMAGE), places, focus, 1):
            return True
        stun = ability_of(me, Kind.CC)
        if stun is None or not within(me, target, stun.info().range):
            return False
        if is_camp and not stun.can_target_camps():
            return False
        if stun.can_target_ground():
            return cast(stun, focus)
        return cast(stun, target)

    def stellar_cast(self, me, target, is_camp):
        # Single hit is 2x Focus. The area is 1.4x, so it wins once two bodies are inside it.
        single = ability_of(me, Kind.DAMAGE)
        area = ability_of(me, Kind.AOE_DAMAGE)
        places = spell_places(target, is_camp)
        focus = target.position()
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
                center, hits = aoe_aim(me.position(), places, info.range, info.radius, focus)
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
        return bool(single_ok and cast(single, target))

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
        hurt = [a for a in seen.champions(Team.ALLY)
                if a != me and a.health() < a.max_health() and within(me, a, heal.info().range)]
        target = min(hurt, key=lambda a: a.health() / a.max_health(), default=None)
        return target is not None and cast(heal, target)

    def engage_vesper(self, me, target, enemies, is_camp, avoid):
        # One ability per turn: attack reset doubles a hit; otherwise the dash closes in or kites out.
        reach = me.attack_range()
        if is_camp and self.should_hold_back(me, target):
            self.hold_camp(me, target, avoid)
            return

        # Ranged walking uses up the attack, so dash straight into range when it is close enough.
        dashed = not within(me, target, reach) and dash_towards(me, target.position(), reach - 1)
        if not within(me, target, reach):
            if not dashed:
                move_towards_closest(tiles_around(target.position(), reach), avoid)
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
