"""v221 step h: job claims in the shared bytes (run once from the repo root)."""
import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch

B = 'bots/v221/src/'

# ---------------------------------------------------------------- constants.h
p = Patch(B + 'constants.h')
p.after("""inline constexpr int JOB_NEAR = 3;""", """
// ---- v221 (JOB_CLAIMS): every champion's job is in the shared bytes. ----
// With no claim, 30% of solo-job turns went to a group another champion also held (walk 13.6%, wait 6.4%): out of sight
//   nobody knows who is already on the way, and a stale respawn record sends a second champion to a group just captured.
// On maps where jobs run, the old capture claim (bytes 2, 3 and 6: capture slot, collector, claim round) and the split
//   pair's target (byte 5 bits 0-5) are not used: nobody claims a reward for someone else (whoever stands on it captures),
//   and the pair split and the satellite are off with jobs. Those 30 bits hold five 6-bit claims, one per roster place
//   (0-3 in bytes 2, 3, 6 as one 24-bit word, low bits first; place 4 in byte 5 bits 0-5): 0 none (zeroed memory), 1-62
//   group + 1, 63 "left unfinished". The old readers get "no claim" there and the old writers do nothing (state.cpp).
// Writer: each champion, at the end of its own turn (its job this turn, else none). Leaving a group it did not capture
//   shows 63 for one round first. Readers: a group with as many living claimants as its crew is nobody else's job; an
//   ally whose claim moves off a group (not to 63, not dead) captured it, so its respawn clock starts for everyone
//   (CAPTURE_NEWS's job, which those bytes carried before). Groups 62 and up cannot be claimed.
// Crew jobs: every member claims; a partly seated crew pulls (rate x (1 + JOB_CREW_PULL x seated / crew)). Once cleared,
//   one member in sight takes the reward (the old collect order: a funnelled Vesper first, then the lowest id) and the
//   rest move on.
inline constexpr bool JOB_CLAIMS = true;
inline constexpr int JOB_CLAIM_NONE = 0, JOB_CLAIM_LEFT = 63, JOB_CLAIM_MAX_GROUP = 61, JOB_CLAIM_PLACES = 5;
inline constexpr double JOB_CREW_PULL = 1.0;""")
p.save()

# ---------------------------------------------------------------- state.h / state.cpp
p = Patch(B + 'state.h')
p.after("""int shared_byte(int offset);
void set_shared_byte(int offset, int value);""", """
// v221 (JOB_CLAIMS): on a jobs map bytes 2, 3, 6 and byte 5 bits 0-5 hold the job claims; the accessors above then read
// "no claim" there and do not write them.
void set_jobs_layout(bool on);
bool jobs_layout();
int claim_read(int place);
void claim_write(int place, int value);""")
p.save()

