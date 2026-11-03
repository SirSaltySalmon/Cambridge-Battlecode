#include "state.h"
#include "board.h"
#include "seen.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

// Raw engine reads that return a code instead of throwing. Allies out of sight fail these every turn,
// and a thrown and caught engine error costs ~2.5k fuel against ~200 for the read itself.
namespace {

std::uint32_t handle_of(const bc::champion& champion) {
    return static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(champion));
}

bool read_items(const bc::champion& champion, std::array<std::uint32_t, ITEM_COUNT>& counts) {
    return bc::detail::sdk_champion_items(handle_of(champion), counts.data()) == 0;
}

// v134: the last emblem read per champion handle (both teams fit; a linear scan beats a map at this size).
struct OwnedEntry {
    std::uint32_t handle;
    Owned owned;
};
std::vector<OwnedEntry> owned_cache;

} // namespace

int emblem_count(const bc::champion& champion) {
    std::array<std::uint32_t, ITEM_COUNT> counts{};
    if (!read_items(champion, counts)) return 0;
    Owned owned;
    // Items come in families of six (shard and emblem per stat); odd indices are emblems.
    for (int i = 1; i < ITEM_COUNT; i += 2)
        if (counts[i]) {
            owned.emblems += static_cast<int>(counts[i]);
            owned.passives |= Passives{1} << (i / 6);
        }
    const auto handle = handle_of(champion);
    for (auto& entry : owned_cache)
        if (entry.handle == handle) {
            entry.owned = owned;
            return owned.emblems;
        }
    owned_cache.push_back({handle, owned});
    return owned.emblems;
}

Owned owned_of(const bc::champion& champion) {
    const auto handle = handle_of(champion);
    for (const auto& entry : owned_cache)
        if (entry.handle == handle) return entry.owned;
    return {};
}

int item_worth(const bc::champion& champion) {
    std::array<std::uint32_t, ITEM_COUNT> counts{};
    if (!read_items(champion, counts)) return 0;
    {
        static const auto shard_costs = [] {
            std::array<int, ITEM_COUNT> costs{};
            // v134: an emblem is worth the shards that made it (a Greed emblem 25 of them).
            for (int i = 0; i < ITEM_COUNT; i += 2) {
                const Item shard{static_cast<std::uint32_t>(i)};
                costs[i] = static_cast<int>(shard.cost());
                costs[i + 1] = costs[i] * static_cast<int>(shard.combine_count());
            }
            return costs;
        }();
        int total = 0;
        for (int i = 0; i < ITEM_COUNT; ++i) {
            const int count = static_cast<int>(counts[i]);
            total += count * shard_costs[i];
        }
        return total;
    }
}

bool team_alive() {
    for (const auto& ally : seen::roster(Team::ally))
        if (!ally.is_alive()) return false;
    return true;
}

int gems_carried(const bc::champion& champion) {
    std::uint32_t gems{};
    if (bc::detail::sdk_champion_gems(handle_of(champion), &gems) != 0) return 0;
    return static_cast<int>(gems);
}

bc::bitboard threat_zone(const std::vector<bc::champion>& enemies, bool include_camps) {
    bc::bitboard zone;
    for (const auto& enemy : enemies) zone.draw_chebyshev(enemy.position(), THREAT_RANGE);
    if (include_camps)
        for (const auto& camp : seen::camps())
            if (camp.strength() > 0) zone.draw_chebyshev(camp.position(), static_cast<float>(camp.attack_range()));
    return zone;
}

int recall_reach(const bc::champion& enemy) {
    int basic = 1;
    try {
        basic = enemy.attack_range();
        if (RECALL_REACH_MODE == 0) return std::max(basic, spell_reach(enemy.champion_name()));
        int dash = 0, spell = 0;
        for (const auto& slot : enemy.abilities()) {
            const auto& info = slot.info();
            if (!info) continue;
            if (RECALL_REACH_MODE == 2 && slot.cooldown() > RECALL_READY_TICKS) continue;
            if (const auto* d = std::get_if<bc::ability_dash>(&*info)) dash = std::max(dash, static_cast<int>(d->range));
            else if (const auto* h = std::get_if<bc::ability_damage>(&*info)) spell = std::max(spell, static_cast<int>(h->range));
            else if (const auto* c = std::get_if<bc::ability_cc>(&*info)) spell = std::max(spell, static_cast<int>(c->range));
            else if (const auto* o = std::get_if<bc::ability_aoe_damage>(&*info))
                spell = std::max(spell, static_cast<int>(o->range + o->radius));
        }
        return std::max(basic + dash, spell);
    } catch (const std::exception&) {
        return std::max(basic, THREAT_RANGE);
    }
}

