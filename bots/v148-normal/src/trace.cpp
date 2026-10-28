#include "trace.h"
#include "seen.h"
#include "board.h"
#include <cambc.h>

namespace trace {

std::vector<std::string> acts;
std::string stall;
std::string attack;
std::vector<std::string> audit;

void reset() {
    acts.clear();
    stall.clear();
    attack.clear();
    audit.clear();
    seen::new_turn();
}

void act(const std::string& kind) {
    acts.push_back(kind);
    seen::reset();
}

namespace {
// v135a audit: every camp spawn tile on the map with its group index (static).
const std::vector<std::pair<bc::vec2, int>>& camp_spawns() {
    static const std::vector<std::pair<bc::vec2, int>> tiles = [] {
        std::vector<std::pair<bc::vec2, int>> out;
        const auto groups = bc::map::camp_groups();
        for (int g = 0; g < static_cast<int>(groups.size()); ++g)
            for (const auto t : groups[g].spawn_tiles().positions()) out.push_back({t, g});
        return out;
    }();
    return tiles;
}
}  // namespace

void moved(const std::string& kind) {
    stall.clear();
    acts.push_back(kind);
    seen::reset();
    // v135a audit: a dash landing (HoR's lands an area stun): camp spawn tiles within 1, awake or asleep or unseen.
    if (kind != "move")
        LOG({
            const auto at = bc::self().position();
            bc::bitboard sight{bc::uninitialized};
            bc::vision::visible_tiles_into(sight);
            std::string beside;
            for (const auto& [t, g] : camp_spawns()) {
                if (std::max(std::abs(t.x - at.x), std::abs(t.y - at.y)) > 1) continue;
                // dead: the tile is in sight and holds no camp (cleared or respawning); unseen: out of sight.
                std::string state = sight.is_set(t) ? "dead" : "unseen";
                for (const auto& c : seen::camps())
                    if (c.spawn_position() == t) {
                        std::uint32_t s = 0;
                        state = bc::detail::sdk_camp_state(static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(c)), &s) != 0
                                    ? "hidden" : s == 1 ? "awake" : "asleep";
                    }
                beside += format(" g%d(%d,%d)%s", g, t.x, t.y, state.c_str());
            }
            audit.push_back(format("@land %s (%d,%d)", kind.c_str(), at.x, at.y) + (beside.empty() ? "" : " beside" + beside));
        });
}

void move_failed(const std::string& reason) {
    stall = reason;
    // A rejected step may have revealed a hidden unit in a bush.
    if (reason.rfind("engine rejected", 0) == 0) seen::reset();
}

void attack_failed(const std::string& reason) {
    attack = reason;
}

} // namespace trace
