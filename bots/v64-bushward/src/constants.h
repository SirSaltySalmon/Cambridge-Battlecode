// Tunables, shop builds, and the shared-memory layout.
#pragma once
#include <cambc.h>
#include <cstdint>
#include <span>

namespace bc = cambc;
using bc::vec2;
using Team = bc::team;
using Champ = bc::champion_type;
using Kind = bc::ability_kind;
using Item = bc::item_type;

// Turn thresholds.
inline constexpr double RECALL_HEALTH = 0.35;
inline constexpr int GEM_BANK = 40;
inline constexpr int WIN_GEMS = 1000;
inline constexpr int THREAT_RANGE = 4;
// While the target is still respawning, bank any bag at least this big.
inline constexpr int IDLE_BANK = 10;
// Recall to shop after a tribute when carrying this much gold.
inline constexpr int RECALL_GOLD = 150;
// Vesper collects most loot, so her purse fills fast; a higher bar keeps her with the team.
inline constexpr int VESPER_RECALL_GOLD = 600;

// Route targeting: a new planned goal must beat the current one by this factor.
inline constexpr double SWITCH_MARGIN = 1.25;
// Route-group walking fields are built from fuel left at turn end, only while at least this much remains.
// C++ gets 100k fuel per turn (Python 2.5M), so this is the Python 1M reserve scaled by 1/25.
inline constexpr std::uint64_t FIELD_FUEL_RESERVE = 40'000;
// Wait this far from a respawning group; standing on its spawn tiles delays it.
inline constexpr int WAIT_RANGE = 2;
// Every group is a possible stop on the way, except monoliths until the team owns this many emblems.
inline constexpr int MONOLITH_EMBLEMS = 12;
// Visible loot this close is taken even if it is slightly off the path to the planned goal.
inline constexpr int NEAR_LOOT = 8;
// Extra walking tiles allowed for a stop to still count as being on the way.
inline constexpr int ON_WAY_SLACK = 4;
// A champion back from spawn rejoins the team once this close to the shared target; until then it can't lead.
inline constexpr int TEAM_RANGE = 8;
// A capture claim is dropped after this many rounds so a dead or lost collector gets replaced.
inline constexpr int CAPTURE_TIMEOUT = 15;
// Vesper's core build is four emblems; fallback purchases continue after the core is complete.
inline constexpr int VESPER_CORE_EMBLEMS = 4;
// Vesper takes over a capture claim herself when the cleared group is this close to her.
inline constexpr int VESPER_CLAIM_RANGE = 8;
inline constexpr int CLAIM_ROUND_OFFSET = 6;

// Escorted tributes: a bag this big is only carried to an altar with the team around it.
inline constexpr int ESCORT_GEMS = 150;
// Escorts within this range of the carrier count; this many (or every living ally if fewer) must be there.
inline constexpr int ESCORT_RANGE = 3;
inline constexpr int ESCORT_MIN = 2;
// Waiting carriers stand this far from the escort altar, and give up waiting after this many turns.
inline constexpr int STAGING_RANGE = 5;
inline constexpr int ESCORT_WAIT = 12;
// While the carrier channels, escorts farm free camps this close to the altar.
inline constexpr int PROTECT_RANGE = 6;
// A carrier below this health stays behind the team instead of fighting at the front.
inline constexpr double FRONT_HEALTH = 800;
// An escort is dropped after this many rounds, in case the carrier died out of everyone's sight.
inline constexpr int ESCORT_TIMEOUT = 60;
// Altars this close to the map centre are contested, so heavy bags only go there escorted.
inline constexpr int CENTRAL_ALTAR_RANGE = 10;