p = Patch(B + 'state.cpp')
p.rep("""int shared_byte(int offset) {
    return static_cast<std::uint8_t>(bc::shared_memory[offset]);
}

void set_shared_byte(int offset, int value) {
    bc::shared_memory[offset] = static_cast<std::uint8_t>(value & 0xFF);
}
""", """namespace {
bool jobs_bytes = false;
bool claim_byte(int offset) {
    return offset == CAPTURE_SLOT_BYTE || offset == CAPTURE_SLOT_BYTE + 1 || offset == CLAIM_ROUND_OFFSET;
}
int raw_byte(int offset) {
    return static_cast<std::uint8_t>(bc::shared_memory[offset]);
}
void set_raw_byte(int offset, int value) {
    bc::shared_memory[offset] = static_cast<std::uint8_t>(value & 0xFF);
}
}  // namespace

void set_jobs_layout(bool on) {
    jobs_bytes = on;
}

bool jobs_layout() {
    return jobs_bytes;
}

int claim_read(int place) {
    if (place < 0 || place >= JOB_CLAIM_PLACES) return JOB_CLAIM_NONE;
    if (place == 4) return raw_byte(PHASE_BYTE) & TARGET2_MASK;
    const int word = raw_byte(CAPTURE_SLOT_BYTE) | raw_byte(CAPTURE_SLOT_BYTE + 1) << 8 | raw_byte(CLAIM_ROUND_OFFSET) << 16;
    return (word >> (6 * place)) & 0x3F;
}

void claim_write(int place, int value) {
    if (place < 0 || place >= JOB_CLAIM_PLACES) return;
    value &= 0x3F;
    if (place == 4) {
        set_raw_byte(PHASE_BYTE, (raw_byte(PHASE_BYTE) & ~TARGET2_MASK & 0xFF) | value);
        return;
    }
    int word = raw_byte(CAPTURE_SLOT_BYTE) | raw_byte(CAPTURE_SLOT_BYTE + 1) << 8 | raw_byte(CLAIM_ROUND_OFFSET) << 16;
    word = (word & ~(0x3F << (6 * place))) | value << (6 * place);
    // Only the bytes this place touches (another champion's write of the others may come between our turns, never inside).
    const int first = 6 * place / 8, last = (6 * place + 5) / 8;
    constexpr int OFFSETS[3] = {CAPTURE_SLOT_BYTE, CAPTURE_SLOT_BYTE + 1, CLAIM_ROUND_OFFSET};
    for (int b = first; b <= last; ++b) set_raw_byte(OFFSETS[b], (word >> (8 * b)) & 0xFF);
}

int shared_byte(int offset) {
    if (jobs_bytes && claim_byte(offset)) return offset == CAPTURE_SLOT_BYTE ? NO_CAPTURE : 0;
    return raw_byte(offset);
}

void set_shared_byte(int offset, int value) {
    if (jobs_bytes && claim_byte(offset)) return;
    set_raw_byte(offset, value);
}
""")
p.rep("""    shared.claimed = b(6);
    shared.vesper_done = flags & VESPER_DONE_FLAG;
    shared.aggressive = flags & AGGRESSIVE_FLAG;
    if (shared.stamp == 0) shared.capture_slot = NO_CAPTURE;""", """    shared.claimed = b(6);
    shared.vesper_done = flags & VESPER_DONE_FLAG;
    shared.aggressive = flags & AGGRESSIVE_FLAG;
    if (shared.stamp == 0) shared.capture_slot = NO_CAPTURE;
    // v221 (JOB_CLAIMS): those bytes are the claims.
    if (jobs_bytes) {
        shared.capture_slot = NO_CAPTURE;
        shared.collector = 0;
        shared.claimed = 0;
        shared.target2 = NO_TARGET2;
    }""")
p.rep("""    const int target2 = (shared_byte(STAMP_BYTE) & STAMP_MASK) == 0 ? NO_TARGET2 : shared_byte(PHASE_BYTE) & TARGET2_MASK;
    const std::array<std::byte, SHARED_BYTES> data{
        std::byte(flags & 0xFF), std::byte(slot & 0xFF), std::byte(capture_slot & 0xFF),
        std::byte(collector & 0xFF), std::byte(stamp & 0xFF), std::byte((phase << PHASE_SHIFT | target2) & 0xFF),
        std::byte(claimed & 0xFF)};
    bc::shared_memory.write(0, std::span<const std::byte>{data});""", """    int target2 = (shared_byte(STAMP_BYTE) & STAMP_MASK) == 0 ? NO_TARGET2 : shared_byte(PHASE_BYTE) & TARGET2_MASK;
    // v221 (JOB_CLAIMS): the claim bits stay as they are.
    if (jobs_bytes) {
        target2 = raw_byte(PHASE_BYTE) & TARGET2_MASK;
        capture_slot = raw_byte(CAPTURE_SLOT_BYTE);
        collector = raw_byte(CAPTURE_SLOT_BYTE + 1);
        claimed = raw_byte(CLAIM_ROUND_OFFSET);
    }
    const std::array<std::byte, SHARED_BYTES> data{
        std::byte(flags & 0xFF), std::byte(slot & 0xFF), std::byte(capture_slot & 0xFF),
        std::byte(collector & 0xFF), std::byte(stamp & 0xFF), std::byte((phase << PHASE_SHIFT | target2) & 0xFF),
        std::byte(claimed & 0xFF)};
    bc::shared_memory.write(0, std::span<const std::byte>{data});""")
