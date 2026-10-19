#include "state.h"
#include "board.h"
#include "seen.h"
#include <array>
#include <cstddef>

// Raw engine reads that return a code instead of throwing. Allies out of sight fail these every turn,
// and a thrown and caught engine error costs ~2.5k fuel against ~200 for the read itself.
namespace {

std::uint32_t handle_of(const bc::champion& champion) {
    return static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(champion));
}

bool read_items(const bc::champion& champion, std::array<std::uint32_t, ITEM_COUNT>& counts) {
    return bc::detail::sdk_champion_items(handle_of(champion), counts.data()) == 0;
}

} // namespace

int emblem_count(const bc::champion& champion) {
    std::array<std::uint32_t, ITEM_COUNT> counts{};
    if (!read_items(champion, counts)) return 0;
    int total = 0;
    for (int i = 1; i < ITEM_COUNT; i += 2) total += static_cast<int>(counts[i]);
    return total;
}

int item_worth(const bc::champion& champion) {
    std::array<std::uint32_t, ITEM_COUNT> counts{};
    if (!read_items(champion, counts)) return 0;
    {
        static const auto shard_costs = [] {
            std::array<int, ITEM_COUNT> costs{};
            for (int i = 0; i < ITEM_COUNT; i += 2) costs[i] = static_cast<int>(Item(static_cast<std::uint32_t>(i)).cost());
            return costs;
        }();
        int total = 0;
        for (int i = 0; i < ITEM_COUNT; ++i) {
            const int count = static_cast<int>(counts[i]);
            total += is_emblem(i) ? EMBLEM_WORTH * count : count * shard_costs[i];
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

bool under_threat(const bc::champion& me) {
    const auto position = me.position();
    for (const auto& enemy : seen::champions(Team::enemy))
        if (distance(position, enemy.position()) <= enemy.attack_range()) return true;
    for (const auto& camp : seen::camps())
        if (camp.strength() > 0 && camp.health() < camp.max_health()
            && distance(position, camp.position()) <= camp.attack_range())
            return true;
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
    shared.stamp = b(4) | b(5) << 8;
    shared.claimed = b(6);
    shared.ren_done = flags & REN_DONE_FLAG;
    shared.aggressive = flags & AGGRESSIVE_FLAG;
    if (shared.stamp == 0) shared.capture_slot = NO_CAPTURE;
    return shared;
}

void write_shared(int mode, int slot, int capture_slot, int collector, int claimed, bool ren_done,
                  bool aggressive, int carrier) {
    const int flags = mode | carrier << CARRIER_SHIFT | (ren_done ? REN_DONE_FLAG : 0)
        | (aggressive ? AGGRESSIVE_FLAG : 0);
    const int stamp = static_cast<int>(bc::round_number()) + 1;
    const std::array<std::byte, SHARED_BYTES> data{
        std::byte(flags & 0xFF), std::byte(slot & 0xFF), std::byte(capture_slot & 0xFF),
        std::byte(collector & 0xFF), std::byte(stamp & 0xFF), std::byte(stamp >> 8 & 0xFF),
        std::byte(claimed & 0xFF)};
    bc::shared_memory.write(0, std::span<const std::byte>{data});
}

void mark_ren_done() {
    set_shared_byte(MODE_BYTE, shared_byte(MODE_BYTE) | REN_DONE_FLAG);
}

int read_carrier() {
    return (shared_byte(MODE_BYTE) & CARRIER_MASK) >> CARRIER_SHIFT;
}

void write_carrier(int carrier) {
    set_shared_byte(MODE_BYTE, (shared_byte(MODE_BYTE) & ~CARRIER_MASK & 0xFF) | carrier << CARRIER_SHIFT);
}