// Wards: spots near the map centre and on a ring around the enemy spawn, at most one ward per spot.
inline constexpr int ANTI_WARD_RADIUS = 8;
inline constexpr int CENTRAL_WARDS = 2;
inline constexpr int ENEMY_SPAWN_WARDS = 3;
// Ring distance from the enemy spawn; ward vision is 6, so the ring sees the spawn exits.
inline constexpr int ENEMY_WARD_GAP = 7;
// A ward still covers its spot when it lands this close to it.
inline constexpr int WARD_SLACK = 1;
// Bush wards: a champion whose ward is still unused spends it once, on a bush it cannot see into in a
// key area: near an altar first, then near the map centre, then near a route camp. Never on a fight
// turn or with an enemy in sight, and never within BUSH_WARD_GAP of a visible allied ward.
inline constexpr int BUSH_ALTAR_RANGE = 6;
inline constexpr int BUSH_CENTER_RANGE = 10;
inline constexpr int BUSH_CAMP_RANGE = 4;
inline constexpr int BUSH_WARD_GAP = 4;
// A followed ward goes on a bush this close to the new target's centre.
inline constexpr int FOLLOW_BUSH_RANGE = 6;
// Fuel set aside for building the key-bush boards at a turn end.
inline constexpr std::uint64_t BUSH_BOARD_FUEL = 15'000;
// Walk to an uncovered spot this far away when the route leaves time; further on the aggressive route.
inline constexpr int WARD_DETOUR_RANGE = 6;
inline constexpr int AGGRESSIVE_WARD_DETOUR_RANGE = 12;

// Fight score.
inline constexpr int FIGHT_THRESHOLD = 5;
// Indecision guard: once fighting, the team only retreats when the score falls this far below the
// threshold, and a switch between fight and retreat holds for at least this many rounds.
inline constexpr int FIGHT_HYSTERESIS = 10;
inline constexpr int MIN_HOLD = 3;
inline constexpr int OUTNUMBER_BONUS = 10;
inline constexpr int MISSING_BODY_PENALTY = 5;
// Prefer farming during the respawn window after enough enemy champions are down (team size -> dead).
inline int kill_window_dead_enemies(std::size_t team) { return team == 3 || team == 4 ? 2 : team == 5 ? 3 : -1; }
inline constexpr int KILL_WINDOW_FIGHT_PENALTY = 20;
// Within this many tiles of the enemy spawn, only enemies that die before reaching it are chased.
inline constexpr int SPAWN_CHASE_RANGE = 8;
inline constexpr int HEALTH_EDGE = 5;
inline constexpr int POWER_EDGE = 5;
inline constexpr int EMBLEM_BONUS = 3;
inline constexpr double DISTANCE_PENALTY = 0.5;
// A champion under this max health is squishy, and fights less while ahead of every melee ally.
inline constexpr double SQUISHY_HEALTH = 900;
inline constexpr int OVEREXTEND_PENALTY = 10;
// Standing on our own spawn is safe: it heals us, so a fight scores higher.
inline constexpr int SPAWN_BONUS = 50;

// Highest acts first, from the round after it is set.
inline int priority_of(Champ kind) {
    switch (kind) {
    case Champ::cassi: return 6;
    case Champ::issac: return 5;
    case Champ::vendetta: return 4;
    case Champ::varg: return 3;
    case Champ::vesper_vale: return 2;
    case Champ::ren: return 1;
    default: return 0;
    }
}

// Ranged bodies hold at least this far from a camp while no melee is coming in to tank it.
// A camp whose own reach is longer (a monolith hits at 5) pushes the ring out past that reach.
inline constexpr int RANGED_CAMP_STANDOFF = 4;

// One family at a time until its emblem is done; the fallback shard is bought without limit.
inline constexpr Item MELEE_STRENGTH[] = {Item::jungle_shard_of_strength, Item::monster_shard_of_strength,
    Item::durability_shard_of_health};
inline constexpr Item VARG_STRENGTH[] = {Item::jungle_shard_of_strength, Item::monster_shard_of_strength,
    Item::bruiser_shard_of_strength};
inline constexpr Item VENDETTA_STRENGTH[] = {Item::jungle_shard_of_strength, Item::monster_shard_of_strength,
    Item::power_shard_of_strength};
inline constexpr Item MELEE_FOCUS[] = {Item::jungle_shard_of_focus, Item::bruiser_shard_of_focus,
    Item::durability_shard_of_focus};
inline constexpr Item RANGED_FOCUS[] = {Item::power_shard_of_focus, Item::monster_shard_of_focus,
    Item::durability_shard_of_focus};
