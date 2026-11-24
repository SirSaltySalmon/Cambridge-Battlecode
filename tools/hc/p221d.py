"""v221 step d: crew jobs (groups no one clears alone), a staging patience, and tributes that stay on (run once)."""
import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch

B = 'bots/v221/src/'

p = Patch(B + 'constants.h')
p.after("""inline constexpr int JOB_BAG_MIN = 20, JOB_BAG_MAX = 80;
inline constexpr double JOB_BAG_STEP = 1.5;""", """
// Crew jobs: a group we cannot clear alone is a job for the smallest crew k (2..living allies) of champions like us that
//   can (k x our damage, under JOB_TAKE of k x our health); its rate is the value per member. The deal over allies in
//   sight seats k of them; out of sight the group is the meeting point: its members wait at the old stage gate
//   (stage_group) until enough power is there. One staged longer than JOB_STAGE_PATIENCE rounds drops the group for
//   JOB_STAGE_SKIP rounds. Without this (00ribs, 2,000 rounds, 792 gems): every champion always held a cluster job (four
//   walking 40 tiles to one respawning cluster), so the elk groups beside the spawn were never opened.
inline constexpr int JOB_STAGE_PATIENCE = 12, JOB_STAGE_SKIP = 40;
// v221 (TRIBUTE_STAY): damage never breaks a tribute, so an enemy in reach only ends it under TRIBUTE_STAY_HEALTH of our
//   health (00swirl vs an idle enemy on its spawn 3 tiles from the altar: 593 tributes started and cancelled).
inline constexpr bool TRIBUTE_STAY = true;
inline constexpr double TRIBUTE_STAY_HEALTH = 0.4;""")
p.save()

p = Patch(B + 'bot.h')
p.after("""    double job_take(int s, double damage, double& rounds) const;""", """
    // The crew our job needs (1: alone), and groups dropped after a long stage (by slot: the round it may be picked again).
    int job_crew = 1;
    std::vector<int> job_skip_until;""")
p.save()

p = Patch(B + 'spread.cpp')
p.rep("""        job_camps.assign(groups, {});
        int count = 0;""", """        job_camps.assign(groups, {});
        job_skip_until.assign(groups, 0);
        int count = 0;""")
p.rep("""    struct Cand {
        int s;
        double value, rounds;
        int ready;  // rounds from now until it is up (0: up or its reward lies there)
        int steps;  // our walking steps
    };""", """    struct Cand {
        int s;
        double value, rounds;
        int ready;  // rounds from now until it is up (0: up or its reward lies there)
        int steps;  // our walking steps
        int crew;   // champions like us it takes
    };
    const int living = allies_alive();""")
p.rep("""        double rounds = 0;
        if (!reward && (u == spread_job ? spread_clear(u, damage, rounds) : job_take(u, damage, rounds)) * taken > JOB_TAKE * health)
            continue;""", """        if (job_skip_until[u] > now) continue;
        double rounds = 0;
        int crew = 1;
        if (!reward && (u == spread_job ? spread_clear(u, damage, rounds) : job_take(u, damage, rounds)) * taken > JOB_TAKE * health) {
            crew = 0;
            for (int k = 2; k <= living && !crew; ++k)
                if (job_take(u, k * damage, rounds) * taken <= JOB_TAKE * k * health) crew = k;
            if (!crew) continue;
        }""")
p.rep("""        cands.push_back({u, values[aggressive][u], rounds, ready, walk});""",
      """        cands.push_back({u, values[aggressive][u], rounds, ready, walk, crew});""")
p.rep("""    static std::vector<char> gone;
    gone.assign(cands.size(), 0);""", """    static std::vector<int> gone;  // seats left
    gone.resize(cands.size());
    for (std::size_t c = 0; c < cands.size(); ++c) gone[c] = cands[c].crew;""")
p.rep("""                if (gone[c]) continue;
                const Cand& k = cands[c];
                const int walk = a == 0 ? k.steps : distance(agents[a].at, centers[k.s]);
                double rate = k.value / (std::max(walk, k.ready) + k.rounds + capture);""", """                if (gone[c] <= 0) continue;
                const Cand& k = cands[c];
                const int walk = a == 0 ? k.steps : distance(agents[a].at, centers[k.s]);
                double rate = k.value / k.crew / (std::max(walk, k.ready) + k.rounds + capture);""")
p.rep("""        if (best_a == 0) return cands[best_c].s;
        agents[best_a].dealt = true;
        gone[best_c] = 1;""", """        if (best_a == 0) {
            job_crew = cands[best_c].crew;
            return cands[best_c].s;
        }
        agents[best_a].dealt = true;
        --gone[best_c];""")
# spread_step: a crew job is kept while it is being cleared, staged at the gate, and dropped after a long stage
p.rep("""    if (spread_job < 0 || now - spread_pick_round >= (JOBS ? JOB_REPICK : SPREAD_REPICK)) {""",
      """    const bool crew_busy = JOBS && spread_job >= 0 && job_crew > 1 && mid_clear(spread_job);
    if (JOBS && spread_job >= 0 && job_crew > 1 && !crew_busy && stage_slot == spread_job && stage_last >= now - 1
        && now - stage_since >= JOB_STAGE_PATIENCE) {
        job_skip_until[spread_job] = now + JOB_STAGE_SKIP;
        spread_drop("staged too long: no crew came");
    }
    if (!crew_busy && (spread_job < 0 || now - spread_pick_round >= (JOBS ? JOB_REPICK : SPREAD_REPICK))) {""")
p.rep("""    slot = spread_job;
    spread_farming = true;
    farm(me, nullptr, JOBS ? "job" : "spread");""", """    slot = spread_job;
    // v221 (JOBS): a crew job keeps the stage gate (the crew opens it together); a job of our own needs none.
    spread_farming = !(JOBS && job_crew > 1);
    farm(me, nullptr, JOBS ? (job_crew > 1 ? "crew" : "job") : "spread");""")
p.save()

p = Patch(B + 'turn.cpp')
p.rep("""        // v71: someone hidden is hitting us, and keeps doing so until the channel would end: stop it.""",
      """        // v221 (TRIBUTE_STAY): for everyone, above TRIBUTE_STAY_HEALTH.
        if (TRIBUTE_STAY && threat && kind == "tribute" && me.health() >= me.max_health() * TRIBUTE_STAY_HEALTH) {
            threat = false;
            LOG(why = "stays on the tribute (damage does not break it): " + why;);
        }
        // v71: someone hidden is hitting us, and keeps doing so until the channel would end: stop it.""")
p.save()
print('ok')
