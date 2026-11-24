"""v221 step f: no jobs on very large groups; a breaker for job / non-job walks that undo each other (run once)."""
import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch

B = 'bots/v221/src/'

p = Patch(B + 'constants.h')
p.after("""inline constexpr int BAND_BANK_STICKY = 12;""", """
// JOB_MAX_CAMPS: a group of more camps is never a job (the shared target takes it as before): the 29-cluster road of
//   01yellow-brick-road and the long rows of 04lines / 00lanes cost whole turns of fuel as jobs (2,788 / 1,163 / 1,048
//   out-of-fuel turns against 141 / 624 / 123).
inline constexpr int JOB_MAX_CAMPS = 12;
// JOB_BREAKER: our tile repeats A B A B over four turns while job turns and other turns alternate (a job's walk and an
//   older rule's walk undoing each other: 04pinwheel's hurt-band bank, 548 turns): no job for JOB_BREAK_ROUNDS rounds, so
//   the older rule finishes what it is doing.
inline constexpr bool JOB_BREAKER = true;
inline constexpr int JOB_BREAK_ROUNDS = 15;""")
p.save()

p = Patch(B + 'bot.h')
p.after("""    std::optional<vec2> band_altar;
    int band_altar_round = -1000;""", """
    // v221 (JOB_BREAKER): our tile at the start of the last four turns (newest first) and whether each was a job turn.
    std::array<vec2, 4> job_hist_at{};
    std::array<char, 4> job_hist_job{};
    int job_hist_n = 0;
    bool job_turn = false;""")
p.save()

p = Patch(B + 'spread.cpp')
p.rep("""            std::sort(list.begin(), list.end());
            if (is_monolith(s)) continue;""", """            std::sort(list.begin(), list.end());
            if (is_monolith(s) || static_cast<int>(list.size()) > JOB_MAX_CAMPS) continue;""")
p.rep("""        if (is_monolith(u) || u == capture_slot || values[aggressive][u] <= 0) continue;
        const int air = distance(position, centers[u]);""", """        if (is_monolith(u) || u == capture_slot || values[aggressive][u] <= 0) continue;
        if (static_cast<int>(job_camps[u].size()) > JOB_MAX_CAMPS) continue;
        const int air = distance(position, centers[u]);""")
p.rep("""    if (JOBS && spread_job < 0 && job_committed(me)) return false;""", """    if (JOBS && spread_job < 0 && job_committed(me)) return false;
    if (JOBS && JOB_BREAKER && job_hist_n >= 4 && job_hist_at[0] == job_hist_at[2] && job_hist_at[1] == job_hist_at[3]
        && job_hist_at[0] != job_hist_at[1] && job_hist_job[0] != job_hist_job[1] && job_hist_job[0] == job_hist_job[2]
        && job_hist_job[1] == job_hist_job[3]) {
        spread_drop("our walk and another rule's undo each other: no job for a while");
        spread_cool_until = now + JOB_BREAK_ROUNDS;
        job_hist_n = 0;
        return false;
    }""")
p.rep("""    spread_farming = !(JOBS && job_crew > 1);
    farm(me, nullptr, JOBS ? (job_crew > 1 ? "crew" : "job") : "spread");""", """    spread_farming = !(JOBS && job_crew > 1);
    job_turn = true;
    farm(me, nullptr, JOBS ? (job_crew > 1 ? "crew" : "job") : "spread");""")
p.save()

p = Patch(B + 'turn.cpp')
p.rep("""    begin_trace();
    std::exception_ptr failure;
    const auto start_round = bc::round_number();
    try {
        play(me);""", """    begin_trace();
    std::exception_ptr failure;
    const auto start_round = bc::round_number();
    // v221 (JOB_BREAKER): this turn's start tile and kind go into the history when the turn is over.
    job_turn = false;
    vec2 job_start{-1, -1};
    try {
        if (JOBS && JOB_BREAKER && me.is_alive()) job_start = me.position();
    } catch (const std::exception&) {
    }
    try {
        play(me);""")
p.rep("""    // Like Python's finally: these run whether or not the turn failed, then the error goes on to main.
    try {
        remember_allies();""", """    if (JOBS && JOB_BREAKER && job_start.x >= 0) {
        for (int i = 3; i > 0; --i) {
            job_hist_at[i] = job_hist_at[i - 1];
            job_hist_job[i] = job_hist_job[i - 1];
        }
        job_hist_at[0] = job_start;
        job_hist_job[0] = job_turn ? 1 : 0;
        if (job_hist_n < 4) ++job_hist_n;
    }
    // Like Python's finally: these run whether or not the turn failed, then the error goes on to main.
    try {
        remember_allies();""")
p.save()
print('ok')