p.rep("""void write_target2(int target) {
    set_shared_byte(PHASE_BYTE,""", """void write_target2(int target) {
    if (jobs_bytes) return;  // v221 (JOB_CLAIMS): a claim lives there
    set_shared_byte(PHASE_BYTE,""")
p.save()

# ---------------------------------------------------------------- bot.h
p = Patch(B + 'bot.h')
p.after("""    std::array<char, 4> job_hist_job{};
    int job_hist_n = 0;
    bool job_turn = false;""", """
    // v221 (JOB_CLAIMS): our roster place (-1: none), every place's claim as last read, the living claimants per group
    // (ours left out), what we last wrote, whether the job we last dropped was captured, and by slot the last round we
    // saw the group's state ourselves.
    int claim_place = -1;
    std::array<int, JOB_CLAIM_PLACES> claim_seen{};
    std::vector<std::uint8_t> claim_count;
    int claim_written = JOB_CLAIM_NONE;
    bool claim_captured = false;
    std::vector<int> state_seen;
    void claims_read(const bc::champion& me);
    void claims_write();
    bool claims_on() const { return JOBS && JOB_CLAIMS && jobs_ready && jobs_layout(); }
    bool job_capturer(const bc::champion& me, int s);""")
p.save()

# ---------------------------------------------------------------- route.cpp (observe)
p = Patch(B + 'route.cpp')
p.rep("""        if (ROAD_WALK && state == 1) {
            if (alive_seen.size() != route.size()) alive_seen.assign(route.size(), -1000);
            alive_seen[index] = now;
        }""", """        if (ROAD_WALK && state == 1) {
            if (alive_seen.size() != route.size()) alive_seen.assign(route.size(), -1000);
            alive_seen[index] = now;
        }
        if (JOB_CLAIMS && index < static_cast<int>(state_seen.size())) state_seen[index] = now;""")
p.save()

# ---------------------------------------------------------------- spread.cpp
p = Patch(B + 'spread.cpp')
p.rep("""        jobs_ready = groups > 0 && !gem_starved && !gated;""", """        jobs_ready = groups > 0 && !gem_starved && !gated;
        if (JOB_CLAIMS && jobs_ready) {
            set_jobs_layout(true);
            claim_count.assign(groups, 0);
            state_seen.assign(groups, -1000);
            claim_seen.fill(JOB_CLAIM_NONE);
            const auto& roster = seen::roster(Team::ally);
            for (std::size_t i = 0; i < roster.size() && i < static_cast<std::size_t>(JOB_CLAIM_PLACES); ++i)
                if (roster[i] == bc::self()) claim_place = static_cast<int>(i);
        }""")