// Ryth's passive turns bonus health into Strength, and Jungle cuts the camp hits he stands in.
inline constexpr Item RYTH_HEALTH[] = {Item::jungle_shard_of_health, Item::durability_shard_of_health,
    Item::bruiser_shard_of_health};
// Vesper Vale's kit is all basic attacks, so she builds Strength only
inline constexpr Item VESPER_STRENGTH[] = {Item::monster_shard_of_strength, Item::power_shard_of_strength,
    Item::durability_shard_of_health, Item::power_shard_of_health};

inline std::span<const Item> shop_order(Champ kind) {
    switch (kind) {
    case Champ::vendetta: return VENDETTA_STRENGTH;
    case Champ::varg: return VARG_STRENGTH;
    case Champ::issac: return MELEE_STRENGTH;
    case Champ::ren: return MELEE_FOCUS;
    case Champ::cassi: case Champ::willow: case Champ::stellar: return RANGED_FOCUS;
    case Champ::ryth: return RYTH_HEALTH;
    case Champ::vesper_vale: return VESPER_STRENGTH;
    default: return {};
    }
}

inline constexpr Item FALLBACK_VENDETTA[] = {Item::power_shard_of_strength};
inline constexpr Item FALLBACK_MELEE[] = {Item::power_shard_of_strength, Item::power_shard_of_health};
inline constexpr Item FALLBACK_REN[] = {Item::durability_shard_of_focus};
inline constexpr Item FALLBACK_RANGED[] = {Item::power_shard_of_focus};
inline constexpr Item FALLBACK_RYTH[] = {Item::bruiser_shard_of_health};
inline constexpr Item FALLBACK_VESPER[] = {Item::power_shard_of_strength, Item::power_shard_of_strength,
    Item::power_shard_of_health};

inline std::span<const Item> shop_fallback(Champ kind) {
    switch (kind) {
    case Champ::vendetta: return FALLBACK_VENDETTA;
    case Champ::varg: case Champ::issac: return FALLBACK_MELEE;
    case Champ::ren: return FALLBACK_REN;
    case Champ::cassi: case Champ::willow: case Champ::stellar: return FALLBACK_RANGED;
    case Champ::ryth: return FALLBACK_RYTH;
    case Champ::vesper_vale: return FALLBACK_VESPER;
    default: return {};
    }
}

inline constexpr int FARM = 0, FIGHT = 1, RETREAT = 2, TRIBUTE = 3;
inline constexpr const char* MODE_NAMES[] = {"farm", "fight", "retreat", "tribute"};

// Shared memory: mode, target slot, capture slot, collector champion + 1, round stamp (u16 LE), capture claim
// round (low byte only; it is just aged against CAPTURE_TIMEOUT), then the ward byte the leader never rewrites.
// The stamp is round + 1 so zeroed memory reads as unwritten; capture slot NO_CAPTURE means nobody is collecting.
// The mode byte's top bit records that Vesper has finished her build; only she can see her own items.
// The next bit records whether the leader put the team on the aggressive route this round.
// Bits 2-5 hold the escorted carrier's champion + 1 (0 for none); the carrier sets and clears it itself.
inline constexpr int SHARED_BYTES = 7;
// Bit i is set once ward spot i is covered, so nobody else spends a ward inside its anti-ward radius.
inline constexpr int WARD_MASK_BYTE = 7;
inline constexpr int NO_CAPTURE = 255;
inline constexpr int SLOT_BYTE = 1;
inline constexpr int CAPTURE_SLOT_BYTE = 2;
inline constexpr int MODE_BYTE = 0;
inline constexpr int VESPER_DONE_FLAG = 0x80;
inline constexpr int AGGRESSIVE_FLAG = 0x40;
inline constexpr int MODE_MASK = 0x03;
inline constexpr int CARRIER_SHIFT = 2;
inline constexpr int CARRIER_MASK = 0x3C;

// Items alternate shard, emblem: every odd item index is an emblem, every even one a shard.
inline constexpr int ITEM_COUNT = static_cast<int>(Item::enum_type::count);
inline bool is_emblem(int item) { return item % 2 == 1; }
// An emblem is five 30-gold shards.
inline constexpr int EMBLEM_WORTH = 150;

inline int champ_id(Champ kind) { return static_cast<int>(kind); }
