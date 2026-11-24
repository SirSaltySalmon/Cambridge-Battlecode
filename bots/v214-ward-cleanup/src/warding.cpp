// The scout ward: our one ward follows us (v72, FIX_SCOUT_WARD).
// v214-ward-cleanup: the fixed ward posts (compute_ward_spots, open_ward_spots, place_ward, ward_detour) and the
// key-bush ward (build_bush_boards, bush_ward, follow_ward) are gone. With FIX_SCOUT_WARD on and SCOUT_KEEP_POSTS off
// as since v72, none of them could run.
#include "bot.h"
#include "seen.h"
#include "trace.h"
#include <algorithm>
#include <array>
#include <optional>

bool Bot::scout_ward(const bc::champion& me) {
    // v72: the ward follows us. Vision is not shared, so a ward far off only helps its owner's
    // view of somewhere else; one beside us shows what is about to hit us. It goes into a reachable bush we
    // cannot see into (hidden enemies and camps), else up to two tiles ahead toward our target.
    try {
        if (me.ward_cooldown() > 0) return false;
    } catch (const bc::error&) {
        return false;
    }
    const auto position = me.position();
    const auto own = me.ward_positions();
    const int reach = static_cast<int>(bc::constants::ward_placement_radius);
    // A ward already watching a bush beside us stays: moving it would hide that bush again.
    for (const auto ward : own)
        if (bc::map::bushes().is_set(ward) && distance(position, ward) <= reach + 1) return false;
    // Moving costs fuel (every vision list is read again), so the ward only moves to look into a bush beside
    // us, or to catch up once it has fallen SCOUT_REFRESH tiles behind. Cheap checks first.
    const bool behind = own.empty() || distance(position, own.front()) > SCOUT_REFRESH;
    // v133t: reused boards and *_into reads (each `~`, `|`, new board and positions() allocated: ~8 a call).
    static bc::bitboard near_bush{bc::uninitialized}, tiles{bc::uninitialized}, part{bc::uninitialized},
        hidden{bc::uninitialized};
    fast::around_into(position, static_cast<float>(reach), near_bush);
    near_bush &= bc::map::bushes();
    if (!behind && near_bush.is_empty()) return false;

    me.ward_placement_tiles_into(tiles);
    bc::vision::ward_tiles_into(Team::ally, part);
    part |= seen::terrain();
    fast::not_into(part, part);
    tiles &= part;
    if (tiles.is_empty()) return false;
    fast::and_into(tiles, near_bush, hidden);
    if (!hidden.is_empty()) {
        bc::vision::visible_tiles_into(part);
        fast::not_into(part, part);
        hidden &= part;
    }
    if (!behind && hidden.is_empty()) return false;
    std::optional<vec2> goal;
    if (!route.empty()) goal = centers[slot % static_cast<int>(route.size())];

    std::optional<vec2> best;
    int best_score = 0;
    std::array<vec2, 64> spots;
    const auto spot_n = std::min<std::size_t>(tiles.positions_into(spots), spots.size());
    for (std::size_t i = 0; i < spot_n; ++i) {
        const auto tile = spots[i];
        int score = (hidden.is_set(tile) ? SCOUT_BUSH_SCORE : 0) + 2 * distance(position, tile);
        if (goal) score -= 3 * distance(tile, *goal);
        if (!best || score > best_score) {
            best = tile;
            best_score = score;
        }
    }
    if (!best || std::find(own.begin(), own.end(), *best) != own.end()) return false;
    try {
        if (!me.place_ward(*best)) {
            // Something hidden stands there, and is now revealed.
            seen::reset();
            return false;
        }
    } catch (const bc::error&) {
        return false;
    }
    trace::act("ward");
    // v90-log: the ward follows us every round (the "ward" act shows it); only a look into a hidden bush is worth a line.
    if (hidden.is_set(*best)) NOTE("scout ward into a hidden bush " + at(*best));
    return true;
}
