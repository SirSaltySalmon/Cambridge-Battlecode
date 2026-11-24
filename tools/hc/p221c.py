"""v221 step c: leave the spawn toward our own job; job picks without engine reads per candidate (run once)."""
import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch

B = 'bots/v221/src/'

p = Patch(B + 'bot.h')
p.after("""    int job_bag_bar(vec2 position) const;""", """
    // Per group: its camps' full health and strength, weakest first (setup), for the clear estimate without reads.
    std::vector<std::vector<std::pair<float, float>>> job_camps;
    double job_take(int s, double damage, double& rounds) const;""")
p.save()

p = Patch(B + 'spread.cpp')
# setup: keep the sorted camps
p.rep("""        spread_owner.assign(groups, -1);
        job_base.assign(groups, 0);
        int count = 0;
        for (int s = 0; s < groups; ++s) {
            if (is_monolith(s)) continue;
            std::array<double, 24> hps{}, strs{};
            std::size_t c = 0;
            for (const auto& camp : route[s]->camps())
                if (c < hps.size()) {
                    hps[c] = kind_health(camp.kind());
                    strs[c++] = camp_kind_strength(camp.kind());
                }
            std::array<std::size_t, 24> order{};
            for (std::size_t i = 0; i < c; ++i) order[i] = i;
            std::sort(order.begin(), order.begin() + c, [&](std::size_t a, std::size_t b) { return hps[a] < hps[b]; });
            double spent = 0, take = 0;
            for (std::size_t i = 0; i < c; ++i) {
                spent += hps[order[i]];
                take += strs[order[i]] * std::ceil(spent / JOB_BASE_DAMAGE);
            }
            job_base[s] = take <= JOB_TAKE * JOB_BASE_HEALTH ? 1 : 0;
            count += job_base[s];
        }""", """        spread_owner.assign(groups, -1);
        job_base.assign(groups, 0);
        job_camps.assign(groups, {});
        int count = 0;
        for (int s = 0; s < groups; ++s) {
            auto& list = job_camps[s];
            for (const auto& camp : route[s]->camps())
                list.push_back({static_cast<float>(kind_health(camp.kind())), static_cast<float>(camp_kind_strength(camp.kind()))});
            std::sort(list.begin(), list.end());
            if (is_monolith(s)) continue;
            double rounds = 0;
            job_base[s] = job_take(s, JOB_BASE_DAMAGE, rounds) <= JOB_TAKE * JOB_BASE_HEALTH ? 1 : 0;
            count += job_base[s];
        }""")
p.before("""int Bot::job_bag_bar(vec2 position) const {""", """double Bot::job_take(int s, double damage, double& rounds) const {
    // The damage a lone champion takes clearing a full group s, its camps dying weakest first, each hitting until it dies.
    double spent = 0, take = 0;
    for (const auto& [hp, strength] : job_camps[s]) {
        spent += hp;
        take += strength * std::ceil(spent / damage);
    }
    rounds = std::ceil(spent / damage);
    return take;
}

""")
# candidates: cheap tests first, the static estimate unless it is the job we are on (its camps are in sight)
p.rep("""        if (distance(position, centers[u]) > JOB_MAX_WALK) continue;
        if (u >= static_cast<int>(goal_fields.size()) || !goal_fields[u]) continue;
        const auto steps = goal_fields[u]->field.get(position);
        if (steps == bc::distance_field::unreachable || static_cast<int>(steps) > JOB_MAX_WALK) continue;
        const int walk = static_cast<int>(steps);
        const bool reward = rewards.count(u) > 0;
        const int ready = reward ? 0 : std::max(0, ready_at[u] - now);
        if (ready > walk + JOB_WAIT) continue;
        if (reward && u != spread_job && held_reward(u, &me)) continue;
        if (spread_danger(u, now)) continue;
        double rounds = 0;
        if (!reward && spread_clear(u, damage, rounds) * taken > JOB_TAKE * health) continue;
        cands.push_back({u, values[aggressive][u], rounds, ready, walk});""", """        const int air = distance(position, centers[u]);
        if (air > JOB_MAX_WALK) continue;
        const bool reward = rewards.count(u) > 0;
        const int ready = reward ? 0 : std::max(0, ready_at[u] - now);
        if (ready > JOB_MAX_WALK + JOB_WAIT) continue;
        double rounds = 0;
        if (!reward && (u == spread_job ? spread_clear(u, damage, rounds) : job_take(u, damage, rounds)) * taken > JOB_TAKE * health)
            continue;
        if (u >= static_cast<int>(goal_fields.size()) || !goal_fields[u]) continue;
        const auto steps = goal_fields[u]->field.get(position);
        if (steps == bc::distance_field::unreachable || static_cast<int>(steps) > JOB_MAX_WALK) continue;
        const int walk = static_cast<int>(steps);
        if (ready > walk + JOB_WAIT) continue;
        if (reward && u != spread_job && held_reward(u, &me)) continue;
        if (spread_danger(u, now)) continue;
        cands.push_back({u, values[aggressive][u], rounds, ready, walk});""")
p.save()

p = Patch(B + 'turn.cpp')
p.rep("""            const int leave_to = prism_team() && is_collector(me) && capture_slot < static_cast<int>(route.size())
                ? capture_slot : slot % static_cast<int>(route.size());""", """            // v221 (JOBS): toward our own job (picked here when we have none): leaving toward the shared target and the
            // job's walk back over the spawn undid each other (04barbell: 4,411 spawn.leave turns, 0 gems).
            if (JOBS && spread_on() && mode == FARM && seen::champions(Team::enemy).empty()) {
                const int now_round = static_cast<int>(bc::round_number());
                if (spread_job >= 0 && ready_at[spread_job] > now_round + JOB_MAX_WALK) spread_job = -1;
                if (spread_job < 0 && now_round >= spread_cool_until) {
                    spread_job = job_pick(me);
                    spread_pick_round = now_round;
                }
            }
            const int leave_to = prism_team() && is_collector(me) && capture_slot < static_cast<int>(route.size())
                ? capture_slot : JOBS && spread_job >= 0 ? spread_job : slot % static_cast<int>(route.size());""")
p.rep("""    return (gems >= bank_bar() || (idle && gems >= IDLE_BANK)) && bank_gems(me, {}, "spawn.bank");""",
      """    if (JOBS && spread_on()) return gems >= job_bag_bar(position) && bank_gems(me, {}, "spawn.bank");
    return (gems >= bank_bar() || (idle && gems >= IDLE_BANK)) && bank_gems(me, {}, "spawn.bank");""")
p.save()
print('ok')
