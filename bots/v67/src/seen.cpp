#include "seen.h"
#include <algorithm>
#include <array>
#include <memory>
#include <optional>

namespace seen {
namespace {

// Lists read this turn. A reset after an action retires them instead of freeing them, so a caller
// still holding a reference from before the action keeps a valid (if stale) list until the turn ends.
std::unique_ptr<std::vector<bc::champion>> turn_allies, turn_enemies;
std::unique_ptr<std::vector<bc::camp>> turn_camps;
std::vector<std::unique_ptr<std::vector<bc::champion>>> retired_champions;
std::vector<std::unique_ptr<std::vector<bc::camp>>> retired_camps;
std::optional<std::vector<bc::champion>> roster_allies, roster_enemies;

template <class Units>
Units living(Units units) {
    Units result;
    for (auto& unit : units) {
        try {
            if (unit.health() > 0) result.push_back(unit);
        } catch (const bc::error&) {
        }
    }
    return result;
}

std::uint64_t generation_count = 1;

} // namespace

std::uint64_t generation() {
    return generation_count;
}

void reset() {
    ++generation_count;
    if (turn_allies) retired_champions.push_back(std::move(turn_allies));
    if (turn_enemies) retired_champions.push_back(std::move(turn_enemies));
    if (turn_camps) retired_camps.push_back(std::move(turn_camps));
}

void new_turn() {
    reset();
    retired_champions.clear();
    retired_camps.clear();
}

const std::vector<bc::champion>& roster(Team team) {
    auto& units = team == Team::ally ? roster_allies : roster_enemies;
    if (!units) units = bc::champions(team);
    return *units;
}

// Living champions of a team that are in vision, in the engine's tile order (what vision::champions
// returns). vision::champions() makes a fresh handle per unit (two ability reads each, ~3k fuel per
// list); here the engine only lists the occupied tiles and each tile is matched to its cached roster
// handle with one raw read.
std::vector<bc::champion> visible_living(Team team) {
    struct Seen {
        vec2 at;
        const bc::champion* champion;
        bool living;
    };
    // A side has at most six champions, so a fixed array stands in for a vector (an allocation each).
    std::array<Seen, 16> found;
    std::size_t found_n = 0;
    for (const auto& champion : roster(team)) {
        bc::detail::champion_info info{};
        const auto handle = static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(champion));
        if (bc::detail::sdk_champion(handle, &info) != 0 || info.x < 0 || info.y < 0) continue;
        if (found_n < found.size()) found[found_n++] = {{info.x, info.y}, &champion, info.health > 0};
    }
    std::vector<bc::champion> result;
    if (found_n == 0) return result;
    // One unit needs no ordering; a living one in vision stands on its own tile.
    if (found_n == 1) {
        if (found[0].living) result.push_back(*found[0].champion);
        return result;
    }
    result.reserve(found_n);
    // The tiles come out in engine order into a stack buffer, without allocating a list.
    std::array<vec2, 16> tiles;
    const auto tile_n = bc::vision::champion_tiles(team).positions_into(tiles);
    for (std::uint32_t t = 0; t < tile_n && t < tiles.size(); ++t)
        for (std::size_t f = 0; f < found_n; ++f)
            if (found[f].at == tiles[t]) {
                if (found[f].living) result.push_back(*found[f].champion);
                break;
            }
    return result;
}

const std::vector<bc::champion>& champions(Team team) {
    auto& units = team == Team::ally ? turn_allies : turn_enemies;
    if (!units) units = std::make_unique<std::vector<bc::champion>>(visible_living(team));
    return *units;
}

const std::vector<bc::camp>& camps() {
    if (!turn_camps) turn_camps = std::make_unique<std::vector<bc::camp>>(living(bc::vision::camps()));
    return *turn_camps;
}

const bc::bitboard& terrain() {
    static const bc::bitboard board = bc::map::walls() | bc::map::holes();
    return board;
}

const bc::bitboard& fixed_blocks() {
    static const bc::bitboard board = terrain() | bc::map::altars() | bc::map::spawn_region(Team::enemy);
    return board;
}

const bc::bitboard& ally_spawn() {
    return bc::map::spawn_region(Team::ally);
}

const std::vector<vec2>& altar_list() {
    static const std::vector<vec2> tiles = bc::map::altars().positions();
    return tiles;
}

} // namespace seen
