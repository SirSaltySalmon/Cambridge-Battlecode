// v218 (MICRO_VESPER, MICRO_APPROACH; fight_coord_plan.md F5): per-tile fight movement for Vesper that acts on a
// projection (before the hurt dodge) and keeps her shot when it can. See MICRO_* in constants.h.
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

// A sturdy ally takes the enemy's hits for her: anyone but the fragile ranged (and herself).
bool sturdy_kind(Champ k) {
    switch (k) {
        case Champ::vesper_vale:
        case Champ::willow:
        case Champ::prism:
        case Champ::stellar:
        case Champ::cassi: return false;
        default: return true;
    }
}

// One engine read for a champion's whole record (tile, health, strength, range, state); false when out of sight.
bool read_info(const bc::champion& c, bc::detail::champion_info& out) {
    const auto handle = static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(c));
    return bc::detail::sdk_champion(handle, &out) == 0 && out.x >= 0 && out.y >= 0;
}

bool has(const bc::detail::champion_info& info, bc::champion_state flag) {
    return bc::champion_state::from_bits_retain(static_cast<std::uint32_t>(info.state)).contains(flag);
}

// What one visible enemy can do before our next turn (it acts about once in between: turns interleave A1 B1 A2 B2).
struct Threat {
    vec2 at;
    int basic_reach = 0;  // walk (melee) + dash (ready) + attack range
    int melee_reach = -1; // a melee's walk + dash + hit (the approach guard); -1 for a ranged enemy
    int spell_reach = 0;  // best ready damaging spell: range + radius (+1 walk for a dash landing)
    int stun_reach = -1;  // a ready stun (cc, or a stunning dash landing)
    int reach = 0;        // the farthest any of the above (or the second turn) gets from its tile
    float basic = 0;      // strength, x2 with a ready attack reset
    float strength = 0;   // a second enemy turn's hit (the reset and spells are down by then)
    float spell = 0;
    float health = 0;
};

constexpr int W = 2;  // window half-size: the dash range
constexpr int SIDE = 2 * W + 1;
constexpr int MID = W * SIDE + W;

}  // namespace

