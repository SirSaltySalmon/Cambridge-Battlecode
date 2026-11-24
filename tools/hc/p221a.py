"""v221 step a: the JOBS allocator in spread.cpp and the shared-table filter in route.cpp (run once from the repo root)."""
import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch

B = 'bots/v221/src/'
p = Patch(B + 'spread.cpp')
p.rep("""bool Bot::spread_on() const {
    return SPREAD && spread_ready && spread_units > 0;
}""", """bool Bot::spread_on() const {
    if (JOBS) return jobs_ready;
    return SPREAD && spread_ready && spread_units > 0;
}""")
p.rep("""    spread_ready = true;
    spread_unit_of.fill(-1);
    spread_units = 0;
""", """    spread_ready = true;
    spread_unit_of.fill(-1);
    spread_units = 0;
    if (JOBS) {
        // v221 (JOBS): no units or bands. job_base: a bare champion (JOB_BASE_DAMAGE a round, JOB_BASE_HEALTH) clears the
        // group alone, weakest camp first, for under JOB_TAKE of its health.
        const int groups = static_cast<int>(route.size());
        spread_owner.assign(groups, -1);
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
        }
        jobs_ready = groups > 0;
        LOG(say(format("setup: jobs on, %d of %d groups are bare-solo", count, groups)););
        return;
    }
""")
p.rep("""int Bot::spread_pick(const bc::champion& me) {
    const int n = static_cast<int>(route.size());""", """int Bot::job_pick(const bc::champion& me) {
    // v221 (JOBS): the best job for us after the allies in sight took theirs. Candidates and our own rates first (path
    // steps, our damage and health); then a greedy deal over us and those allies, best rate first, lower id on a tie.
    const int n = static_cast<int>(route.size());
    const int now = static_cast<int>(bc::round_number());
    const auto position = me.position();
    const auto passives = owned_of(me).passives;
    const double damage = std::max(1.0, me.strength() * camp_dmg_mult(passives) + SAT_FOCUS_SHARE * me.focus());
    const double health = me.health();
    const double taken = camp_taken_mult(passives);
    struct Cand {
        int s;
        double value, rounds;
        int ready;  // rounds from now until it is up (0: up or its reward lies there)
        int steps;  // our walking steps
    };
    static std::vector<Cand> cands;
    cands.clear();
    for (int u = 0; u < n; ++u) {
        if (is_monolith(u) || u == capture_slot || values[aggressive][u] <= 0) continue;
        if (distance(position, centers[u]) > JOB_MAX_WALK) continue;
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
        cands.push_back({u, values[aggressive][u], rounds, ready, walk});
    }
    if (cands.empty()) return -1;
    struct Agent {
        vec2 at;
        int ident;
        bool dealt;
    };
    std::array<Agent, 8> agents{};
    int agent_n = 0;
    const int mine = cached_ident(me);
    agents[agent_n++] = {position, mine, false};
    {
        const auto& allies = seen::champions(Team::ally);
        const auto& tiles = seen::positions(Team::ally);
        for (std::size_t i = 0; i < allies.size() && i < tiles.size() && agent_n < static_cast<int>(agents.size()); ++i) {
            if (allies[i] == me || channelling(allies[i])) continue;
            agents[agent_n++] = {tiles[i], cached_ident(allies[i]), false};
        }
    }
    static std::vector<char> gone;
    gone.assign(cands.size(), 0);
    const double capture = capture_rounds;
    for (int deal = 0; deal < agent_n; ++deal) {
        int best_a = -1, best_c = -1;
        double best_rate = 0;
        for (int a = 0; a < agent_n; ++a) {
            if (agents[a].dealt) continue;
            for (std::size_t c = 0; c < cands.size(); ++c) {
                if (gone[c]) continue;
                const Cand& k = cands[c];
                const int walk = a == 0 ? k.steps : distance(agents[a].at, centers[k.s]);
                double rate = k.value / (std::max(walk, k.ready) + k.rounds + capture);
                if (a == 0 && k.s == spread_job) rate *= JOB_KEEP;
                if (best_a < 0 || rate > best_rate || (rate == best_rate && agents[a].ident < agents[best_a].ident)) {
                    best_a = a;
                    best_c = static_cast<int>(c);
                    best_rate = rate;
                }
            }
        }
        if (best_a < 0) return -1;
        if (best_a == 0) return cands[best_c].s;
        agents[best_a].dealt = true;
        gone[best_c] = 1;
    }
    return -1;
}

bool Bot::job_committed(const bc::champion& me) {
    // v221 (JOBS): within JOB_COMMIT_RANGE of a started shared target that is no bare-solo group: the team's job.
    if (route.empty()) return false;
    const int s = slot % static_cast<int>(route.size());
    if (s < static_cast<int>(job_base.size()) && job_base[s]) return false;
    return distance(me.position(), centers[s]) <= JOB_COMMIT_RANGE && mid_clear(s);
}

int Bot::spread_pick(const bc::champion& me) {
    if (JOBS) return job_pick(me);
    const int n = static_cast<int>(route.size());""")
