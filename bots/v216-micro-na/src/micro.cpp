// v216 (MICRO_VESPER, fight_coord_plan.md F5): per-tile fight movement for Vesper. See MICRO_* in constants.h.
#include "bot.h"
#include "seen.h"
#include "state.h"
#include "trace.h"
#include <algorithm>
#include <array>

namespace {

bool melee_kind(Champ k) {
    switch (k) {
        case Champ::ryth:
        case Champ::vendetta:
        case Champ::issac:
        case Champ::marcella:
        case Champ::ren:
        case Champ::varg:
        case Champ::heart_of_ruin: return true;
        default: return false;
    }
}

// What one visible enemy can do before our next turn (it acts about once in between: turns interleave A1 B1 A2 B2).
struct Threat {
    vec2 at;
    int basic_reach = 0;  // walk (melee) + dash (ready) + attack range
    int spell_reach = 0;  // best ready damaging spell: range + radius (+1 walk for a dash landing)
    int stun_reach = -1;  // a ready stun (cc, or a stunning dash landing)
    float basic = 0;      // strength, x2 with a ready attack reset
    float spell = 0;
    float cover = 1;      // filled per tile
};

constexpr int W = 2;  // window half-size: the dash range
constexpr int SIDE = 2 * W + 1;

}  // namespace

