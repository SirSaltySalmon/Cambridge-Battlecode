#include "seen.h"
#include <algorithm>
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

} // namespace

void reset() {
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
    std::vector<Seen> found;
    found.reserve(roster(team).size());
    for (const auto& champion : roster(team)) {
        bc::detail::champion_info info{};
        const auto handle = static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(champion));
        if (bc::detail::sdk_champion(handle, &info) != 0 || info.x < 0 || info.y < 0) continue;
        found.push_back({{info.x, info.y}, &champion, info.health > 0});
    }
    std::vector<bc::champion> result;
    if (found.empty()) return result;
    result.reserve(found.size());
    for (const auto tile : bc::vision::champion_tiles(team).positions())
        for (const auto& entry : found)
            if (entry.at == tile) {
                if (entry.living) result.push_back(*entry.champion);
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
