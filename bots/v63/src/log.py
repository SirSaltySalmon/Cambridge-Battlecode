"""The decision log printed at the end of each turn."""
import cambc
from cambc import game_map

import action_trace as trace
from board import at, distance, step_room
from constants import NO_CAPTURE, Team


class Log:
    # Decision log

    def branch(self, name, why):
        self._branch = name
        self._why = why

    def note(self, text):
        self._notes.append(text)

    def tile_flags(self, tile):
        flags = []
        if game_map.spawn_region(Team.ENEMY).is_set(tile):
            flags.append("enemy-spawn")
        if game_map.spawn_region(Team.ALLY).is_set(tile):
            flags.append("ally-spawn")
        if self.spawn_zone.is_set(tile):
            flags.append("chase-ring")
        return ",".join(flags) if flags else "open"

    def slot_text(self):
        if not self.route:
            return "s?"
        slot = self.slot % len(self.route)
        center = self.centers[slot]
        return f"s{slot}{at(center)} {self.tile_flags(center)}"

    def shared_tags(self):
        tags = ["aggr" if self.aggressive else "calm"]
        tags.append(f"car {self.carrier - 1}" if self.carrier else "car -")
        if self.capture_slot != NO_CAPTURE:
            tags.append(f"cap s{self.capture_slot}->c{self.collector - 1}")
        if self.rejoining:
            tags.append("rejoining")
        return " ".join(tags)

    def begin_trace(self):
        trace._trace_reset()
        self.shared_why = ""
        self.local_why = ""
        self._branch = ""
        self._why = ""
        self._notes = []
        self.score_why = ""

    def nearest_spawn(self, origin):
        d_ally = min((distance(origin, tile) for tile in self.ally_spawn_list), default=None)
        d_enemy = min((distance(origin, tile) for tile in self.enemy_spawn_list), default=None)
        return d_ally, d_enemy

    def in_spawn_probe(self, me):
        if not me.is_alive() or not self.ready:
            return False
        pos = me.position()
        return (game_map.spawn_region(Team.ENEMY).is_set(pos) or self.spawn_zone.is_set(pos))

    def where_text(self, me):
        if not me.is_alive():
            return "dead"
        pos = me.position()
        bits = [at(pos)]
        if game_map.spawn_region(Team.ENEMY).is_set(pos):
            bits.append("ON-ENEMY-SPAWN")
        if game_map.spawn_region(Team.ALLY).is_set(pos):
            bits.append("on-ally-spawn")
        if self.ready and self.spawn_zone.is_set(pos):
            bits.append("IN-CHASE-RING")
        if self.spawn_gap is not None:
            bits.append(f"gap {self.spawn_gap}")
            if self.spawn_gap <= 2:
                bits.append("SPAWNS-CLOSE")
        if self.ally_in_ring:
            bits.append(f"ally-in-ring {self.ally_in_ring}/{len(self.ally_spawn_list)}")
        return " ".join(bits)

    def outcome_chunks(self, me):
        acts = list(trace._trace_acts)
        moved = any(kind in ("move", "dash") for kind in acts)
        chunks = []
        if acts:
            chunks.append("ACT " + ",".join(acts))
        if not moved:
            holding = (self._branch.endswith(".hold") or self._branch.startswith("channel.")
                       or self._branch == "spawn.heal")
            if trace._trace_stall:
                reason = trace._trace_stall
            elif trace._trace_attack:
                reason = trace._trace_attack
            elif holding:
                reason = "chose to wait"
            elif acts:
                reason = "no step"
            else:
                reason = "no action attempted"
            chunks.append("IDLE " + reason)
        if me.is_alive() and (not moved or self.in_spawn_probe(me)):
            chunks.append(step_room(me.position()))
            d_ally, d_enemy = self.nearest_spawn(me.position())
            chunks.append(f"dA{d_ally} dE{d_enemy}")
        return chunks or ["IDLE no action attempted"]

    def log_turn(self, me):
        rnd = cambc.round_number()
        branch = self._branch or "unset"
        why = self._why or "no branch recorded"
        lines = [
            self.where_text(me),
            self.shared_why or "shared ?",
            f"{branch}: {why}",
            *self.outcome_chunks(me),
        ]
        if self.local_why:
            lines.append(self.local_why)
        lines.extend(f"note: {text}" for text in self._notes)
        for line in lines:
            print(f"r{rnd} {line}", flush=True)