p.rep("""    if (!spread_on() || spread_unit < 0 || route.empty()) return false;
    const int now = static_cast<int>(bc::round_number());""", """    if (!spread_on() || (!JOBS && spread_unit < 0) || route.empty()) return false;
    const int now = static_cast<int>(bc::round_number());""")
p.rep("""        spread_cool_until = now + SPREAD_ENEMY_COOL;""", """        spread_cool_until = now + (JOBS ? JOB_ENEMY_COOL : SPREAD_ENEMY_COOL);""")
p.rep("""    if (mode == FIGHT && distance(me.position(), centers[s]) <= SPREAD_HELP) {""",
      """    if (mode == FIGHT && distance(me.position(), centers[s]) <= (JOBS ? JOB_HELP : SPREAD_HELP)) {""")
p.rep("""    if (me.health() < me.max_health() * SPREAD_HEALTH) {""",
      """    // v221 (JOBS): no health bar: a job must cost under JOB_TAKE of the health we have, and clusters cost none.
    if (!JOBS && me.health() < me.max_health() * SPREAD_HEALTH) {""")
p.rep("""    if (static_cast<int>(me.gem_count()) >= SPREAD_BAG) {""",
      """    if (static_cast<int>(me.gem_count()) >= (JOBS ? JOB_BAG : SPREAD_BAG)) {""")
p.rep("""    if (me.champion_name() == Champ::cassi || me.champion_name() == Champ::issac) {
        bool partner = false;""", """    if (JOBS && spread_job < 0 && job_committed(me)) return false;
    if (!JOBS && (me.champion_name() == Champ::cassi || me.champion_name() == Champ::issac)) {
        bool partner = false;""")
p.rep("""    if (spread_job < 0 || now - spread_pick_round >= SPREAD_REPICK) {""",
      """    if (spread_job < 0 || now - spread_pick_round >= (JOBS ? JOB_REPICK : SPREAD_REPICK)) {""")
p.rep("""            else spread_drop("nothing in our band");""",
      """            else spread_drop(JOBS ? "no job for us" : "nothing in our band");""")
p.rep("""    farm(me, nullptr, "spread");""", """    farm(me, nullptr, JOBS ? "job" : "spread");""")
p.save()

p = Patch(B + 'route.cpp')
p.before("""    // v114 (HARD_GATE): last, so its fallback only looks at groups every other rule left open.
    // v202 (PT_GATE): by power-to-threat.
    if (PT_GATE && pt_on())""", """    // v221 (JOBS): groups a bare champion clears alone are left to the jobs, unless nothing else is in the table (the
    // main team's table only: pair_filter 0).
    if (JOBS && spread_on() && pair_filter == 0) {
        bool other = false;
        for (int s = 0; s < n && !other; ++s)
            if (result.has(s) && !job_base[s]) other = true;
        if (other)
            for (int s = 0; s < n; ++s)
                if (result.has(s) && job_base[s]) {
                    result.steps[s] = -1;
                    --result.reachable;
                }
    }
""")
p.save()
print('ok')