p.before("""int Bot::job_bag_bar(vec2 position) const {""", """void Bot::claims_read(const bc::champion&) {
    // v221 (JOB_CLAIMS): once a turn, before any pick. A claim that moved off a group means its owner captured it.
    if (!claims_on()) return;
    const int now = static_cast<int>(bc::round_number());
    const int n = static_cast<int>(route.size());
    std::fill(claim_count.begin(), claim_count.end(), 0);
    const auto& roster = seen::roster(Team::ally);
    for (int i = 0; i < JOB_CLAIM_PLACES && i < static_cast<int>(roster.size()); ++i) {
        const int value = claim_read(i);
        const int before = claim_seen[i];
        claim_seen[i] = value;
        if (i == claim_place) continue;
        bool alive = false;
        try {
            alive = roster[i].is_alive();
        } catch (const bc::error&) {
        }
        if (before >= 1 && before <= JOB_CLAIM_MAX_GROUP + 1 && value != before && value != JOB_CLAIM_LEFT && alive) {
            const int s = before - 1;
            // Not a group we see the state of ourselves this turn, nor one we already know is down.
            if (s < n && state_seen[s] != now && ready_at[s] <= now) {
                ready_at[s] = now + route[s]->max_respawn_timer();
                rewards.erase(s);
                NOTE(format("claim news: s%d was captured, respawns in %d", s, route[s]->max_respawn_timer()));
            }
        }
        if (alive && value >= 1 && value <= JOB_CLAIM_MAX_GROUP + 1 && value - 1 < n) ++claim_count[value - 1];
    }
}

void Bot::claims_write() {
    // v221 (JOB_CLAIMS): the end of our turn. A job turn shows the job; any other turn shows none. A group left without
    // its capture shows 63 for one round first, so nobody starts its respawn clock.
    if (!claims_on() || claim_place < 0) return;
    int wanted = JOB_CLAIM_NONE;
    if (job_turn && spread_job >= 0 && spread_job <= JOB_CLAIM_MAX_GROUP) wanted = spread_job + 1;
    const bool held = claim_written >= 1 && claim_written <= JOB_CLAIM_MAX_GROUP + 1;
    if (held && wanted != claim_written && !claim_captured) wanted = JOB_CLAIM_LEFT;
    claim_captured = false;
    if (wanted == claim_written) return;
    claim_write(claim_place, wanted);
    claim_written = wanted;
}

bool Bot::job_capturer(const bc::champion& me, int s) {
    // v221 (JOB_CLAIMS): a cleared crew job: one of us in sight takes the reward. The old collect order first (1 before 2,
    // 0 last), then whoever is nearer its tiles, then the lower id.
    auto key_of = [&](const bc::champion& c, vec2 at) {
        const int rank = collect_rank(c, s);
        int near = 1000;
        for (const auto t : spawn_lists[s]) near = std::min(near, distance(at, t));
        return std::tuple<int, int, int>{rank == 0 ? 3 : rank, near, cached_ident(c)};
    };
    const auto mine = key_of(me, me.position());
    const auto& allies = seen::champions(Team::ally);
    const auto& tiles = seen::positions(Team::ally);
    for (std::size_t i = 0; i < allies.size() && i < tiles.size(); ++i) {
        if (allies[i] == me || channelling(allies[i])) continue;
        if (distance(tiles[i], centers[s]) > JOB_COMMIT_RANGE + 2) continue;
        if (key_of(allies[i], tiles[i]) < mine) return false;
    }
    return true;
}

""")

# candidates: seats left after the claims
p.rep("""        if (job_skip_until[u] > now) continue;
        double rounds = 0;
        int crew = 1;""", """        if (job_skip_until[u] > now) continue;
        const int seated = claims_on() ? claim_count[u] : 0;
        double rounds = 0;
        int crew = 1;""")
p.rep("""            if (!crew) continue;
        }""", """            if (!crew) continue;
        }
        if (seated >= crew) continue;  // v221 (JOB_CLAIMS): its crew is already on it""")
p.rep("""        cands.push_back({u, values[aggressive][u], rounds, ready, walk, crew});""",
      """        // A partly seated crew pulls: its value per member grows with the seats taken.
        cands.push_back({u, values[aggressive][u] * (1 + JOB_CREW_PULL * seated / crew), rounds, ready, walk, crew});
        cand_seated.push_back(seated);""")
p.rep("""    static std::vector<Cand> cands;
    cands.clear();""", """    static std::vector<Cand> cands;
    static std::vector<int> cand_seated;
    cands.clear();
    cand_seated.clear();""")