bool Bot::micro_vesper(const bc::champion& me, const Target& target, const bc::bitboard* avoid, bc::bitboard& unsafe,
                       bool& has_unsafe) {
    has_unsafe = false;
    if (!MICRO_VESPER || !target.champion() || bc::fuel_remaining() < MICRO_FUEL) return false;
    const auto& enemies = seen::champions(Team::enemy);
    const auto& enemy_tiles = seen::positions(Team::enemy);
    std::array<Threat, 8> threats{};
    std::size_t n = 0;
    for (std::size_t i = 0; i < enemies.size() && i < enemy_tiles.size() && n < threats.size(); ++i) {
        const auto& e = enemies[i];
        try {
            if (!alive(e) || channelling(e)) continue;
            Threat t;
            t.at = enemy_tiles[i];
            const bool melee = melee_kind(e.champion_name());
            int dash = 0;
            bool reset = false;
            for (const auto& slot : e.abilities()) {
                if (!slot.is_ready()) continue;
                const int range = static_cast<int>(ability_range(slot));
                const int radius = static_cast<int>(ability_radius(slot));
                if (slot.is(Kind::attack_reset)) {
                    reset = true;
                } else if (slot.is(Kind::dash)) {
                    dash = std::max(dash, range);
                    const float hit = slot.damage_amount();
                    if (hit > 0) {  // Heart of Ruin's landing: walk-compatible, stuns
                        const int reach = 1 + range + radius;
                        if (hit > t.spell) t.spell = hit, t.spell_reach = reach;
                        t.stun_reach = std::max(t.stun_reach, reach);
                    }
                } else if (slot.is(Kind::damage) || slot.is(Kind::aoe_damage) || slot.is(Kind::cc)) {
                    const int reach = range + radius;
                    const float hit = slot.damage_amount();
                    if (hit > t.spell) t.spell = hit, t.spell_reach = reach;
                    if (slot.is(Kind::cc)) t.stun_reach = std::max(t.stun_reach, reach);
                }
            }
            t.basic = e.strength() * (reset ? 2.0f : 1.0f);
            t.basic_reach = melee ? 2 + dash : 3 + dash;
            threats[n++] = t;
        } catch (const std::exception&) {
        }
    }
    if (n == 0) return false;

    // Sturdy allies (melee, not channelling): an enemy with one strictly closer to it than the tile hits it instead.
    std::array<vec2, 8> guards{};
    std::size_t g_n = 0;
    {
        const auto& allies = seen::champions(Team::ally);
        const auto& ally_tiles = seen::positions(Team::ally);
        for (std::size_t i = 0; i < allies.size() && i < ally_tiles.size() && g_n < guards.size(); ++i)
            try {
                if (allies[i] != me && melee_kind(allies[i].champion_name()) && !channelling(allies[i])) guards[g_n++] = ally_tiles[i];
            } catch (const std::exception&) {
            }
    }

    const vec2 here = seen::self_position();
    auto danger = [&](vec2 t) {
        float sum = 0;
        bool stunned = false;
        for (std::size_t i = 0; i < n; ++i) {
            const auto& e = threats[i];
            const int d = distance(t, e.at);
            float hit = 0;
            if (d <= e.basic_reach) hit += e.basic;
            if (d <= e.spell_reach) hit += e.spell;
            if (d <= e.stun_reach) stunned = true;
            if (hit <= 0) continue;
            for (std::size_t k = 0; k < g_n; ++k)
                if (distance(guards[k], e.at) < d) {
                    hit *= static_cast<float>(MICRO_COVER);
                    break;
                }
            sum += hit;
        }
        // Stunned, we stand there one more enemy turn: basic hits from one step farther out land too.
        if (stunned)
            for (std::size_t i = 0; i < n; ++i)
                if (distance(t, threats[i].at) <= threats[i].basic_reach + 1) sum += threats[i].basic;
        return sum;
    };

    const float health = me.health();
    const float bar = static_cast<float>(MICRO_DMG_BAR) * health;
    const int reach = me.attack_range();
    const vec2 aim = target.position();
    // Danger and shot over the window (the dash range); blocked tiles are not candidates.
    static bc::bitboard blocked{bc::uninitialized};
    blocked_tiles_into(blocked);
    if (avoid) blocked |= *avoid;
    if (walk_guard && !walk_guard->is_set(here)) blocked |= *walk_guard;
    const int w = bc::map::width(), h = bc::map::height();
    std::array<float, SIDE * SIDE> dmg{};
    std::array<signed char, SIDE * SIDE> shot{};  // 2 the target, 1 another enemy, 0 none, -1 not a candidate
    for (int dy = -W; dy <= W; ++dy)
        for (int dx = -W; dx <= W; ++dx) {
            const vec2 t{here.x + dx, here.y + dy};
            const int k = (dy + W) * SIDE + (dx + W);
            if (t.x < 0 || t.y < 0 || t.x >= w || t.y >= h || ((dx || dy) && blocked.is_set(t))) {
                shot[k] = -1;
                continue;
            }
            dmg[k] = danger(t);
            signed char s = distance(t, aim) <= reach ? 2 : 0;
            for (std::size_t i = 0; i < n && s == 0; ++i)
                if (distance(t, threats[i].at) <= reach) s = 1;
            shot[k] = s;
        }
    const int mid = W * SIDE + W;
    const float here_dmg = dmg[mid];

    if (here_dmg <= bar) {
        // Standing is affordable: v213's play. On the approach, the unsafe window tiles stay out of the walk and dash.
        if (MICRO_APPROACH && !within(me, target, reach)) {
            unsafe.clear();
            for (int k = 0; k < SIDE * SIDE; ++k)
                if (k != mid && shot[k] >= 0 && dmg[k] > bar) {
                    unsafe.set(vec2{here.x + k % SIDE - W, here.y + k / SIDE - W});
                    has_unsafe = true;
                }
            if (has_unsafe) NOTE(format("micro: approach keeps out of %d unsafe tiles (bar %.0f)", unsafe.count(), bar));
        }
        return false;
    }

    const auto dash = ability_of(me, Kind::dash);
    const bool dash_ready = dash && dash->is_ready();
    // 1. A safe dash landing that still shoots; 3. else one that shoots at <= MICRO_DASH_KEEP of the danger here.
    auto dash_to = [&](bool safe_only) -> bool {
        if (!dash_ready || !me.can_attack()) return false;
        bc::bitboard goals;
        bool any = false;
        for (int k = 0; k < SIDE * SIDE; ++k) {
            if (k == mid || shot[k] <= 0) continue;
            if (safe_only ? dmg[k] > bar : dmg[k] > here_dmg * MICRO_DASH_KEEP) continue;
            goals.set(vec2{here.x + k % SIDE - W, here.y + k / SIDE - W});
            any = true;
        }
        if (!any) return false;
        Go opt;
        opt.walk = false;
        opt.dash = DashUse::urgent;
        opt.guard_dash = GO_GUARD_DASH;
        opt.avoid = avoid;
        opt.rank = [&](vec2 t) -> std::optional<long> {
            if (!goals.is_set(t)) return std::nullopt;
            const int k = (t.y - here.y + W) * SIDE + (t.x - here.x + W);
            return shot[k] * 1'000'000L - static_cast<long>(dmg[k]);
        };
        // The vision list is read before the dash: an enemy it leaves out of sight would throw on any read.
        const auto before = enemies;
        if (!go(goals, opt)) return false;
        if (sighted(target) && alive(target) && attack_if_in_range(target)) return true;
        std::vector<bc::champion> sorted;
        for (const auto& e : before)
            if (sighted(Target{e})) sorted.push_back(e);
        std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.health() < b.health(); });
        for (const auto& e : sorted)
            if (attack_if_in_range(e)) break;
        return true;
    };
    if (dash_to(true)) {
        SUB(".micro", format("standing takes ~%.0f (bar %.0f): dash to a safe tile and fire", here_dmg, bar));
        return true;
    }
    // 2. A safe walk (no shot this turn): the least danger, then the nearest to a shot at the target.
    if (me.can_move()) {
        int best = -1;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const int k = (dy + W) * SIDE + (dx + W);
                if (k == mid || shot[k] < 0 || dmg[k] > bar) continue;
                const vec2 t{here.x + dx, here.y + dy};
                if (best < 0) {
                    best = k;
                    continue;
                }
                const vec2 b{here.x + best % SIDE - W, here.y + best / SIDE - W};
                const int gt = std::abs(distance(t, aim) - reach), gb = std::abs(distance(b, aim) - reach);
                if (dmg[k] < dmg[best] || (dmg[k] == dmg[best] && gt < gb)) best = k;
            }
        if (best >= 0) {
            bc::bitboard goal;
            goal.set(vec2{here.x + best % SIDE - W, here.y + best / SIDE - W});
            Go opt;
            opt.dash = DashUse::keep;
            opt.avoid = avoid;
            if (go(goal, opt)) {
                SUB(".micro", format("standing takes ~%.0f (bar %.0f): step to a safe tile (~%.0f)", here_dmg, bar, dmg[best]));
                return true;
            }
        }
    }
    if (dash_to(false)) {
        SUB(".micro", format("standing takes ~%.0f (bar %.0f), nothing safe: dash to a shot with less", here_dmg, bar));
        return true;
    }
    NOTE(format("micro: standing takes ~%.0f (bar %.0f), no better tile: v213 play", here_dmg, bar));
    return false;
}
