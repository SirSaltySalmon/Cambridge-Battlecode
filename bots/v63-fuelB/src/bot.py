"""Champion decisions. main.py dispatches here after the draft."""
from combat import Combat
from constants import FARM, NO_CAPTURE
from kits import Kits
from log import Log
from route import Route
from shop import Shop
from tribute import Tribute
from turn import Turn
from warding import Warding


class Bot(Turn, Route, Log, Warding, Tribute, Combat, Kits, Shop):
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
        # Each ally's emblem key, by roster index, once read.
        self.ally_keys = []
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
        self.win_tribute = False
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
        # Fixed anti-ward zone around each ward spot, and their union, built on first use.
        self.ward_zones = None
        self.ward_union = None
        # Set from spawn until we reach the shared target again; everyone starts on spawn.
        self.rejoining = True
        # Whether this champion settled the shared decision this round.
        self.led = False
        # Champion + 1 of the escorted carrier, 0 for none, and how long we have waited for escorts.
        self.carrier = 0
        self.escort_wait = 0
        self.carrier_since = 0
        # Round the shared mode last changed, and this champion's own last fight decision.
        self.mode_changed = 0
        self.local_prev = FARM
        # Altars a heavy bag may visit alone (far from the centre, on our side) and the escorted rest.
        self.solo_altars = []
        self.escort_altars = []
        # Set for the rest of a turn after a channel is cancelled, so it is not started again.
        self.channel_cut = False
        # Decision log for this turn: the shared plan, the branch taken, and why.
        self.shared_why = ""
        self.local_why = ""
        self._branch = ""
        self._why = ""
        self._notes = []
        self.score_why = ""
        # Last place each ally was seen, so a closer melee can be told from one walking off.
        self.ally_at = {}
        self.spawn_gap = None
        self.ally_in_ring = 0
        self.ally_spawn_list = []
        self.enemy_spawn_list = []