# agents: allies in sight with no claim of their own
p.rep("""        for (std::size_t i = 0; i < allies.size() && i < tiles.size() && agent_n < static_cast<int>(agents.size()); ++i) {
            if (allies[i] == me || channelling(allies[i])) continue;
            agents[agent_n++] = {tiles[i], cached_ident(allies[i]), false};
        }""", """        const auto& roster = seen::roster(Team::ally);
        for (std::size_t i = 0; i < allies.size() && i < tiles.size() && agent_n < static_cast<int>(agents.size()); ++i) {
            if (allies[i] == me || channelling(allies[i])) continue;
            // v221 (JOB_CLAIMS): one with a claim has its job (and its seat is counted).
            if (claims_on()) {
                bool claims = false;
                for (int r = 0; r < JOB_CLAIM_PLACES && r < static_cast<int>(roster.size()); ++r)
                    if (roster[r] == allies[i]) claims = claim_seen[r] >= 1 && claim_seen[r] <= JOB_CLAIM_MAX_GROUP + 1;
                if (claims) continue;
            }
            agents[agent_n++] = {tiles[i], cached_ident(allies[i]), false};
        }""")
p.rep("""    for (std::size_t c = 0; c < cands.size(); ++c) gone[c] = cands[c].crew;""",
      """    for (std::size_t c = 0; c < cands.size(); ++c) gone[c] = cands[c].crew - cand_seated[c];""")

# drop: remember a capture
p.rep("""void Bot::spread_drop(const char* why) {
    if (spread_job >= 0) NOTE(format("spread: drops s%d: ", spread_job) + why);
    spread_job = -1;
}""", """void Bot::spread_drop(const char* why) {
    if (spread_job >= 0) NOTE(format("spread: drops s%d: ", spread_job) + why);
    // v221 (JOB_CLAIMS): a capture lets the claim move on at once (the others start its respawn clock).
    if (spread_job >= 0 && std::string_view(why) == "captured") claim_captured = true;
    spread_job = -1;
}""")
# step: a job whose seats others filled meanwhile; the one capturer of a cleared crew job
p.rep("""    const bool crew_busy = JOBS && spread_job >= 0 && job_crew > 1 && mid_clear(spread_job);""",
      """    if (claims_on() && spread_job >= 0 && claim_count[spread_job] >= job_crew && !mid_clear(spread_job)
        && !spawn_board(spread_job).is_set(me.position()))
        spread_drop("claimed by others meanwhile");
    if (claims_on() && spread_job >= 0 && job_crew > 1 && rewards.count(spread_job) && !job_capturer(me, spread_job))
        spread_drop("captured");  // an ally in sight takes the reward: the group is as good as captured for us
    const bool crew_busy = JOBS && spread_job >= 0 && job_crew > 1 && mid_clear(spread_job);""")
p.rep("""void Bot::spread_update(const bc::champion& me) {
    // Every turn before the shared decision: units alive (for the leader's filter) and danger marks (enemy sightings,
    // ally deaths at their last seen tile).
    if (!spread_on()) return;""", """void Bot::spread_update(const bc::champion& me) {
    // Every turn before the shared decision: units alive (for the leader's filter) and danger marks (enemy sightings,
    // ally deaths at their last seen tile).
    if (!spread_on()) return;
    claims_read(me);""")
p.save()

# ---------------------------------------------------------------- turn.cpp
p = Patch(B + 'turn.cpp')
p.rep("""        job_hist_at[0] = job_start;
        job_hist_job[0] = job_turn ? 1 : 0;
        if (job_hist_n < 4) ++job_hist_n;
    }""", """        job_hist_at[0] = job_start;
        job_hist_job[0] = job_turn ? 1 : 0;
        if (job_hist_n < 4) ++job_hist_n;
    }
    try {
        claims_write();
    } catch (const std::exception& error) {
        say(std::string("claim write error: ") + error_text(error));
    }""")
p.save()
print('ok')
