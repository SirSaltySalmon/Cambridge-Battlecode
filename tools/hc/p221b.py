"""v221 step b: no shared claims on bare-solo rewards; the bank bar follows the walk to the altar (run once)."""
import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch

B = 'bots/v221/src/'

p = Patch(B + 'constants.h')
p.rep("""inline constexpr int JOB_MAX_WALK = 40, JOB_WAIT = 4, JOB_REPICK = 3, JOB_BAG = 30, JOB_ENEMY_COOL = 8, JOB_HELP = 10;""",
      """inline constexpr int JOB_MAX_WALK = 40, JOB_WAIT = 4, JOB_REPICK = 3, JOB_ENEMY_COOL = 8, JOB_HELP = 10;
// The bank bar with jobs: JOB_BAG_MIN plus JOB_BAG_STEP per tile to the nearest altar, at most JOB_BAG_MAX (a far altar
//   is worth a bigger bag: 04gem-rush, one altar in the middle, went 558 of 2,900 turns walking 10-30 gem bags to it).
//   The old idle rule (IDLE_BANK while the shared target respawns) only holds for a champion with no job.
inline constexpr int JOB_BAG_MIN = 20, JOB_BAG_MAX = 80;
inline constexpr double JOB_BAG_STEP = 1.5;""")
p.save()

p = Patch(B + 'bot.h')
p.after("""    int job_pick(const bc::champion& me);
    bool job_committed(const bc::champion& me);""", """
    // A bare-solo group's reward is captured by whoever stands on it: no shared claim.
    bool job_reward(int r) const { return JOBS && jobs_ready && r >= 0 && r < static_cast<int>(job_base.size()) && job_base[r]; }
    int job_bag_bar(vec2 position) const;""")
p.save()

p = Patch(B + 'spread.cpp')
p.rep("""    if (static_cast<int>(me.gem_count()) >= (JOBS ? JOB_BAG : SPREAD_BAG)) {""",
      """    if (!JOBS && static_cast<int>(me.gem_count()) >= SPREAD_BAG) {""")
p.before("""bool Bot::job_committed(const bc::champion& me) {""", """int Bot::job_bag_bar(vec2 position) const {
    int steps = 1000;
    for (const auto altar : seen::altar_list()) steps = std::min(steps, distance(position, altar));
    if (steps >= 1000) return JOB_BAG_MAX;
    return std::clamp(JOB_BAG_MIN + static_cast<int>(JOB_BAG_STEP * steps), JOB_BAG_MIN, JOB_BAG_MAX);
}

""")
p.save()

p = Patch(B + 'route.cpp')
# the leader's claim
p.rep("""    for (const int r : rewards) {
        if (r == sat_slot) continue;
        if (team_of_prism) {""", """    for (const int r : rewards) {
        if (r == sat_slot || job_reward(r)) continue;  // v221 (JOBS): bare-solo rewards take no claim
        if (team_of_prism) {""")
# vesper / prism / marcella claim
p.rep("""    auto takes = [&](int r) {
        const int rank = collect_rank(me, r);
        if (rank == 0) return false;""", """    auto takes = [&](int r) {
        const int rank = collect_rank(me, r);
        if (rank == 0 || job_reward(r)) return false;""")
# self claim
p.rep("""        if (r != sat_slot && r < static_cast<int>(route.size()) && !(hers && !gemless[r]) && !picker_reward(r)
            && distance(position, centers[r]) <= SELF_CLAIM_RANGE""", """        if (r != sat_slot && r < static_cast<int>(route.size()) && !(hers && !gemless[r]) && !picker_reward(r)
            && !job_reward(r) && distance(position, centers[r]) <= SELF_CLAIM_RANGE""")
p.save()

p = Patch(B + 'turn.cpp')
p.rep("""    } else if (!building && (gems >= bank_bar() || (idle && gems >= IDLE_BANK)
                             || (BANK_KEEP && gems >= IDLE_BANK && bank_walk_round >= static_cast<int>(bc::round_number()) - 1))
               && bank_gems(me)) {""", """    } else if (!building && (gems >= (JOBS && spread_on() ? job_bag_bar(position) : bank_bar())
                             || (idle && gems >= IDLE_BANK && !(JOBS && spread_on()))
                             || (BANK_KEEP && gems >= IDLE_BANK && bank_walk_round >= static_cast<int>(bc::round_number()) - 1))
               && bank_gems(me)) {""")
p.rep("""    if (spread_step(me, now_mode)) return;
    if (now_mode != FARM && now_mode != RETREAT) NOTE""", """    if (spread_step(me, now_mode)) return;
    // v221 (JOBS): no job and the shared target is respawning: a small bag goes in meanwhile (the old idle rule).
    if (JOBS && spread_on() && now_mode == FARM && !holding && !building && idle && gems >= IDLE_BANK && carrier != mine
        && bank_gems(me)) {
        bank_walk_round = static_cast<int>(bc::round_number());
        return;
    }
    if (now_mode != FARM && now_mode != RETREAT) NOTE""")
p.save()
print('ok')
