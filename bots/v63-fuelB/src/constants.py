"""Tunables, shop builds, and the shared-memory layout."""
import cambc

Team, Champ, Kind, Item = cambc.Team, cambc.ChampionType, cambc.AbilityKind, cambc.ItemType

# Turn thresholds.
RECALL_HEALTH = 0.35
GEM_BANK = 40
WIN_GEMS = 1000
THREAT_RANGE = 4
# While the target is still respawning, bank any bag at least this big.
IDLE_BANK = 10
# Recall to shop after a tribute when carrying this much gold.
RECALL_GOLD = 150
# Vesper collects most loot, so her purse fills fast; a higher bar keeps her with the team.
VESPER_RECALL_GOLD = 600

# Route targeting: a new planned goal must beat the current one by this factor.
SWITCH_MARGIN = 1.25
# Route-group walking fields are built from fuel left at turn end, only while at least this much remains
# (one field on a 100x100 map costs ~0.7M; running out mid-build would eat into the next turn).
FIELD_FUEL_RESERVE = 1_000_000
# Wait this far from a respawning group; standing on its spawn tiles delays it.
WAIT_RANGE = 2
# Escorts guarding a channelling carrier only farm groups of these kinds.
FREE_KINDS = (cambc.CampType.WOLF, cambc.CampType.GEM_CLUSTER, cambc.CampType.GOLD_CLUSTER)
# Every group is a possible stop on the way, except monoliths until the team owns this many emblems.
MONOLITH_EMBLEMS = 12
# Visible loot this close is taken even if it is slightly off the path to the planned goal.
NEAR_LOOT = 8
# Extra walking tiles allowed for a stop to still count as being on the way.
ON_WAY_SLACK = 4
# A champion back from spawn rejoins the team once this close to the shared target; until then it can't lead.
TEAM_RANGE = 8
# A capture claim is dropped after this many rounds so a dead or lost collector gets replaced.
CAPTURE_TIMEOUT = 15
# Vesper's core build is four emblems; fallback purchases continue after the core is complete.
VESPER_CORE_EMBLEMS = 4
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
ESCORT_WAIT = 12
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

# Fight score.
FIGHT_THRESHOLD = 5
# Indecision guard: once fighting, the team only retreats when the score falls this far below the
# threshold, and a switch between fight and retreat holds for at least this many rounds.
FIGHT_HYSTERESIS = 10
MIN_HOLD = 3
OUTNUMBER_BONUS = 10
MISSING_BODY_PENALTY = 5
# Prefer farming during the respawn window after enough enemy champions are down.
KILL_WINDOW_DEAD_ENEMIES = {3: 2, 4: 2, 5: 3}
KILL_WINDOW_FIGHT_PENALTY = 20
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

# Ranged bodies hold at least this far from a camp while no melee is coming in to tank it.
# A camp whose own reach is longer (a monolith hits at 5) pushes the ring out past that reach.
RANGED_CAMP_STANDOFF = 4

# One family at a time until its emblem is done; the fallback shard is bought without limit.
_MELEE_STRENGTH = (Item.JUNGLE_SHARD_OF_STRENGTH, Item.MONSTER_SHARD_OF_STRENGTH,
                   Item.DURABILITY_SHARD_OF_HEALTH,)
_VARG_STRENGTH = (Item.JUNGLE_SHARD_OF_STRENGTH, Item.MONSTER_SHARD_OF_STRENGTH,
                  Item.BRUISER_SHARD_OF_STRENGTH,)                   
_VENDETTA_STRENGTH = (Item.JUNGLE_SHARD_OF_STRENGTH, Item.MONSTER_SHARD_OF_STRENGTH,
                   Item.POWER_SHARD_OF_STRENGTH)
_MELEE_FOCUS = (Item.JUNGLE_SHARD_OF_FOCUS, Item.BRUISER_SHARD_OF_FOCUS,
                Item.DURABILITY_SHARD_OF_FOCUS)
_RANGED_FOCUS = (Item.POWER_SHARD_OF_FOCUS, Item.MONSTER_SHARD_OF_FOCUS,
                 Item.DURABILITY_SHARD_OF_FOCUS)
# Ryth's passive turns bonus health into Strength, and Jungle cuts the camp hits he stands in.
_RYTH_HEALTH = (Item.JUNGLE_SHARD_OF_HEALTH, Item.DURABILITY_SHARD_OF_HEALTH,
                Item.BRUISER_SHARD_OF_HEALTH)
# Vesper Vale's kit is all basic attacks, so she builds Strength only
_VESPER_STRENGTH = (Item.MONSTER_SHARD_OF_STRENGTH, Item.POWER_SHARD_OF_STRENGTH,
 Item.DURABILITY_SHARD_OF_HEALTH, Item.POWER_SHARD_OF_HEALTH)
SHOP_ORDER = {
    Champ.VENDETTA: _VENDETTA_STRENGTH,
    Champ.VARG: _VARG_STRENGTH,
    Champ.ISSAC: _MELEE_STRENGTH,
    Champ.REN: _MELEE_FOCUS,
    Champ.CASSI: _RANGED_FOCUS,
    Champ.WILLOW: _RANGED_FOCUS,
    Champ.STELLAR: _RANGED_FOCUS,
    Champ.RYTH: _RYTH_HEALTH,
    Champ.VESPER_VALE: _VESPER_STRENGTH,
}
SHOP_FALLBACK = {
    Champ.VENDETTA: (Item.POWER_SHARD_OF_STRENGTH,),
    Champ.VARG: (Item.POWER_SHARD_OF_STRENGTH, Item.POWER_SHARD_OF_HEALTH),
    Champ.ISSAC: (Item.POWER_SHARD_OF_STRENGTH, Item.POWER_SHARD_OF_HEALTH),
    Champ.REN: (Item.DURABILITY_SHARD_OF_FOCUS,),
    Champ.CASSI: (Item.POWER_SHARD_OF_FOCUS,),
    Champ.WILLOW: (Item.POWER_SHARD_OF_FOCUS,),
    Champ.STELLAR: (Item.POWER_SHARD_OF_FOCUS,),
    Champ.RYTH: (Item.BRUISER_SHARD_OF_HEALTH,),
    Champ.VESPER_VALE: (
        Item.POWER_SHARD_OF_STRENGTH,
        Item.POWER_SHARD_OF_STRENGTH,
        Item.POWER_SHARD_OF_HEALTH,
    ),
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