bool Bot::micro_vesper(const bc::champion& me, const Target& target, const bc::bitboard* avoid, bc::bitboard& unsafe,
                       bool& has_unsafe) {
    has_unsafe = false;
    if ((!MICRO_VESPER && !MICRO_APPROACH) || !target.champion() || bc::fuel_remaining() < MICRO_FUEL) return false;
    const auto& enemies = seen::champions(Team::enemy);
    const auto& enemy_tiles = seen::positions(Team::enemy);
    const vec2 aim = target.position();
    const vec2 here = seen::self_position();
    std::array<Threat, 8> threats{};
    std::size_t n = 0;
    int aim_i = -1;  // the target's threat (a kill shot takes it out of the danger)
    bool reaches = false;  // some threat reaches into the window
    for (std::size_t i = 0; i < enemies.size() && i < enemy_tiles.size() && n < threats.size(); ++i) {
        // Farther than any reach into the window (dash 2 + walk 1 + spell 4 + radius 1, two turns): no reads at all.
        const int gap = distance(here, enemy_tiles[i]);
        if (gap > W + 7) continue;
        const auto& e = enemies[i];
        try {
            bc::detail::champion_info info{};
            if (!read_info(e, info) || info.health <= 0) continue;
            if (has(info, bc::champion_state::recalling) || has(info, bc::champion_state::submitting_gems)) continue;
            const bool can_cast = has(info, bc::champion_state::can_cast);
            Threat t;
            t.at = enemy_tiles[i];
            const bool melee = melee_kind(e.champion_name());
            int dash = 0;
            bool reset = false;
            for (const auto& slot : e.abilities()) {
                // ability::is_ready(), with the owner's state from the record above.
                if (!can_cast || !slot.info() || slot.cooldown() > 0) continue;
                const int range = static_cast<int>(ability_range(slot));
                const int radius = static_cast<int>(ability_radius(slot));
                if (slot.is(Kind::attack_reset)) {
                    reset = true;
                } else if (slot.is(Kind::dash)) {
                    dash = std::max(dash, range);
                    const float hit = slot.damage_amount();
                    if (hit > 0) {  // Heart of Ruin's landing: walk-compatible, stuns
                        const int r = 1 + range + radius;
                        if (hit > t.spell) t.spell = hit, t.spell_reach = r;
                        t.stun_reach = std::max(t.stun_reach, r);
                    }
                } else if (slot.is(Kind::damage) || slot.is(Kind::aoe_damage) || slot.is(Kind::cc)) {
                    const int r = range + radius;
                    const float hit = slot.damage_amount();
                    if (hit > t.spell) t.spell = hit, t.spell_reach = r;
                    if (slot.is(Kind::cc)) t.stun_reach = std::max(t.stun_reach, r);
                }
            }
            t.strength = info.strength;
            t.health = info.health;
            t.basic = t.strength * (reset ? 2.0f : 1.0f);
            t.basic_reach = melee ? 2 + dash : 3 + dash;
            if (melee) t.melee_reach = t.basic_reach;
            t.reach = std::max({t.basic_reach + 1, t.spell_reach, t.stun_reach});
            if (gap - W <= t.reach) reaches = true;
            if (t.at == aim) aim_i = static_cast<int>(n);
            threats[n++] = t;
        } catch (const std::exception&) {
        }
    }
    if (n == 0) return false;
    // Our own record in one read.
    bc::detail::champion_info self{};
    if (!read_info(me, self)) return false;
    // Nothing reaches a window tile: no tile is in danger or bare, so v213's play as is (MICRO_SAFE_ZERO off: only over
    // the recall bar, where a zero-danger tile cannot trigger either).
    if (!reaches && (MICRO_SAFE_ZERO || self.health >= static_cast<float>(RECALL_HEALTH) * self.max_health)) return false;

    // Sturdy allies (not channelling): an enemy with one strictly closer to it than the tile hits it instead. Kept as
    // each threat's nearest-guard distance, so a tile's cover is one compare.
    std::array<int, 8> guard_d{};
    guard_d.fill(1 << 20);
    {
        const auto& allies = seen::champions(Team::ally);
        const auto& ally_tiles = seen::positions(Team::ally);
        for (std::size_t i = 0; i < allies.size() && i < ally_tiles.size(); ++i)
            try {
                if (allies[i] == me || !sturdy_kind(allies[i].champion_name())) continue;
                bc::detail::champion_info info{};
                if (!read_info(allies[i], info)) continue;
                if (has(info, bc::champion_state::recalling) || has(info, bc::champion_state::submitting_gems)) continue;
                for (std::size_t j = 0; j < n; ++j) guard_d[j] = std::min(guard_d[j], distance(ally_tiles[i], threats[j].at));
            } catch (const std::exception&) {
            }
    }
    const float cover = static_cast<float>(MICRO_COVER);

    const float health = self.health;
    const float low = static_cast<float>(RECALL_HEALTH) * self.max_health;
    const float bar = static_cast<float>(MICRO_DMG_BAR) * health;
    const int reach = self.attack_range;
    const bool can_attack = has(self, bc::champion_state::can_attack);
    const bool can_move = has(self, bc::champion_state::can_move);
    const auto dash = ability_of(me, Kind::dash);
    const bool dash_ready = dash && has(self, bc::champion_state::can_cast) && dash->cooldown() <= 0;
    const bool in_reach = distance(here, aim) <= reach;

    // One pass over the threats for a tile: the next enemy turn's damage (skip: a threat left out, the target a kill
    // shot removes), whether a ready stun reaches it, the second enemy turn (a melee walks one more tile and swings; a
    // ranged one shoots again from where it stands; spells and resets are down), the approach guard's bare reach, and
    // whether some enemy is in our reach from it.
    struct Tile {
        float next = 0, proj = 0;
        bool stun = false, bare = false, shoots = false;
    };
    auto tile = [&](vec2 t, int skip) {
        Tile r;
        float later = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (static_cast<int>(i) == skip) continue;
            const auto& e = threats[i];
            const int d = distance(t, e.at);
            if (d <= reach) r.shoots = true;
            if (d > e.reach) continue;
            const bool cov = guard_d[i] < d;
            float hit = 0;
            if (d <= e.basic_reach) hit += e.basic;
            if (d <= e.spell_reach) hit += e.spell;
            if (d <= e.stun_reach) r.stun = true;
            if (hit > 0) r.next += cov ? hit * cover : hit;
            if (d <= e.basic_reach + (e.melee_reach >= 0 ? 1 : 0)) later += cov ? e.strength * cover : e.strength;
            if (d <= e.melee_reach && !cov) r.bare = true;
        }
        // Stunned, we stand through the second enemy turn as well: basic hits from one step farther out land too.
        if (r.stun) {
            for (std::size_t i = 0; i < n; ++i)
                if (static_cast<int>(i) != skip && distance(t, threats[i].at) <= threats[i].basic_reach + 1) r.next += threats[i].basic;
            r.proj = r.next;
        } else {
            r.proj = r.next + later;
        }
        return r;
    };

    // Danger, shot and the approach guard over the window: the dash range when the dash is ready or we are closing in
    // (the approach's path keeps out of it), else the step range. Blocked tiles are not candidates.
    const int wr = dash_ready || !in_reach ? W : 1;
    static bc::bitboard blocked{bc::uninitialized};
    blocked_tiles_into(blocked);
    if (avoid) blocked |= *avoid;
    if (walk_guard && !walk_guard->is_set(here)) blocked |= *walk_guard;
    const int w = bc::map::width(), h = bc::map::height();
    std::array<float, SIDE * SIDE> dmg{}, proj{};
    std::array<signed char, SIDE * SIDE> shot{};  // 2 the target, 1 another enemy, 0 none, -1 not a candidate
    shot.fill(-1);
    std::array<bool, SIDE * SIDE> bare{};         // MICRO_APPROACH: an enemy melee (+ its dash) reaches it uncovered
    bool stun_here = false;
    // The trigger: standing there for the next two enemy turns (`proj`) drops us under the recall bar, or the next one
    // alone costs more than the bar. MICRO_SAFE_ZERO: a tile no enemy reaches next turn is safe (a turn to react).
    auto unsafe_k = [&](int k) {
        return (!MICRO_SAFE_ZERO || dmg[k] > 0) && (health - proj[k] < low || dmg[k] > bar);
    };
    for (int dy = -wr; dy <= wr; ++dy)
        for (int dx = -wr; dx <= wr; ++dx) {
            const vec2 t{here.x + dx, here.y + dy};
            const int k = (dy + W) * SIDE + (dx + W);
            if (t.x < 0 || t.y < 0 || t.x >= w || t.y >= h || ((dx || dy) && blocked.is_set(t))) continue;
            const Tile r = tile(t, -1);
            dmg[k] = r.next;
            proj[k] = r.proj;
            if (k == MID) stun_here = r.stun;
            bare[k] = MICRO_APPROACH && k != MID && r.bare;
            shot[k] = distance(t, aim) <= reach ? 2 : r.shoots ? 1 : 0;
        }
    const float here_dmg = dmg[MID];
    const int round_now = static_cast<int>(bc::round_number());

    if (!MICRO_VESPER || !unsafe_k(MID)) {
        // Standing is affordable: v213's play. On the approach, the window keeps out of (MICRO_APPROACH) tiles an enemy
        // melee reaches with no sturdy ally closer to it, and (MICRO_VESPER) for MICRO_REENTER rounds after a retreat,
        // tiles the model still calls unsafe (a tile whose danger dropped is free again).
        if (!in_reach) {
            const bool recent = MICRO_VESPER && round_now - micro_left_round <= MICRO_REENTER;
            if (!MICRO_APPROACH && !recent) return false;
            unsafe.clear();
            for (int k = 0; k < SIDE * SIDE; ++k)
                if (k != MID && shot[k] >= 0 && ((MICRO_APPROACH && bare[k]) || (recent && unsafe_k(k)))) {
                    unsafe.set(vec2{here.x + k % SIDE - W, here.y + k / SIDE - W});
                    has_unsafe = true;
                }
            if (has_unsafe) SUB(recent ? ".mguard.re" : ".mguard", "approach keeps out of uncovered melee reach");
        }
        return false;
    }

    // (a) Keep the shot: fire from here (v213's play, attack reset included) when the target is in reach and either
    // the hits kill it and the rest leaves us over the bar, or one more enemy turn here is affordable and the dash stays
    // ready for next turn's escape with no stun reaching us.
    if (shot[MID] == 2 && can_attack) {
        const auto reset = ability_of(me, Kind::attack_reset);
        const float hits = (reset && has(self, bc::champion_state::can_cast) && reset->cooldown() <= 0) ? 2.0f : 1.0f;
        if (aim_i >= 0 && threats[aim_i].health <= static_cast<float>(MICRO_KILL_SHARE) * hits * self.strength
            && health - tile(here, aim_i).next >= low) {
            SUB(".mfire", "standing is unsafe, but the hits kill the target: fire here");
            return false;
        }
        if (dash_ready && !stun_here && health - here_dmg >= low && here_dmg <= bar) {
            SUB(".mfire", "unsafe in two turns; fire here and keep the dash for next turn");
            return false;
        }
    }

    auto left = [&]() { micro_left_round = round_now; };
    // Dash landings. mode 0: safe with a shot (dash, then fire); 1: safe; 2: a shot with at most MICRO_DASH_KEEP of
    // the danger here. Then fire at what is in reach (the dash is an ability; the attack allowance is still ours).
    static bc::bitboard goals{bc::uninitialized};
    auto dash_to = [&](int mode) -> bool {
        if (!dash_ready || (mode != 1 && !can_attack)) return false;
        goals.clear();
        bool any = false;
        for (int k = 0; k < SIDE * SIDE; ++k) {
            if (k == MID || shot[k] < 0) continue;
            if (mode != 1 && shot[k] == 0) continue;
            if (mode == 2 ? dmg[k] > here_dmg * MICRO_DASH_KEEP : unsafe_k(k)) continue;
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
        left();
        if (!can_attack) return true;
        if (sighted(target) && alive(target) && attack_if_in_range(target)) return true;
        std::vector<bc::champion> sorted;
        for (const auto& e : before)
            if (sighted(Target{e})) sorted.push_back(e);
        std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.health() < b.health(); });
        for (const auto& e : sorted)
            if (attack_if_in_range(e)) break;
        return true;
    };
    // (b) A ready dash to a safe tile that still shoots.
    if (dash_to(0)) {
        SUB(".micro.dash", "unsafe here in two turns: dash to a safe tile and fire");
        return true;
    }
    // (c) A safe step (no shot this turn): the least danger, then the nearest to a shot at the target. The dash stays.
    if (can_move) {
        std::array<int, 8> order{};
        int m = 0;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const int k = (dy + W) * SIDE + (dx + W);
                if (k != MID && shot[k] >= 0 && !unsafe_k(k)) order[m++] = k;
            }
        auto gap = [&](int k) { return std::abs(distance(vec2{here.x + k % SIDE - W, here.y + k / SIDE - W}, aim) - reach); };
        std::sort(order.begin(), order.begin() + m, [&](int a, int b) { return dmg[a] != dmg[b] ? dmg[a] < dmg[b] : gap(a) < gap(b); });
        for (int i = 0; i < m; ++i) {
            const vec2 t{here.x + order[i] % SIDE - W, here.y + order[i] / SIDE - W};
            bool moved = false;
            try {
                moved = me.move_absolute(t);
            } catch (const bc::error&) {
                continue;  // known occupied (a unit our board missed): the next tile
            }
            if (moved) {
                trace::moved("move");
                left();
                SUB(".micro.step", "unsafe here in two turns: step to a safe tile");
                return true;
            }
            seen::reset();  // a hidden unit in a bush refused the step (the walk is still ours)
        }
    }
    // (c2) A safe dash landing without a shot; (c3) a dash to a shot with less danger.
    if (dash_to(1)) {
        SUB(".micro.dash", "unsafe here in two turns, no safe step: dash to a safe tile");
        return true;
    }
    if (dash_to(2)) {
        SUB(".micro.dash", "unsafe here, nothing safe: dash to a shot with less danger");
        return true;
    }
    // (d) v213's play.
    SUB(".mnone", "unsafe here, no better tile");
    return false;
}
