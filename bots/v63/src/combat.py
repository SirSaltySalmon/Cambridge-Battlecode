"""Fighting, retreating, and clearing the current camp group."""
import cambc

from actions import alive, attack_if_in_range, channelling, lowest, move_towards_closest, within
from board import at, distance
from constants import Team, WAIT_RANGE
from state import threat_zone


class Combat:
    def fight(self, me):
        enemies = alive(cambc.vision.champions(Team.ENEMY))
        if not enemies:
            return False
        # Near their spawn, only chase what dies before it gets home (anything already in reach still gets
        # hit); with nothing left, leave.
        if self.spawn_zone.is_set(me.position()):
            kept = []
            lines = []
            for enemy in enemies:
                try:
                    name = f"c{int(enemy.champion_name())}"
                except cambc.Error:
                    name = "c?"
                if within(me, enemy, me.attack_range()):
                    kept.append(enemy)
                    lines.append(f"{name} keep in-range {at(enemy.position())}")
                    continue
                keep, text = self.chase_line(me, enemy)
                lines.append(text)
                if keep:
                    kept.append(enemy)
            detail = "; ".join(lines)
            enemies = kept
            if not enemies:
                self.branch("fight.leave", f"in chase ring, nothing catchable ({detail})")
                move_towards_closest(~self.spawn_zone)
                return True
        else:
            detail = "outside chase ring"
        # A tributing enemy is stuck in place with its bag, which the kill steals.
        in_reach = [e for e in enemies if within(me, e, me.attack_range())]
        target = (lowest([e for e in in_reach if channelling(e)]) or lowest(in_reach)
                  or lowest([e for e in enemies if channelling(e)]) or lowest(enemies))
        try:
            tname = f"c{int(target.champion_name())}"
        except cambc.Error:
            tname = "c?"
        closing = "in-range" if within(me, target, me.attack_range()) else "closing"
        chan = " channelling" if channelling(target) else ""
        self.branch("fight.hit", f"{closing} {tname}{at(target.position())}{chan} ({detail})")
        self.engage(me, target, enemies, is_camp=False)
        return True

    def catchable(self, me, enemy):
        # Rounds for us to close in and land enough basic attacks (allies in reach help), against its walk home.
        return self.chase_line(me, enemy)[0]

    def chase_line(self, me, enemy):
        position = enemy.position()
        try:
            name = f"c{int(enemy.champion_name())}"
        except cambc.Error:
            name = "c?"
        spot = at(position)
        allies = [a for a in alive(cambc.vision.champions(Team.ALLY))
                  if distance(a.position(), position) <= a.attack_range() + 1]
        if me not in allies:
            allies.append(me)
        damage = sum(a.strength() for a in allies)
        home = self.spawn_steps.get(position)
        if damage <= 0:
            return False, f"{name} drop no-damage home {home}r {spot}"
        approach = max(0, distance(me.position(), position) - me.attack_range())
        rounds = approach + -(-enemy.health() // damage)
        keep = rounds <= home
        word = "keep" if keep else "drop"
        return keep, f"{name} {word} kill {rounds}r home {home}r dmg {damage:.0f} {spot}"

    def retreat(self, me):
        enemies = alive(cambc.vision.champions(Team.ENEMY))
        zone = threat_zone(enemies, include_camps=False)
        if not zone.is_set(me.position()):
            self.farm(me, avoid=zone, stem="retreat")
            self._why = f"outside threat, {len(enemies)} enemies; {self._why}"
            return True

        # Melee may swing at something already in range before stepping out; nobody dashes in.
        swung = None
        if me.attack_range() <= 1:
            for target in [*sorted(enemies, key=lambda e: e.health()), *self.group_camps()]:
                if attack_if_in_range(target):
                    swung = target
                    break
        if not move_towards_closest(~zone):
            target = lowest([e for e in enemies if within(me, e, me.attack_range())])
            if target is not None and attack_if_in_range(target):
                swung = target
        swing = ""
        if swung is not None:
            try:
                swing = f", swung c{int(swung.champion_name())}{at(swung.position())}"
            except (AttributeError, cambc.Error):
                swing = ", swung"
        self.branch("retreat.out", f"inside threat of {len(enemies)}, step out{swing}")
        return True

    def group_camps(self):
        return self.camps_at(self.slot % len(self.route)) if self.route else []

    def camps_at(self, slot):
        spawns = self.spawns[slot]
        return [camp for camp in alive(cambc.vision.camps()) if spawns.is_set(camp.spawn_position())]

    def farm(self, me, avoid=None, stem="farm"):
        if not self.route:
            self.branch(f"{stem}.none", "no route")
            return
        slot = self.slot % len(self.route)
        camps = self.camps_at(slot)
        reach = me.attack_range()
        center = self.centers[slot]
        # Clusters have no strength, so a passing hit is free and should not be walked past.
        passing = [c for c in alive(cambc.vision.camps())
                   if c.strength() <= 0 and within(me, c, reach) and self.can_take_camp(c)]
        if passing and not any(within(me, c, reach) for c in camps):
            target = lowest(passing)
            self.branch(f"{stem}.pass", f"free cluster {at(target.position())} in range, slot camps are not")
            self.engage(me, target, [], is_camp=True, avoid=avoid)
            return
        if not camps:
            spawns = self.spawns[slot]
            flags = self.tile_flags(center)
            if self.ready_at[slot] > cambc.round_number():
                wait = self.ready_at[slot] - cambc.round_number()
                self.branch(f"{stem}.wait",
                            f"slot {slot}{at(center)} {flags} respawns in {wait}, stand {WAIT_RANGE} off")
                ring = cambc.Bitboard()
                for tile in spawns.positions():
                    ring.draw_chebyshev(tile, WAIT_RANGE)
                move_towards_closest(ring & ~spawns, avoid)
            else:
                self.branch(f"{stem}.walk",
                            f"slot {slot}{at(center)} {flags} is up, no camp visible, walk onto it")
                move_towards_closest(spawns, avoid)
            return
        in_reach = [c for c in camps if within(me, c, reach)]
        target = lowest(in_reach) or min(camps, key=lambda c: distance(me.position(), c.position()))
        where = "in range" if in_reach else "closing"
        self.branch(f"{stem}.camp",
                    f"slot {slot} camp {at(target.position())} {self.tile_flags(target.position())}, {where}")
        self.engage(me, target, [], is_camp=True, avoid=avoid)
