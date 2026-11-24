// v215 (TP): teleport crystals. The draft finds where a crystal beats walking; travel moves launch from there.
#include "crystal.h"
#include "actions.h"
#include "board.h"
#include "draft.h"
#include "seen.h"
#include "trace.h"
#include <algorithm>
#include <array>
#include <utility>

namespace crystal {

bool carrying = false;
bool gate = false;

namespace {

std::vector<Jump> stored;
int map_value = 0;

// ---- Draft: bitboard wavefronts (host board ops cost the same on any map size; guest loops over tiles cost ~300
// fuel a tile a pass, too dear on 100x100 maps). ----

// 8-neighbour dilation (radius 1) of in, into out.
void dilate1(const bc::bitboard& in, bc::bitboard& tmp, bc::bitboard& h, bc::bitboard& out) {
    fast::copy_into(in, h);
    in.shift_right_into(1, tmp);
    h |= tmp;
    in.shift_right_into(-1, tmp);
    h |= tmp;
    fast::copy_into(h, out);
    h.shift_up_into(1, tmp);
    out |= tmp;
    h.shift_up_into(-1, tmp);
    out |= tmp;
}

// Chebyshev dilation by REACH (3): radius 1, then 2 more, along x, then along y.
void dilate3(const bc::bitboard& in, bc::bitboard& tmp, bc::bitboard& h, bc::bitboard& out) {
    fast::copy_into(in, h);
    in.shift_right_into(1, tmp);
    h |= tmp;
    in.shift_right_into(-1, tmp);
    h |= tmp;
    fast::copy_into(h, out);
    h.shift_right_into(2, tmp);
    out |= tmp;
    h.shift_right_into(-2, tmp);
    out |= tmp;
    fast::copy_into(out, h);
    out.shift_up_into(1, tmp);
    h |= tmp;
    out.shift_up_into(-1, tmp);
    h |= tmp;
    fast::copy_into(h, out);
    h.shift_up_into(2, tmp);
    out |= tmp;
    h.shift_up_into(-2, tmp);
    out |= tmp;
}

struct Cand {
    vec2 t;
    int k = 0;     // steps to the goals with the crystal launched here
    int save = 0;  // walking steps minus k
    int cls = 0;
};

constexpr int FAR = 200;     // walking steps counted for tiles walking never reaches (pockets)
constexpr int KMAX = 400;    // wavefront layers at most
constexpr std::size_t CAND_CAP = 3000;

struct Wave {
    bc::bitboard tmp, h, step, F, nF, O, nO, pads, newq, done;
    std::array<bc::bitboard, 4> D;
};

// One goal class: walking layers F_k (steps <= k) and one-crystal layers O_k = walk(O_{k-1}) | F_k | D_{k-3}, where D_j
// (the Chebyshev-3 dilation of F_j) holds the tiles a crystal lands from within j steps of the goals. A tile first
// reached through D (not by walking, nor by walking to a launch tile) is a launch tile: candidates with their saving.
// Query tiles get their one-crystal steps in one_at (by tile index), for the map's value. False if fuel ran low.
bool wave(const bc::bitboard& goals, int cls, const bc::distance_field& walk, const bc::bitboard& walk_reached,
          const bc::bitboard& queries, std::vector<int>& one_at, std::vector<Cand>& out, Wave& w) {
    static const bc::bitboard open = ~seen::fixed_blocks();
    const int width = bc::map::width();
    fast::and_into(goals, open, w.F);
    fast::copy_into(w.F, w.O);
    dilate3(w.F, w.tmp, w.h, w.D[0]);
    w.D[0] &= open;
    fast::and_into(w.O, queries, w.done);
    for (const auto t : w.done.positions()) one_at[t.y * width + t.x] = 0;
    int stable = 0;
    for (int k = 1; k <= KMAX; ++k) {
        if (bc::fuel_remaining() < TP_DRAFT_RESERVE) return false;
        dilate1(w.F, w.tmp, w.h, w.nF);
        w.nF &= open;
        LOG(if (k < 6 || k % 10 == 0) say(format("WAVE cls %d k %d F %u nF %u O %u", cls, k, w.F.count(), w.nF.count(), w.O.count())););
        dilate1(w.O, w.tmp, w.h, w.step);
        w.step &= open;
        fast::copy_into(w.step, w.nO);
        w.nO |= w.nF;
        if (k >= 3) {
            const auto& launch = w.D[(k - 3) % 4];
            w.nO |= launch;
            fast::not_into(w.step, w.tmp);
            fast::and_into(launch, w.tmp, w.pads);
            fast::not_into(w.nF, w.tmp);
            w.pads &= w.tmp;
            if (!w.pads.is_empty() && out.size() < CAND_CAP)
                for (const auto t : w.pads.positions()) {
                    const int f = walk_reached.is_set(t) ? static_cast<int>(walk.get(t)) : FAR;
                    const int save = std::min(f, FAR) - k;
                    if (save >= TP_KEEP_SAVE && out.size() < CAND_CAP) out.push_back({t, k, save, cls});
                }
        }
        fast::and_into(w.nO, queries, w.newq);
        fast::not_into(w.done, w.tmp);
        w.newq &= w.tmp;
        if (!w.newq.is_empty()) {
            for (const auto t : w.newq.positions()) { one_at[t.y * width + t.x] = k; LOG(if (cls == 0) say(format("MARK (%d,%d) k %d vec %p", t.x, t.y, k, (void*)one_at.data()));); }
            w.done |= w.newq;
        }
        dilate3(w.nF, w.tmp, w.h, w.D[k % 4]);
        w.D[k % 4] &= open;
        const bool same = w.nO == w.O && w.nF == w.F;
        std::swap(w.F, w.nF);
        std::swap(w.O, w.nO);
        if (same) {
            if (++stable >= 4) { LOG(say(format("WAVE cls %d end k %d F %u O %u q %u done %u", cls, k, w.F.count(), w.O.count(), queries.count(), w.done.count()));); break; }
        } else {
            stable = 0;
        }
    }
    return true;
}

// The best landing for a launch tile: the legal tile within REACH nearest the goals by walking.
std::optional<vec2> best_landing(vec2 t, const bc::distance_field& walk, const bc::bitboard& walk_reached) {
    static const bc::bitboard open = ~seen::fixed_blocks();
    static bc::bitboard around{bc::uninitialized};
    fast::around_into(t, static_cast<float>(REACH), around);
    around &= open;
    around &= walk_reached;
    std::array<vec2, 64> tiles;
    const auto n = std::min<std::size_t>(around.positions_into(tiles), tiles.size());
    std::optional<vec2> best;
    std::uint16_t value = bc::distance_field::unreachable;
    for (std::size_t i = 0; i < n; ++i) {
        const auto v = walk.get(tiles[i]);
        if (v < value) value = v, best = tiles[i];
    }
    return best;
}

}  // namespace

std::vector<std::uint8_t> encode(int room) {
    std::vector<std::uint8_t> out;
    if (!TP || room < 6 || bc::fuel_remaining() < TP_DRAFT_RESERVE + 500000) return out;
    const auto fuel_in = bc::fuel_remaining();
    const auto& data = draft::group_data();
    const int width = bc::map::width();
    const auto& ours = bc::map::spawn_region(Team::ally);
    // Goal classes: the altars' tribute rings, our spawn, every camp tile.
    bc::bitboard ring, camps;
    for (const auto a : bc::map::altars().positions()) ring.draw_chebyshev(a, 1.0f);
    bc::bitboard queries = ours;
    for (std::size_t i = 0; i < data.groups.size(); ++i)
        for (const auto t : data.spawn_lists[i]) {
            camps.set(t);
            if (data.groups[i].gem_value() > 0) queries.set(t);
        }
    const bc::bitboard* goals[3] = {&ring, &ours, &camps};
    std::array<bc::distance_field, 3> walk;
    std::array<bc::bitboard, 3> walk_reached;
    std::vector<int> one_alt(static_cast<std::size_t>(width) * bc::map::height(), -1), one_home(one_alt.size(), -1), unused;
    std::vector<Cand> cands;
    Wave w;
    bool full = true;
    for (int c = 0; c < 3 && full; ++c) {
        fast::and_into(*goals[c], ~seen::fixed_blocks(), w.tmp);
        walk[c].compute(seen::fixed_blocks(), w.tmp, walk_reached[c]);
        if (c == 2) unused.assign(one_alt.size(), -1);
        LOG(say(format("CALL c %d alt %p home %p unused %p", c, (void*)one_alt.data(), (void*)one_home.data(), (void*)unused.data())););
        full = wave(*goals[c], c, walk[c], walk_reached[c], queries, c == 0 ? one_alt : c == 1 ? one_home : unused, cands, w);
    }
    const auto fuel_waves = bc::fuel_remaining();
    // The map's value: gem-weighted turns saved per trip, group to altar plus spawn to group.
    auto at = [&](vec2 t) { return static_cast<std::size_t>(t.y) * width + t.x; };
    double weight = 0, save_alt = 0, save_home = 0;
    for (std::size_t i = 0; i < data.groups.size(); ++i) {
        const double gems = data.groups[i].gem_value();
        if (gems <= 0 || data.spawn_lists[i].empty()) continue;
        int wa = FAR, oa = FAR, wh = FAR, oh = FAR;
        for (const auto t : data.spawn_lists[i]) {
            if (walk_reached[0].is_set(t)) wa = std::min<int>(wa, walk[0].get(t));
            if (walk_reached[1].is_set(t)) wh = std::min<int>(wh, walk[1].get(t));
            if (one_alt[at(t)] >= 0) oa = std::min(oa, one_alt[at(t)]);
            if (one_home[at(t)] >= 0) oh = std::min(oh, one_home[at(t)]);
        }
        if (oa >= FAR && oh >= FAR) continue;
        weight += gems;
        save_alt += gems * std::max(0, std::min(wa, oa + 60) - std::min(oa, wa));
        save_home += gems * std::max(0, std::min(wh, oh + 60) - std::min(oh, wh));
    }
    LOG({ for (std::size_t i = 0; i < data.groups.size() && i < 12; ++i) { if (data.spawn_lists[i].empty()) continue; auto t = data.spawn_lists[i][0];
        say(format("NEWGRP %zu (%d,%d) alt walk %d one %d", i, t.x, t.y, walk_reached[0].is_set(t) ? (int)walk[0].get(t) : -1, one_alt[at(t)])); } });
    const double per_trip = weight > 0 ? (save_alt + save_home) / weight : 0.0;
    map_value = std::clamp(static_cast<int>(per_trip + 0.5), 0, 255);
    // Keep the jumps: biggest saving first (altar class first on ties), none beside a kept one going the same way.
    std::stable_sort(cands.begin(), cands.end(), [](const Cand& x, const Cand& y) {
        return x.save != y.save ? x.save > y.save : x.cls < y.cls;
    });
    const int most = std::min<int>(TP_MAX_JUMPS, (room - 3) / 3);
    std::vector<Jump> kept;
    for (const auto& c : cands) {
        if (static_cast<int>(kept.size()) >= most || bc::fuel_remaining() < TP_DRAFT_RESERVE) break;
        bool near = false;
        for (const auto& j : kept)
            if (distance(j.a, c.t) <= 1 || distance(j.b, c.t) <= 1) near = true;
        if (near) continue;
        const auto land = best_landing(c.t, walk[c.cls], walk_reached[c.cls]);
        if (!land) continue;
        bool same = false;
        for (const auto& j : kept)
            if ((distance(j.a, c.t) <= 2 && distance(j.b, *land) <= 2) || (distance(j.b, c.t) <= 2 && distance(j.a, *land) <= 2))
                same = true;
        if (!same) kept.push_back({c.t, *land});
    }
    for (const auto& j : kept) {
        out.push_back(static_cast<std::uint8_t>(j.a.x));
        out.push_back(static_cast<std::uint8_t>(j.a.y));
        out.push_back(static_cast<std::uint8_t>((j.b.x - j.a.x + REACH) * 7 + (j.b.y - j.a.y + REACH)));
    }
    out.push_back(static_cast<std::uint8_t>(kept.size()));
    out.push_back(static_cast<std::uint8_t>(map_value));
    out.push_back(MAGIC);
    LOG(say(format("draft tp: value %d (alt %.1f home %.1f per trip), %zu candidates, %zu jumps, %s; fuel waves %llu, keep %llu, left %llu",
                   map_value, weight > 0 ? save_alt / weight : 0.0, weight > 0 ? save_home / weight : 0.0, cands.size(), kept.size(),
                   full ? "full" : "cut short", static_cast<unsigned long long>(fuel_in - fuel_waves),
                   static_cast<unsigned long long>(fuel_waves - bc::fuel_remaining()),
                   static_cast<unsigned long long>(bc::fuel_remaining())));
        for (const auto& j : kept) say(format("draft tp jump (%d,%d)->(%d,%d)", j.a.x, j.a.y, j.b.x, j.b.y));
        if (bc::map::width() <= 60) {
            bc::bitboard A, B; for (const auto& j : kept) { A.set(j.a); B.set(j.b); }
            for (int y = 0; y < bc::map::height(); ++y) { std::string row;
                for (int x = 0; x < bc::map::width(); ++x) { const vec2 t{x, y}; char ch = '.';
                    if (bc::map::walls().is_set(t)) ch = '#'; else if (bc::map::holes().is_set(t)) ch = 'o'; else if (bc::map::altars().is_set(t)) ch = 'A';
                    else if (bc::map::spawn_region(Team::ally).is_set(t)) ch = 'S'; else if (bc::map::spawn_region(Team::enemy).is_set(t)) ch = 'E'; else if (camps.is_set(t)) ch = 'c';
                    if (A.is_set(t)) ch = '1'; if (B.is_set(t)) ch = '2'; row += ch; }
                say("MAP " + std::string(y < 10 ? " " : "") + std::to_string(y) + " " + row); } });
    return out;
}

void decode(const std::vector<std::uint8_t>& memory) {
    stored.clear();
    map_value = 0;
    const std::size_t size = memory.size();
    if (size < 3 || memory[size - 1] != MAGIC) return;
    const std::size_t n = memory[size - 3];
    if (size < 3 + 3 * n) return;
    map_value = memory[size - 2];
    const int width = bc::map::width(), height = bc::map::height();
    for (std::size_t i = 0, at = size - 3 - 3 * n; i < n; ++i, at += 3) {
        const vec2 a{memory[at], memory[at + 1]};
        const int off = memory[at + 2];
        const vec2 b{a.x + off / 7 - REACH, a.y + off % 7 - REACH};
        if (a.x >= width || a.y >= height || b.x < 0 || b.y < 0 || b.x >= width || b.y >= height) continue;
        stored.push_back({a, b});
    }
}

const std::vector<Jump>& jumps() { return stored; }
int value() { return map_value; }
bool map_on() { return TP && !stored.empty() && map_value >= TP_BUY_VALUE; }

namespace {

struct Scratch {
    bc::distance_field me;
    bc::bitboard land{bc::uninitialized}, part{bc::uninitialized}, visited{bc::uninitialized}, root{bc::uninitialized};
};
Scratch& scratch() {
    static Scratch s;
    return s;
}

// Start the channel toward `land`, else the other landings in order (a hidden unit refuses one and is revealed).
bool launch(const bc::champion& me, vec2 land, const std::array<std::pair<int, vec2>, 64>& order, std::size_t n, int limit) {
    auto try_one = [&](vec2 tile) {
        try {
            if (me.consumables()[bc::consumable_type::tp_crystal].use(tile)) {
                carrying = false;
                seen::reset();
                trace::act("crystal");
                LOG(trace::audit.push_back(format("crystal: launch to %s", at(tile).c_str())););
                return true;
            }
        } catch (const bc::error& error) {
            LOG(trace::audit.push_back("crystal refused: " + error_text(error)););
            if (error.code() == bc::error_code::invalid_action_use_consumable_not_owned) carrying = false;
        }
        return false;
    };
    if (try_one(land)) return true;
    if (!carrying) return false;
    for (std::size_t i = 0; i < n; ++i) {
        if (order[i].first > limit) break;
        if (order[i].second != land && try_one(order[i].second)) return true;
        if (!carrying) return false;
    }
    return false;
}

}  // namespace

int travel(const bc::champion& me, vec2 origin, const bc::distance_field& field, const bc::bitboard& reached,
           const bc::bitboard& blocked, const bc::bitboard& goals, bool owns_dash, vec2& pad) {
    if (!armed()) return 0;
    if (walk_guard && walk_guard->is_set(origin)) return 0;
    const int far_steps = 1000;
    int here = reached.is_set(origin) ? static_cast<int>(field.get(origin)) : far_steps;
    if (here < CHANNEL + TP_SAVE + 1) return 0;
    auto& s = scratch();
    // A dasher's walk may hop thin holes: compare against that.
    if (owns_dash && !thin_holes().is_empty())
        if (const auto hop = hop_steps(origin, goals); hop && *hop < here) here = *hop;
    if (here < CHANNEL + TP_SAVE + 1) return 0;
    int limit = here - TP_SAVE;
    // Launch from here: the legal landings within REACH, by walking steps left.
    fast::around_into(origin, static_cast<float>(REACH), s.land);
    fast::not_into(blocked, s.part);
    s.land &= s.part;
    s.land &= reached;
    if (walk_guard) {
        fast::not_into(*walk_guard, s.part);
        s.land &= s.part;
    }
    std::array<vec2, 64> tiles;
    const auto n = std::min<std::size_t>(s.land.positions_into(tiles), tiles.size());
    std::array<std::pair<int, vec2>, 64> order;
    for (std::size_t i = 0; i < n; ++i) order[i] = {CHANNEL + static_cast<int>(field.get(tiles[i])), tiles[i]};
    std::sort(order.begin(), order.begin() + static_cast<std::ptrdiff_t>(n),
              [](const auto& x, const auto& y) { return x.first < y.first; });
    const bool now = n > 0 && order[0].first <= limit;
    if (now) limit = order[0].first - 1;  // a launch tile must beat launching here
    // Stored jumps, both ways: walk to one end, land on the other.
    struct Option {
        vec2 a, b;
        int lower, fb;
    };
    std::array<Option, 32> options;
    std::size_t m = 0;
    for (const auto& j : stored)
        for (int dir = 0; dir < 2 && m < options.size(); ++dir) {
            const vec2 a = dir ? j.b : j.a, b = dir ? j.a : j.b;
            const int lower = distance(origin, a) + CHANNEL;
            if (lower > limit || !reached.is_set(b) || blocked.is_set(b)) continue;
            const int fb = field.get(b);
            if (lower + fb > limit) continue;
            if (walk_guard && walk_guard->is_set(b)) continue;
            options[m++] = {a, b, lower, fb};
        }
    std::optional<Option> pick;
    int pick_cost = limit + 1;
    if (m > 0) {
        fast::around_into(origin, 0, s.root);
        s.me.compute(blocked, s.root, s.visited, static_cast<std::uint16_t>(std::max(0, limit - CHANNEL)));
        for (std::size_t i = 0; i < m; ++i) {
            const auto& o = options[i];
            if (!s.visited.is_set(o.a)) continue;
            const int cost = static_cast<int>(s.me.get(o.a)) + CHANNEL + o.fb;
            if (cost < pick_cost) pick_cost = cost, pick = o;
        }
    }
    if (pick && pick->a != origin) {
        pad = pick->a;
        LOG(trace::audit.push_back(format("crystal: walk to launch tile %s for %s (%d steps vs %d walking)", at(pick->a).c_str(),
                                          at(pick->b).c_str(), pick_cost, here)););
        return 2;
    }
    if (pick && pick->a == origin) {
        LOG(trace::audit.push_back(format("crystal: on launch tile, %d steps vs %d walking", pick_cost, here)););
        return launch(me, pick->b, order, n, pick_cost) ? 1 : 0;
    }
    if (now) {
        LOG(trace::audit.push_back(format("crystal: launch here, %d steps vs %d walking", order[0].first, here)););
        return launch(me, order[0].second, order, n, order[0].first) ? 1 : 0;
    }
    return 0;
}

}  // namespace crystal
