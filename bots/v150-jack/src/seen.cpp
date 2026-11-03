#include "seen.h"
#include "board.h"
#include "draft.h"
#include <algorithm>
#include <array>
#include <memory>
#include <optional>

namespace seen {
namespace {

// Lists read this turn. A reset after an action retires them instead of freeing them, so a caller
// still holding a reference from before the action keeps a valid (if stale) list until the turn ends.
std::unique_ptr<std::vector<bc::champion>> turn_allies, turn_enemies;
// v133t: their tiles, filled with them and retired with them.
std::unique_ptr<std::vector<vec2>> turn_ally_tiles, turn_enemy_tiles;
std::vector<std::unique_ptr<std::vector<vec2>>> retired_tiles, spare_tiles;
std::uint64_t self_gen = 0;
vec2 self_at{};
std::unique_ptr<std::vector<bc::camp>> turn_camps;
std::vector<std::unique_ptr<std::vector<bc::champion>>> retired_champions;
std::vector<std::unique_ptr<std::vector<bc::camp>>> retired_camps;
// v99: lists retired last turn are emptied and handed out again, keeping their storage: a fresh list was a
// unique_ptr and a vector allocation (~300 fuel each) every time vision was read, several times a turn.
std::vector<std::unique_ptr<std::vector<bc::champion>>> spare_champions;
std::vector<std::unique_ptr<std::vector<bc::camp>>> spare_camps;
std::optional<std::vector<bc::champion>> roster_allies, roster_enemies;

template <class List>
std::unique_ptr<List> take(std::vector<std::unique_ptr<List>>& spare) {
    if (spare.empty()) return std::make_unique<List>();
    auto list = std::move(spare.back());
    spare.pop_back();
    list->clear();
    return list;
}

template <class Units>
void living_into(Units units, Units& out) {
    for (auto& unit : units) {
        try {
            if (unit.health() > 0) out.push_back(unit);
        } catch (const bc::error&) {
        }
    }
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
    if (turn_ally_tiles) retired_tiles.push_back(std::move(turn_ally_tiles));
    if (turn_enemy_tiles) retired_tiles.push_back(std::move(turn_enemy_tiles));
    if (turn_camps) retired_camps.push_back(std::move(turn_camps));
}

void new_turn() {
    reset();
    for (auto& list : retired_champions) spare_champions.push_back(std::move(list));
    for (auto& list : retired_camps) spare_camps.push_back(std::move(list));
    for (auto& list : retired_tiles) spare_tiles.push_back(std::move(list));
    retired_tiles.clear();
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
void visible_living_into(Team team, std::vector<bc::champion>& result, std::vector<vec2>& tiles) {
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
    if (found_n == 0) return;
    // One unit needs no ordering; a living one in vision stands on its own tile.
    if (found_n == 1) {
        if (found[0].living) {
            result.push_back(*found[0].champion);
            tiles.push_back(found[0].at);
        }
        return;
    }
    // v99: engine tile order is fast::rank order, so a sort of the few found champions stands in for reading the
    // team's champion tile board (~470 fuel and a board, twice a turn). Each living one is on its own tile.
    std::sort(found.begin(), found.begin() + static_cast<std::ptrdiff_t>(found_n),
              [](const Seen& a, const Seen& b) { return fast::rank(a.at) < fast::rank(b.at); });
    for (std::size_t f = 0; f < found_n; ++f)
        if (found[f].living) {
            result.push_back(*found[f].champion);
            tiles.push_back(found[f].at);
        }
}

const std::vector<bc::champion>& champions(Team team) {
    auto& units = team == Team::ally ? turn_allies : turn_enemies;
    if (!units) {
        auto& tiles = team == Team::ally ? turn_ally_tiles : turn_enemy_tiles;
        units = take(spare_champions);
        tiles = take(spare_tiles);
        visible_living_into(team, *units, *tiles);
    }
    return *units;
}

const std::vector<vec2>& positions(Team team) {
    seen::champions(team);
    return team == Team::ally ? *turn_ally_tiles : *turn_enemy_tiles;
}

vec2 self_position() {
    if (self_gen != generation_count) {
        self_at = bc::self().position();
        self_gen = generation_count;
    }
    return self_at;
}

// Living visible camps in the engine's tile order, as living(vision::camps()) lists them. Camps never
// leave their spawn tile, so each visible camp tile names its camp from the map's camp list, and one raw
// read per camp checks it is alive; vision::camps() built every handle with two engine reads and the
// list was then copied. A camp tile that is not a spawn tile falls back to the engine list.
void visible_camps_into(std::vector<bc::camp>& result) {
    struct Index {
        std::vector<std::int16_t> at;
        std::vector<bc::camp> camps;
    };
    static const Index index = [] {
        Index built;
        const int width = bc::map::width();
        built.at.assign(static_cast<std::size_t>(width) * bc::map::height(), -1);
        for (const auto& group : draft::group_data().groups)
            for (const auto& camp : group.camps()) {
                const auto spot = camp.spawn_position();
                built.at[static_cast<std::size_t>(spot.y) * width + spot.x] = static_cast<std::int16_t>(built.camps.size());
                built.camps.push_back(camp);
            }
        return built;
    }();
    std::array<vec2, 128> tiles;
    // v99: read into one reused board (camp_tiles() allocated one each time).
    static bc::bitboard board{bc::uninitialized};
    bc::vision::camp_tiles_into(board);
    const auto tile_n = board.positions_into(tiles);
    auto fallback = [&] {
        result.clear();
        living_into(bc::vision::camps(), result);
    };
    if (tile_n >= tiles.size()) return fallback();
    const int width = bc::map::width();
    for (std::uint32_t t = 0; t < tile_n; ++t) {
        const int id = index.at[static_cast<std::size_t>(tiles[t].y) * width + tiles[t].x];
        if (id < 0) return fallback();
        const auto& camp = index.camps[id];
        bc::detail::camp_state state{};
        if (bc::detail::sdk_camp(static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(camp)), &state) != 0) continue;
        if (state.health > 0) result.push_back(camp);
    }
}

const std::vector<bc::camp>& camps() {
    if (!turn_camps) {
        turn_camps = take(spare_camps);
        visible_camps_into(*turn_camps);
    }
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
