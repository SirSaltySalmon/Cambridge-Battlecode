"""Banking gems, escorted tributes, and recalling to shop."""
import cambc
from cambc import game_map

import action_trace as trace
from action_trace import _act
from actions import alive, channelling, lowest, move_towards_closest, within
from board import at, distance, tiles_around
from constants import (
    Champ,
    ESCORT_GEMS,
    ESCORT_MIN,
    ESCORT_RANGE,
    ESCORT_WAIT,
    FREE_KINDS,
    GEM_BANK,
    PROTECT_RANGE,
    STAGING_RANGE,
    Team,
    VESPER_CORE_EMBLEMS,
)
from state import emblem_count, threat_zone, write_carrier


class Tribute:
    def bank_gems(self, me, altars=None, stem="bank"):
        # A locked altar is still tried: the engine turns the tribute into an attack on the lock.
        origin = me.position()
        goals = cambc.Bitboard()
        spots = list(altars or game_map.altars().positions())
        held = None
        for altar in spots:
            in_range = distance(origin, altar) <= cambc.constants.SUBMIT_GEMS_RANGE
            if not self.channel_cut and in_range:
                try:
                    me.submit_gems(altar)
                    self.tributed = True
                    _act("tribute")
                    self.branch(f"{stem}.submit", f"altar {at(altar)} in submit range")
                    return True
                except cambc.Error:
                    continue
            if self.channel_cut and in_range and held is None:
                held = altar
            goals |= tiles_around(altar, cambc.constants.SUBMIT_GEMS_RANGE)
        nearest = min(spots, key=lambda altar: distance(origin, altar), default=None)
        where = at(nearest) if nearest is not None else "none"
        if held is not None:
            why = (f"tribute just aborted, altar {at(held)} in submit range, "
                   f"walk instead of resubmitting")
        else:
            why = f"walk toward altar {where}"
        self.branch(f"{stem}.walk", why)
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
        building = me.champion_name() == Champ.VESPER_VALE and emblem_count(me) < VESPER_CORE_EMBLEMS
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
            self.note("carrier but no escort altar, fall through")
            return False
        others = [a for a in alive(cambc.vision.champions(Team.ALLY)) if a != me and not channelling(a)]
        living = sum(1 for a in cambc.champions(Team.ALLY) if a != me and a.is_alive())
        escorts = sum(1 for a in others if within(me, a, ESCORT_RANGE))
        need = min(ESCORT_MIN, living)
        if escorts >= need or self.escort_wait >= ESCORT_WAIT:
            ok = self.bank_gems(me, [altar], stem="carry")
            if ok:
                self._why = f"escorts {escorts}/{need}, waited {self.escort_wait}/{ESCORT_WAIT}; {self._why}"
            else:
                self.note(f"carry bank failed ({self._why})")
            return ok
        self.escort_wait += 1
        if others:
            nearest = min(others, key=lambda a: distance(me.position(), a.position()))
            self.branch("carry.regroup",
                        f"escorts {escorts}/{need}, waited {self.escort_wait}, walk to ally {at(nearest.position())}")
            move_towards_closest(tiles_around(nearest.position(), 1))
        else:
            self.branch("carry.stage",
                        f"escorts {escorts}/{need}, no ally in vision, stage {STAGING_RANGE} from altar {at(altar)}")
            move_towards_closest(tiles_around(altar, STAGING_RANGE))
        return True

    def escort(self, me):
        # Stay beside the walking carrier; once it channels (or is out of sight) hold the altar.
        altar = self.escort_altar()
        if altar is None:
            return False
        carrier = next((a for a in alive(cambc.vision.champions(Team.ALLY))
                        if int(a.champion_name()) + 1 == self.carrier), None)
        if carrier is not None and not carrier.is_submitting_gems():
            gap = distance(me.position(), carrier.position())
            try:
                cname = int(carrier.champion_name())
            except cambc.Error:
                cname = "?"
            if gap > ESCORT_RANGE - 1:
                self.branch("escort.follow",
                            f"carrier c{cname}{at(carrier.position())} gap {gap} > {ESCORT_RANGE - 1}")
                move_towards_closest(tiles_around(carrier.position(), ESCORT_RANGE - 1))
            else:
                self.branch("escort.hold", f"beside carrier c{cname} gap {gap} <= {ESCORT_RANGE - 1}")
            return True
        building = me.champion_name() == Champ.VESPER_VALE and emblem_count(me) < VESPER_CORE_EMBLEMS
        if (not building and me.gem_count() >= GEM_BANK
                and distance(me.position(), altar) <= PROTECT_RANGE and self.bank_gems(me, [altar], stem="escort")):
            self._why = f"carrier busy, bag ready within {PROTECT_RANGE}; {self._why}"
            return True
        return self.guard(me, altar)

    def guard(self, me, altar):
        # Farm free camps near the channelling carrier, never leaving protecting range of the altar.
        position = me.position()
        camps = [c for c in alive(cambc.vision.camps())
                 if c.kind() in FREE_KINDS and distance(c.position(), altar) <= PROTECT_RANGE]
        if camps:
            in_reach = [c for c in camps if within(me, c, me.attack_range())]
            target = lowest(in_reach) or min(camps, key=lambda c: distance(position, c.position()))
            where = "in range" if in_reach else "closing"
            self.branch("guard.camp",
                        f"free camp {at(target.position())} {where}, within {PROTECT_RANGE} of altar {at(altar)}")
            self.engage(me, target, [], is_camp=True)
        elif distance(position, altar) > STAGING_RANGE:
            self.branch("guard.close",
                        f"no free camp, {distance(position, altar)} from altar {at(altar)} > {STAGING_RANGE}")
            move_towards_closest(tiles_around(altar, STAGING_RANGE))
        else:
            self.branch("guard.hold",
                        f"no free camp, staged {distance(position, altar)} <= {STAGING_RANGE} from altar {at(altar)}")
        return True

    def recall_to_shop(self, me):
        position = me.position()
        home = min(distance(position, tile) for tile in game_map.spawn_region(Team.ALLY).positions())
        if home <= cambc.constants.RECALL_TIME:
            self.branch("shop.walk", f"spawn is {home} tiles, recall takes {cambc.constants.RECALL_TIME}")
            ok = move_towards_closest(game_map.spawn_region(Team.ALLY))
            if not ok:
                self.note(f"shop walk failed: {trace._trace_stall}")
            return ok
        if self.channel_cut:
            self.note(f"shop recall held, channel just aborted, spawn is {home} tiles away")
            return False
        if threat_zone(alive(cambc.vision.champions(Team.ENEMY)), include_camps=True).is_set(position):
            return False
        try:
            me.recall()
            _act("recall")
            self.branch("shop.recall",
                        f"spawn is {home} tiles > recall {cambc.constants.RECALL_TIME}, tile safe")
            return True
        except cambc.Error:
            return False