bc::bitboard recall_zone(const std::vector<bc::champion>& enemies, bool include_camps) {
    auto zone = threat_zone(enemies, include_camps);
    if (!RECALL_SPELL_CAUTION) return zone;
    for (const auto& enemy : enemies) {
        const int reach = recall_reach(enemy);
        if (reach + RECALL_SPELL_STEP > THREAT_RANGE)
            zone.draw_chebyshev(enemy.position(), static_cast<float>(reach + RECALL_SPELL_STEP));
    }
    return zone;
}

std::optional<bc::champion> recall_caster(const std::vector<bc::champion>& enemies, vec2 tile) {
    for (const auto& enemy : enemies) {
        const int reach = recall_reach(enemy);
        const int gap = distance(tile, enemy.position());
        if (gap > THREAT_RANGE && gap <= reach + RECALL_SPELL_STEP) return enemy;
    }
    return std::nullopt;
}

bool under_threat(const bc::champion& me) {
    const auto position = me.position();
    for (const auto& enemy : seen::champions(Team::enemy))
        if (distance(position, enemy.position()) <= enemy.attack_range()) return true;
    // v99: camps never leave their spawn tile and none reaches past MONOLITH_REACH, so a distance check on the spawn tile
    // (no engine read) skips the far ones before the four reads below; the result is the same.
    for (const auto& camp : seen::camps())
        if (distance(position, camp.spawn_position()) <= MONOLITH_REACH && camp.strength() > 0
            && distance(position, camp.position()) <= camp.attack_range()) {
            if (camp.health() < camp.max_health()) return true;
            // v145 (AWAKE_READ): an unhurt camp of a woken group hits too.
            try {
                if (AWAKE_READ && camp.is_awake()) return true;
            } catch (const bc::error&) {
            }
        }
    return false;
}

int camp_exit_cost(const bc::champion& me) {
    // Each round start, every awake camp hits the nearest champion in its reach. Walking out takes one step per
    // turn, and every round start before the last step is one more hit; the steps themselves are a hit of margin.
    const auto position = me.position();
    double strength = 0;
    int steps = 0;
    for (const auto& camp : seen::camps()) {
        if (camp.strength() <= 0) continue;
        const auto spot = camp.position();
        const int reach = camp.attack_range();
        const int gap = distance(position, spot);
        if (gap > reach) continue;
        try {
            // Damage wakes the whole group, so an unhurt camp may be awake too.
            if (camp.health() >= camp.max_health() && !camp.is_awake()) continue;
        } catch (const bc::error&) {
            continue;
        }
        bool nearer = false;
        for (const auto& ally : seen::champions(Team::ally))
            if (ally != me) {
                const int g = distance(ally.position(), spot);
                if (g < gap || (g == gap && acts_before_me(ally))) nearer = true;
            }
        if (nearer) continue;
        strength += camp.strength();
        steps = std::max(steps, reach - gap + 1);
    }
    // v134 (ITEM_DAMAGE): Jungle and Durability cut the hits.
    return static_cast<int>(strength * steps * camp_taken_mult(owned_of(me).passives));
}

bool acts_before_me(const bc::champion& ally) {
    // Priorities are fixed per champion type and the roster lists a team in draft order, so the allies acting before
    // us are worked out once a game.
    if (!CAMP_TIE_ORDER) return false;
    static std::array<std::uint32_t, 8> earlier{};
    static std::size_t n = 0;
    static bool ready = false;
    if (!ready) {
        const auto& me = bc::self();
        const auto& team = seen::roster(Team::ally);
        std::size_t mine = team.size();
        for (std::size_t i = 0; i < team.size(); ++i)
            if (team[i] == me) mine = i;
        const int own = priority_of(me.champion_name());
        for (std::size_t i = 0; i < team.size() && n < earlier.size(); ++i) {
            if (i == mine) continue;
            const int p = priority_of(team[i].champion_name());
            if (p > own || (p == own && i < mine)) earlier[n++] = static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(team[i]));
        }
        ready = true;
    }
    const auto handle = static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(ally));
    for (std::size_t i = 0; i < n; ++i)
        if (earlier[i] == handle) return true;
    return false;
}

