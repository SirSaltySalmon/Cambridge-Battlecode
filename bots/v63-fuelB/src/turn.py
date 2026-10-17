"""What a champion does on its turn, in order."""
import cambc

import seen
from action_trace import _act
from actions import channel_left, channel_threat, move_towards_closest, stop_channel
from constants import (
    Champ,
    ESCORT_GEMS,
    FARM,
    FIGHT,
    FRONT_HEALTH,
    GEM_BANK,
    IDLE_BANK,
    MODE_NAMES,
    PRIORITY,
    RECALL_GOLD,
    RECALL_HEALTH,
    RETREAT,
    TRIBUTE,
    Team,
    VESPER_CORE_EMBLEMS,
)
from state import emblem_count, threat_zone, under_threat


class Turn:
    # Turn

    def play_turn(self):
        me = cambc.me()
        self.begin_trace()
        try:
            self._play(me)
        finally:
            try:
                self.remember_allies()
            except Exception as error:
                print(f"ally memory error: {error}", flush=True)
            try:
                self.log_turn(me)
            except Exception as error:
                print(f"log error: {error}", flush=True)
            try:
                self.build_fields()
            except Exception as error:
                print(f"field build error: {error}", flush=True)

    def _play(self, me):
        self.channel_cut = False
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
            self.branch("dead.shop", "dead, buy from spawn shop")
            return
        self.update_carrier(me)

        # 1. Stay on a channel an enemy cannot hit. One step out is safe when it ends next turn.
        if me.is_recalling() or me.is_submitting_gems() or me.is_teleporting():
            if me.is_recalling():
                kind = "recall"
            elif me.is_submitting_gems():
                kind = "tribute"
            else:
                kind = "teleport"
            if kind == "teleport":
                self.branch("channel.teleport", f"teleport cannot be cancelled, {channel_left(me)}")
                return
            threat, why = channel_threat(me)
            if threat is None or not stop_channel(me):
                if threat is not None:
                    why = f"cancel rejected; {why}"
                self.branch(f"channel.{kind}", why)
                return
            self.channel_cut = True
            self.note(f"aborts {kind}: {why}")

        # Placing a ward costs no move, attack or cast, so it is tried every turn.
        self.place_ward(me)

        # 2. On spawn: buy, stay until healed (spawn heals at turn end), then head out.
        position = me.position()
        if seen.ally_spawn().is_set(position):
            self.tributed = False
            self.buy_items(me)
            if me.health() < me.max_health():
                self.branch("spawn.heal", "on ally spawn, not full, wait for heal")
                return
            if self.win_tribute and self.mode != FIGHT:
                if me.gem_count() > 0:
                    self.bank_gems(me, stem="win")
                else:
                    self.branch("win.tribute.wait", "no gems to submit; hold on spawn")
                return
            if self.route:
                self.branch("spawn.leave", f"full hp, leave toward {self.slot_text()}")
                move_towards_closest(self.spawns[self.slot % len(self.route)])
            else:
                self.branch("spawn.stay", "full hp but no route")
            return

        # 3. Recall when hurt and nothing can interrupt it; otherwise step out of reach first.
        if me.health() < me.max_health() * RECALL_HEALTH:
            zone = threat_zone(seen.champions(Team.ENEMY), include_camps=True)
            hurt = f"below {RECALL_HEALTH:.0%} health"
            if not self.channel_cut and not zone.is_set(position):
                try:
                    me.recall()
                    _act("recall")
                    self.branch("hurt.recall", f"{hurt}, tile not in threat")
                    return
                except cambc.Error as error:
                    self.note(f"recall rejected ({error})")
            why = ("channel just aborted, not recalling again this turn" if self.channel_cut
                   else "stepping out of threat")
            self.branch("hurt.dodge", f"{hurt}, {why}")
            move_towards_closest(~zone)
            return

        # 4. The one champion collecting a cleared group stays on it unless something can hit it.
        if not self.win_tribute:
            self.vesper_claim(me)
            self.share_loot(me)
            if self.is_collector(me) and under_threat(me):
                self.note(f"collector of slot {self.capture_slot} skipped, under threat")
            elif self.is_collector(me) and self.collect(me):
                return

        # 5. Engaging outranks any tribute. The shared mode comes from the leader's vision, so enemies
        # in our own sight are scored here too. A carrier too hurt for the front line stays behind.
        gems = me.gem_count()
        if gems > 0:
            self.tributed = False
        mode = self.mode
        enemies = seen.champions(Team.ENEMY)
        if enemies and mode in (FARM, TRIBUTE):
            personal = self.local_prev
            mode = self.score_mode(position, current=personal)
            self.local_why = (f"rescore shared {MODE_NAMES[self.mode]} with personal "
                              f"{MODE_NAMES[personal]}: {self.score_why}")
        self.local_prev = mode
        if self.win_tribute and mode != FIGHT:
            if gems > 0:
                self.bank_gems(me, stem="win")
            else:
                self.branch("win.tribute.wait", "team can win; no gems to submit")
            return
        if gems >= ESCORT_GEMS and me.health() < FRONT_HEALTH and mode in (FIGHT, RETREAT):
            if self.retreat(me):
                self._branch = "front." + self._branch
                self._why = f"heavy bag, too hurt for the front; {self._why}"
                return
        elif mode == FIGHT:
            if self.fight(me):
                return
            self.note("mode fight but no living enemy, fall through")
        # 6. A heavy bag goes to the altar with the team; everyone else escorts it there.
        mine = int(me.champion_name()) + 1
        if self.carrier == mine and self.carry(me):
            return
        if mode == TRIBUTE and self.carrier != mine:
            if self.escort(me):
                return
            self.note("tribute but escort declined, fall through")

        # 7. Bank a full bag, or a smaller one while the target is still respawning.
        # Vesper keeps farming instead of tributing until her build is done.
        vesper = me.champion_name() == Champ.VESPER_VALE
        building = vesper and emblem_count(me) < VESPER_CORE_EMBLEMS
        # A heavy bag goes alone to a solo altar; a locked one still works, as tributing breaks the lock.
        idle = bool(self.route) and self.wait_at(self.slot % len(self.route), position) > 0
        if not building and gems >= ESCORT_GEMS and self.carrier != mine:
            if self.solo_altars and self.bank_gems(me, self.solo_altars, stem="bank.solo"):
                return
        elif (not building and (gems >= GEM_BANK or (idle and gems >= IDLE_BANK))
                and self.bank_gems(me)):
            return

        # 8. Return to shop at the gold threshold; Vesper can recall with gems still carried.
        recall_gold = self.vesper_recall_gold(me) if vesper else RECALL_GOLD
        ready_to_shop = vesper or (self.tributed and gems == 0)
        if me.gold_count() >= recall_gold:
            if not ready_to_shop:
                self.note(f"purse at recall bar but shop gated "
                          f"(vesper {vesper}, tributed {self.tributed}, building {building})")
            elif self.recall_to_shop(me):
                self._why = f"purse at recall bar; {self._why}"
                return
            else:
                self.note("purse at recall bar but recall refused")

        # 9-10. Follow the mode; a quiet farm turn may walk over to an uncovered ward spot.
        if mode == RETREAT and self.retreat(me):
            return
        if mode == FARM and self.ward_detour(me, gems):
            return
        if mode not in (FARM, RETREAT):
            self.note(f"mode {MODE_NAMES[mode]} reached farm fallback")
        self.farm(me)
