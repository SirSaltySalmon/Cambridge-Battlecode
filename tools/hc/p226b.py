"""v226 step b: leashed jobs on the Prism team (run once)."""
import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch

B = 'bots/v226/src/'

p = Patch(B + 'constants.h')
p.rep("inline constexpr bool JOBS = false;", "inline constexpr bool JOBS = true;")
p.rep("inline constexpr int JOB_MODE_OFF = 0, JOB_MODE_CARRY = 1, JOB_MODE_ALL = 2;", """inline constexpr int JOB_MODE_OFF = 0, JOB_MODE_CARRY = 1, JOB_MODE_ALL = 2, JOB_MODE_LEASH = 3;
// ---- v226 (JOB_MODE_LEASH): the Prism team's allies fan out on a leash. ----
// Platform (results/plat): everyone on jobs with no leash lost cluster maps 2-24 (7.0 deaths by r600: lone champions
//   lose the 1v1s and 1v2s) where the Prism team, one group and one collector, went 8-18 at 2.7 deaths but collected
//   300 gems by r300 to the opponents' 387-420. Carry-only jobs on ordinary maps were level on the platform (24 vs 26
//   of 100, flips 8 up / 10 down) for +4% gems unopposed, so they are off (JOB_CARRY_ON).
// LEASH: on a Prism-team map every ally but Prism takes pure-cluster groups within JOB_LEASH tiles of her (of the shared
//   target while she is out of sight), dealt among the allies in sight; no shared claim (the old bytes stay the old
//   claims and her cash-in altar). She walks the shared target as before, and a reward is hers only within
//   PRISM_WANT_RANGE of her. The team stays within a few steps of each other and captures in parallel.
// JOB_ALL_MAPS: mode ALL on cluster maps instead (no Prism team). JOB_CARRY_ON: mode CARRY on ordinary maps.
inline constexpr bool JOB_ALL_MAPS = false, JOB_CARRY_ON = false;
inline constexpr bool JOB_LEASH_ON = true;
inline constexpr int JOB_LEASH = 8;""")
p.rep("inline constexpr bool PRISM_TEAM = !JOBS && !(DRAFT_SET > 0 && DRAFT_SET_CLUSTER);",
      "inline constexpr bool PRISM_TEAM = !(JOBS && JOB_ALL_MAPS) && !(DRAFT_SET > 0 && DRAFT_SET_CLUSTER);")
p.rep("inline constexpr int PRISM_WANT_RANGE = 8;", "inline constexpr int PRISM_WANT_RANGE = 1;")
p.save()

p = Patch(B + 'spread.cpp')
p.rep("        job_mode = !jobs_ready ? JOB_MODE_OFF : draft::cluster_map() ? JOB_MODE_ALL : JOB_MODE_CARRY;",
      """        job_mode = !jobs_ready ? JOB_MODE_OFF
            : draft::cluster_map() ? (JOB_ALL_MAPS ? JOB_MODE_ALL : JOB_LEASH_ON && prism_team() ? JOB_MODE_LEASH : JOB_MODE_OFF)
            : JOB_CARRY_ON ? JOB_MODE_CARRY : JOB_MODE_OFF;""")
p.rep("""        if (job_mode == JOB_MODE_OFF) jobs_ready = false;
        if (JOB_CLAIMS) set_jobs_layout(job_mode);""", """        if (job_mode == JOB_MODE_LEASH) job_taker = bc::self().champion_name() != Champ::prism;
        if (job_mode == JOB_MODE_OFF) jobs_ready = false;
        if (JOB_CLAIMS) set_jobs_layout(job_mode == JOB_MODE_LEASH ? JOB_MODE_OFF : job_mode);""")
p.rep("""                       job_mode == JOB_MODE_ALL ? "everyone" : job_mode == JOB_MODE_CARRY ? "the carry" : "off",""",
      """                       job_mode == JOB_MODE_ALL ? "everyone" : job_mode == JOB_MODE_CARRY ? "the carry"
                       : job_mode == JOB_MODE_LEASH ? "leashed to Prism" : "off",""")
# candidates: the leash
p.rep("""    const int living = allies_alive();
    static std::vector<Cand> cands;""", """    const int living = allies_alive();
    // v226 (LEASH): around Prism while she is in sight, else around the shared target.
    vec2 anchor = centers[slot % n];
    int shared = -1;
    if (job_mode == JOB_MODE_LEASH) {
        shared = slot % n;
        const auto& allies = seen::champions(Team::ally);
        const auto& tiles = seen::positions(Team::ally);
        for (std::size_t i = 0; i < allies.size() && i < tiles.size(); ++i)
            if (allies[i].champion_name() == Champ::prism) anchor = tiles[i];
    }
    static std::vector<Cand> cands;""")
p.rep("""        if (static_cast<int>(job_camps[u].size()) > JOB_MAX_CAMPS) continue;
        const double worth = job_value(u, carry);""", """        if (static_cast<int>(job_camps[u].size()) > JOB_MAX_CAMPS) continue;
        if (job_mode == JOB_MODE_LEASH && (!job_base[u] || u == shared || distance(centers[u], anchor) > JOB_LEASH)) continue;
        const double worth = job_value(u, carry);""")
p.rep("""            if (job_mode != JOB_MODE_ALL) break;  // the carry alone takes jobs
            if (allies[i] == me || channelling(allies[i])) continue;""", """            if (job_mode != JOB_MODE_ALL && job_mode != JOB_MODE_LEASH) break;  // the carry alone takes jobs
            if (allies[i] == me || channelling(allies[i])) continue;
            if (job_mode == JOB_MODE_LEASH && allies[i].champion_name() == Champ::prism) continue;  // she takes no job""")
p.save()
print('ok')