int shared_byte(int offset) {
    return static_cast<std::uint8_t>(bc::shared_memory[offset]);
}

void set_shared_byte(int offset, int value) {
    bc::shared_memory[offset] = static_cast<std::uint8_t>(value & 0xFF);
}

Shared read_shared() {
    std::array<std::byte, SHARED_BYTES> data{};
    bc::shared_memory.read(0, std::span<std::byte>{data});
    auto b = [&](int i) { return std::to_integer<int>(data[i]); };
    const int flags = b(0);
    Shared shared{};
    shared.mode = flags & MODE_MASK;
    shared.slot = b(1);
    shared.capture_slot = b(2);
    shared.collector = b(3);
    shared.stamp = b(STAMP_BYTE) & STAMP_MASK;
    shared.phase = b(PHASE_BYTE) >> PHASE_SHIFT;
    shared.target2 = b(PHASE_BYTE) & TARGET2_MASK;
    // Zeroed memory: no pair target yet.
    if (shared.stamp == 0) shared.target2 = NO_TARGET2;
    shared.claimed = b(6);
    shared.vesper_done = flags & VESPER_DONE_FLAG;
    shared.aggressive = flags & AGGRESSIVE_FLAG;
    if (shared.stamp == 0) shared.capture_slot = NO_CAPTURE;
    return shared;
}

void write_shared(int mode, int slot, int capture_slot, int collector, int claimed, bool vesper_done,
                  bool aggressive, int carrier, int phase) {
    const int flags = mode | carrier << CARRIER_SHIFT | (vesper_done ? VESPER_DONE_FLAG : 0)
        | (aggressive ? AGGRESSIVE_FLAG : 0);
    // v183 (FOCUS): the focus target in the stamp byte's high nibble is kept.
    const int stamp = stamp_of(static_cast<int>(bc::round_number())) | (shared_byte(STAMP_BYTE) & ~STAMP_MASK & 0xFF);
    // Zeroed memory has no pair target yet: NO_TARGET2 on the first write.
    const int target2 = (shared_byte(STAMP_BYTE) & STAMP_MASK) == 0 ? NO_TARGET2 : shared_byte(PHASE_BYTE) & TARGET2_MASK;
    const std::array<std::byte, SHARED_BYTES> data{
        std::byte(flags & 0xFF), std::byte(slot & 0xFF), std::byte(capture_slot & 0xFF),
        std::byte(collector & 0xFF), std::byte(stamp & 0xFF), std::byte((phase << PHASE_SHIFT | target2) & 0xFF),
        std::byte(claimed & 0xFF)};
    bc::shared_memory.write(0, std::span<const std::byte>{data});
}

void write_phase(int phase) {
    set_shared_byte(PHASE_BYTE, (shared_byte(PHASE_BYTE) & TARGET2_MASK) | phase << PHASE_SHIFT);
}

void write_target2(int target) {
    set_shared_byte(PHASE_BYTE, (shared_byte(PHASE_BYTE) & ~TARGET2_MASK & 0xFF) | (target & TARGET2_MASK));
}

void mark_vesper_done() {
    set_shared_byte(MODE_BYTE, shared_byte(MODE_BYTE) | VESPER_DONE_FLAG);
}

int read_carrier() {
    return (shared_byte(MODE_BYTE) & CARRIER_MASK) >> CARRIER_SHIFT;
}

void write_carrier(int carrier) {
    set_shared_byte(MODE_BYTE, (shared_byte(MODE_BYTE) & ~CARRIER_MASK & 0xFF) | carrier << CARRIER_SHIFT);
}

int read_focus() {
    return ((shared_byte(STAMP_BYTE) >> FOCUS_SHIFT) & FOCUS_INDEX_MASK) - 1;
}

void write_focus(int index) {
    // v216 (FOCUS_KEEP): bit 7 is the refresh bit, round & 1 of the write.
    const int fresh = FOCUS_KEEP ? (static_cast<int>(bc::round_number()) & 1) << 3 : 0;
    set_shared_byte(STAMP_BYTE, (shared_byte(STAMP_BYTE) & STAMP_MASK)
                                    | (((index + 1) & FOCUS_INDEX_MASK) | fresh) << FOCUS_SHIFT);
}

int focus_nibble() {
    return shared_byte(STAMP_BYTE) >> FOCUS_SHIFT;
}
