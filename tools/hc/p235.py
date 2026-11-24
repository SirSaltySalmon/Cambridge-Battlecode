"""v235 = bots/v231 + RALLY_HIT (run once from the repo root, after copying bots/v231 to bots/v235).
Reasons and benches: bots/v235/README.md."""
import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch

B = 'bots/v235/src/'
p = Patch(B + 'constants.h')
p.after("inline constexpr int RALLY_FOE_RANGE = 6, RALLY_MATE_RANGE = 4;\n", """// RALLY_HIT (v235): the same call from a champion about to hit an enemy champion (target in reach, a melee one step
//   more), not only from one that lost PING_MIN_LOSS health last round. Same numbers test.
inline constexpr bool RALLY_HIT = true;
""")
p.save()
p = Patch(B + 'bot.h')
p.after("    void commit_ping(const bc::champion& me);\n", "    void rally_hit(const bc::champion& me, const bc::champion& target);\n")
p.save()
p = Patch(B + 'combat.cpp')
p.rep("""    commit_ping(me);
    mark_ghost(*target);
    engage(me, *target, enemies, false, avoid);""", """    commit_ping(me);
    rally_hit(me, *target);
    mark_ghost(*target);
    engage(me, *target, enemies, false, avoid);""")
p.before("bool Bot::ping_busy(const bc::champion& me) {", """void Bot::rally_hit(const bc::champion& me, const bc::champion& target) {
    // v235 (RALLY_HIT): we are about to hit an enemy champion in a fight that is not ours by numbers: call the team.
    // Someone else's fresh ping stands; our own is refreshed while the fight goes on.
    if (!RALLY || !RALLY_HIT || route.empty()) return;
    const int now = turn_round;
    const bool own_fresh = ping_mine_round >= now - PING_FRESH;
    if (ping_slot && !own_fresh) return;
    const int reach = static_cast<int>(me.attack_range());
    if (!within(me, target, reach + (reach <= 1 ? 1 : 0))) return;
    const vec2 position = seen::self_position();
    int foes = 0, mates = 0;
    for (const auto tile : seen::positions(Team::enemy))
        if (distance(position, tile) <= RALLY_FOE_RANGE) ++foes;
    for (const auto tile : seen::positions(Team::ally))
        if (distance(position, tile) <= RALLY_MATE_RANGE) ++mates;
    if (mates > foes) return;
    int best = 0;
    for (int s = 1; s < static_cast<int>(centers.size()); ++s)
        if (distance(position, centers[s]) < distance(position, centers[best])) best = s;
    if (best >= 63) return;
    set_shared_byte(PING_BYTE, (best + 1) | ((now & 3) << 6));
    ping_slot = best;
    ping_mine_slot = best;
    if (own_fresh) return;
    ping_mine_round = now;
    ping_mine_health = static_cast<int>(me.health());
    if (PING_LOG) {
        int dead = 0;
        for (const auto& e : seen::roster(Team::enemy))
            if (!e.is_alive()) ++dead;
        ping_mine_dead = dead;
    }
    NOTE(format("ping s%d: hitting an enemy champion, %d of us against %d here: rally", best, mates, foes));
}

""")
p.save()
print('v235 patched')
