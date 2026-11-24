// The scout ward: our one ward follows us (v72, FIX_SCOUT_WARD).
// v214-ward-cleanup: the fixed ward posts (compute_ward_spots, open_ward_spots, place_ward, ward_detour) and the
// key-bush ward (build_bush_boards, bush_ward, follow_ward) are gone. With FIX_SCOUT_WARD on and SCOUT_KEEP_POSTS off
// as since v72, none of them could run.
// v219-ward: + SCOUT_MASK (v214-ward-mask) and WARD_HOLD (v214-ward-hold), see constants.h.
#include "bot.h"
#include "seen.h"
#include "trace.h"
#include <algorithm>
#include <array>
#include <optional>

namespace {
// v214-ward-mask (SCOUT_MASK): tiles a scout ward was refused on (in practice an ally's ward hidden in a bush), kept out
// of the candidates until SCOUT_MASK_ROUNDS after the latest refusal. File statics (one bot instance per champion),
// constant-initialised: a Bot member cost ~9 fuel on draft turns. The board is made on the first refusal.
bc::bitboard* scout_blocked = nullptr;  // lives for the match, no static destructor
int scout_blocked_round = -1000;
// v214-ward-hold (WARD_HOLD): last round an enemy champion in sight was within WARD_HOLD_RANGE of us.
int last_near_enemy_round = -1000;
}  // namespace

bool Bot::scout_ward(const bc::champion& me) {
    // v72: the ward follows us. Vision is not shared, so a ward far off only helps its owner's
    // view of somewhere else; one beside us shows what is about to hit us. It goes into a reachable bush we
    // cannot see into (hidden enemies and camps), else up to two tiles ahead toward our target.
    // v214-ward-hold: note an enemy champion within WARD_HOLD_RANGE every call (cached vision list, no engine read), so
    // the WARD_HOLD_ROUNDS window also covers rounds the charge was recharging.
    bool hold_near = false;
    if constexpr (WARD_HOLD) {
        const auto here = seen::self_position();
        for (const auto p : seen::positions(Team::enemy))
            if (distance(p, here) <= WARD_HOLD_RANGE) {
                hold_near = true;
                last_near_enemy_round = turn_round;
                break;
            }
    }
    try {
        if (me.ward_cooldown() > 0) return false;
    } catch (const bc::error&) {
        return false;
    }
    // v214-ward-hold: a fight is likely, so the catch-up ward is not placed (the charge stays for ghost_ward and
    // answer_hidden). A look into a hidden bush beside us still goes ahead (WARD_HOLD_BUSH off).
    const bool held = WARD_HOLD && (mode == FIGHT || hold_near || turn_round - last_near_enemy_round < WARD_HOLD_ROUNDS);
    if (WARD_HOLD_BUSH && held) return false;
    const auto position = me.position();
    const auto own = me.ward_positions();
    const int reach = static_cast<int>(bc::constants::ward_placement_radius);
    // A ward already watching a bush beside us stays: moving it would hide that bush again.
    for (const auto ward : own)
        if (bc::map::bushes().is_set(ward) && distance(position, ward) <= reach + 1) return false;
    // Moving costs fuel (every vision list is read again), so the ward only moves to look into a bush beside
    // us, or to catch up once it has fallen SCOUT_REFRESH tiles behind. Cheap checks first.
    const bool behind = !held && (own.empty() || distance(position, own.front()) > SCOUT_REFRESH);
    // v133t: reused boards and *_into reads (each `~`, `|`, new board and positions() allocated: ~8 a call).
    static bc::bitboard near_bush{bc::uninitialized}, tiles{bc::uninitialized}, part{bc::uninitialized},
        hidden{bc::uninitialized};
    fast::around_into(position, static_cast<float>(reach), near_bush);
    near_bush &= bc::map::bushes();
    if (!behind && near_bush.is_empty()) return false;

    me.ward_placement_tiles_into(tiles);
    bc::vision::ward_tiles_into(Team::ally, part);
    int mask_round = 0;
    if constexpr (SCOUT_MASK > 0) {
        // v214-ward-mask: altars and the enemy spawn too ("Forbidden ward tile" throws, ~1 a game), and tiles we were
        // refused on lately (one board op).
        part |= seen::fixed_blocks();
        if (scout_blocked_round > -1000) {
            mask_round = static_cast<int>(bc::round_number());
            if (mask_round - scout_blocked_round <= SCOUT_MASK_ROUNDS) {
                part |= *scout_blocked;
            } else {
                scout_blocked->clear();
                scout_blocked_round = -1000;
            }
        }
    } else {
        part |= seen::terrain();
    }
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

    std::array<vec2, 64> spots;
    const auto spot_n = std::min<std::size_t>(tiles.positions_into(spots), spots.size());
    std::optional<vec2> best, skip;
    // v214-ward-mask (SCOUT_MASK=2): after a refusal the next-best tile is tried once in the same turn, when it is
    // another hidden bush tile or a catch-up (without the retry the turn's ward would have gone nowhere).
    for (int attempt = 0;; ++attempt) {
        best.reset();
        int best_score = 0;
        for (std::size_t i = 0; i < spot_n; ++i) {
            const auto tile = spots[i];
            if (skip && tile == *skip) continue;
            int score = (hidden.is_set(tile) ? SCOUT_BUSH_SCORE : 0) + 2 * distance(position, tile);
            if (goal) score -= 3 * distance(tile, *goal);
            if (!best || score > best_score) {
                best = tile;
                best_score = score;
            }
        }
        if (!best || std::find(own.begin(), own.end(), *best) != own.end()) return false;
        if (attempt > 0 && !behind && !hidden.is_set(*best)) return false;
        try {
            if (me.place_ward(*best)) break;
        } catch (const bc::error&) {
            return false;
        }
        // Something hidden stands there, and is now revealed.
        seen::reset();
        if constexpr (SCOUT_MASK > 0) {
            if (!scout_blocked) scout_blocked = new bc::bitboard();
            scout_blocked->set(*best);
            scout_blocked_round = mask_round > 0 ? mask_round : static_cast<int>(bc::round_number());
        }
        if (SCOUT_MASK < 2 || attempt > 0) return false;
        skip = best;
    }
    trace::act("ward");
    // v90-log: the ward follows us every round (the "ward" act shows it); only a look into a hidden bush is worth a line.
    if (hidden.is_set(*best)) NOTE("scout ward into a hidden bush " + at(*best));
    return true;
}
