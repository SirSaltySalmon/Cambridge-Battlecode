"""v221 step i: two modes. Cluster maps: everyone on jobs (as before). Other maps: only the carry (run once)."""
import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch

B = 'bots/v221/src/'

# ---------------------------------------------------------------- constants.h
p = Patch(B + 'constants.h')
p.after("""inline constexpr double JOB_CREW_PULL = 1.0;""", """
// ---- v221 (JOB_MODES): who takes jobs. ----
// Everyone on jobs was level with v220 on ordinary maps (1,076-1,114 rounds unopposed vs 1,129) and far ahead on cluster
//   maps (04gem-rush 2,000+ -> 666, gemcombat5 2,000+ -> ~450, 00vault, 04gem-caverns: -1,000 rounds or more). Why level
//   elsewhere: own-half gems are mostly behind wolves and elks (ceiling per map and round: elk groups 0.72, wolf groups
//   0.62, pure gem clusters 0.34, monoliths 0.27), every champion capturing its own reward ends the gold funnel, and a
//   bare champion clears nothing but clusters alone, so crews of three walked together as the old team did.
// Mode ALL (cluster maps: gem clusters are over PRISM_TEAM_BAR of the camps): everyone, with the five claims.
// Mode CARRY (every other jobs map): only Vesper. She takes whatever she clears alone, clusters first (gold ones too:
//   20 gold is two thirds of a shard, x1.33 for her), while the other four farm the shared target as before and the
//   funnel stays (she still claims their rewards when near). Her one claim sits in byte 5 bits 0-5 (free: the pair split
//   and the satellite are off with jobs), so the bytes of the old capture claim stay the old claim. The shared target
//   and the old claims leave her claimed group alone.
// Top-1's Vesper does this (00corners r30-100: alone on clusters while five fight in the middle; Prospector by r48).
inline constexpr int JOB_MODE_OFF = 0, JOB_MODE_CARRY = 1, JOB_MODE_ALL = 2;
inline constexpr int JOB_CARRY_PLACE = 4;
// A job's worth: its gems plus JOB_GOLD_CARRY (the carry) or JOB_GOLD_OTHER gems per gold coin.
inline constexpr double JOB_GOLD_CARRY = 0.15, JOB_GOLD_OTHER = 0.05;""")
p.save()

# ---------------------------------------------------------------- draft
p = Patch(B + 'draft.h')
p.rep("""bool prism_map();""", """bool prism_map();
// v221: the cluster-map rule itself (gem clusters over PRISM_TEAM_BAR of the camps), whatever PRISM_TEAM says.
bool cluster_map();""")
p.save()
p = Patch(B + 'draft.cpp')
p.rep("""bool prism_map() {
    if (!PRISM_TEAM) return false;
    static const bool rule = [] {""", """bool prism_map() {
    return PRISM_TEAM && cluster_map();
}

bool cluster_map() {
    static const bool rule = [] {""")
p.save()

# ---------------------------------------------------------------- state
p = Patch(B + 'state.h')
p.rep("""void set_jobs_layout(bool on);
bool jobs_layout();""", """void set_jobs_layout(int mode);
int jobs_layout();""")
p.save()
p = Patch(B + 'state.cpp')
p.rep("""bool jobs_bytes = false;""", """int jobs_mode = JOB_MODE_OFF;
#define jobs_bytes (jobs_mode == JOB_MODE_ALL)""")
p.rep("""void set_jobs_layout(bool on) {
    jobs_bytes = on;
}

bool jobs_layout() {
    return jobs_bytes;
}""", """void set_jobs_layout(int mode) {
    jobs_mode = mode;
}

int jobs_layout() {
    return jobs_mode;
}""")
p.rep("""    if (place < 0 || place >= JOB_CLAIM_PLACES) return JOB_CLAIM_NONE;
    if (place == 4) return raw_byte(PHASE_BYTE) & TARGET2_MASK;""", """    if (place < 0 || place >= JOB_CLAIM_PLACES || jobs_mode == JOB_MODE_OFF) return JOB_CLAIM_NONE;
    if (place == 4) return raw_byte(PHASE_BYTE) & TARGET2_MASK;
    if (jobs_mode != JOB_MODE_ALL) return JOB_CLAIM_NONE;""")
p.rep("""    if (place < 0 || place >= JOB_CLAIM_PLACES) return;
    value &= 0x3F;
    if (place == 4) {
        set_raw_byte(PHASE_BYTE, (raw_byte(PHASE_BYTE) & ~TARGET2_MASK & 0xFF) | value);
        return;
    }""", """    if (place < 0 || place >= JOB_CLAIM_PLACES || jobs_mode == JOB_MODE_OFF) return;
    value &= 0x3F;
    if (place == 4) {
        set_raw_byte(PHASE_BYTE, (raw_byte(PHASE_BYTE) & ~TARGET2_MASK & 0xFF) | value);
        return;
    }
    if (jobs_mode != JOB_MODE_ALL) return;""")
# read_shared / write_shared: target2 is a claim in both modes; the capture bytes only in mode ALL
p.rep("""    if (jobs_bytes) {
        shared.capture_slot = NO_CAPTURE;
        shared.collector = 0;
        shared.claimed = 0;
        shared.target2 = NO_TARGET2;
    }""", """    if (jobs_bytes) {
        shared.capture_slot = NO_CAPTURE;
        shared.collector = 0;
        shared.claimed = 0;
    }
    if (jobs_mode != JOB_MODE_OFF) shared.target2 = NO_TARGET2;""")
p.rep("""    if (jobs_bytes) {
        target2 = raw_byte(PHASE_BYTE) & TARGET2_MASK;
        capture_slot = raw_byte(CAPTURE_SLOT_BYTE);""", """    if (jobs_mode != JOB_MODE_OFF) target2 = raw_byte(PHASE_BYTE) & TARGET2_MASK;
    if (jobs_bytes) {
        capture_slot = raw_byte(CAPTURE_SLOT_BYTE);""")
p.rep("""    if (jobs_bytes) return;  // v221 (JOB_CLAIMS): a claim lives there""",
      """    if (jobs_mode != JOB_MODE_OFF) return;  // v221 (JOB_CLAIMS): a claim lives there""")
p.save()

# ---------------------------------------------------------------- bot.h
p = Patch(B + 'bot.h')
p.rep("""    bool job_reward(int r) const { return JOBS && jobs_ready && r >= 0 && r < static_cast<int>(job_base.size()) && job_base[r]; }""",
      """    // (JOB_MODES) In mode ALL; in either mode a group an ally claims is its claimant's.
    bool job_reward(int r) const {
        if (!JOBS || !jobs_ready || r < 0 || r >= static_cast<int>(job_base.size())) return false;
        if (job_mode == JOB_MODE_ALL && job_base[r]) return true;
        return r < static_cast<int>(claim_count.size()) && claim_count[r] > 0;
    }
    // v221 (JOB_MODES): this map's mode, whether we take jobs, by claim place the roster index that owns it (-1 none),
    // and a job's worth to us.
    int job_mode = JOB_MODE_OFF;
    bool job_taker = false;
    std::array<int, JOB_CLAIM_PLACES> claim_owner{};
    double job_value(int s, bool carry) const;""")
p.save()

# ---------------------------------------------------------------- spread.cpp
p = Patch(B + 'spread.cpp')
p.rep("""        jobs_ready = groups > 0 && !gem_starved && !gated;
        if (JOB_CLAIMS && jobs_ready) {
            set_jobs_layout(true);
            claim_count.assign(groups, 0);
            state_seen.assign(groups, -1000);
            claim_seen.fill(JOB_CLAIM_NONE);
            const auto& roster = seen::roster(Team::ally);
            for (std::size_t i = 0; i < roster.size() && i < static_cast<std::size_t>(JOB_CLAIM_PLACES); ++i)
                if (roster[i] == bc::self()) claim_place = static_cast<int>(i);
        }
        LOG(say(format("setup: jobs %s, %d of %d groups are bare-solo", jobs_ready ? "on" : "off (monolith map)", count, groups)););
        return;""", """        jobs_ready = groups > 0 && !gem_starved && !gated;
        claim_count.assign(groups, 0);
        state_seen.assign(groups, -1000);
        claim_seen.fill(JOB_CLAIM_NONE);
        claim_owner.fill(-1);
        job_mode = !jobs_ready ? JOB_MODE_OFF : draft::cluster_map() ? JOB_MODE_ALL : JOB_MODE_CARRY;
        const auto& roster = seen::roster(Team::ally);
        const bool vesper = bc::self().champion_name() == Champ::vesper_vale;
        if (job_mode == JOB_MODE_ALL) {
            job_taker = true;
            for (std::size_t i = 0; i < roster.size() && i < static_cast<std::size_t>(JOB_CLAIM_PLACES); ++i) {
                claim_owner[i] = static_cast<int>(i);
                if (roster[i] == bc::self()) claim_place = static_cast<int>(i);
            }
        } else if (job_mode == JOB_MODE_CARRY) {
            job_taker = vesper;
            for (std::size_t i = 0; i < roster.size(); ++i)
                if (roster[i].champion_name() == Champ::vesper_vale) claim_owner[JOB_CARRY_PLACE] = static_cast<int>(i);
            if (vesper) claim_place = JOB_CARRY_PLACE;
            if (claim_owner[JOB_CARRY_PLACE] < 0) job_mode = JOB_MODE_OFF;  // no carry drafted: the old team
        }
        if (job_mode == JOB_MODE_OFF) jobs_ready = false;
        if (JOB_CLAIMS) set_jobs_layout(job_mode);
        LOG(say(format("setup: jobs mode %d (%s), we %s, %d of %d groups are bare-solo", job_mode,
                       job_mode == JOB_MODE_ALL ? "everyone" : job_mode == JOB_MODE_CARRY ? "the carry" : "off",
                       job_taker ? "take jobs" : "farm the shared target", count, groups)););
        return;""")
p.rep("""bool Bot::claims_on() const {
    return JOBS && JOB_CLAIMS && jobs_ready && jobs_layout();
}""", """bool Bot::claims_on() const {
    return JOBS && JOB_CLAIMS && jobs_ready && jobs_layout() != JOB_MODE_OFF;
}

double Bot::job_value(int s, bool carry) const {
    return route[s]->gem_value() + (carry ? JOB_GOLD_CARRY : JOB_GOLD_OTHER) * route[s]->gold_value();
}""")
# claims_read: by owner
p.rep("""    const auto& roster = seen::roster(Team::ally);
    for (int i = 0; i < JOB_CLAIM_PLACES && i < static_cast<int>(roster.size()); ++i) {
        const int value = claim_read(i);
        const int before = claim_seen[i];
        claim_seen[i] = value;
        if (i == claim_place) continue;
        bool alive = false;
        try {
            alive = roster[i].is_alive();
        } catch (const bc::error&) {
        }""", """    const auto& roster = seen::roster(Team::ally);
    for (int i = 0; i < JOB_CLAIM_PLACES; ++i) {
        const int owner = claim_owner[i];
        if (owner < 0 || owner >= static_cast<int>(roster.size())) continue;
        const int value = claim_read(i);
        const int before = claim_seen[i];
        claim_seen[i] = value;
        if (i == claim_place) continue;
        bool alive = false;
        try {
            alive = roster[owner].is_alive();
        } catch (const bc::error&) {
        }""")
# job_pick: value, the carry's solo-only jobs, held rewards, the claims of allies in sight by owner
p.rep("""    const double taken = camp_taken_mult(passives);
    struct Cand {""", """    const double taken = camp_taken_mult(passives);
    const bool carry = me.champion_name() == Champ::vesper_vale;
    struct Cand {""")
p.rep("""        if (is_monolith(u) || u == capture_slot || values[aggressive][u] <= 0) continue;
        if (static_cast<int>(job_camps[u].size()) > JOB_MAX_CAMPS) continue;""", """        if (is_monolith(u) || u == capture_slot) continue;
        if (static_cast<int>(job_camps[u].size()) > JOB_MAX_CAMPS) continue;
        const double worth = job_value(u, carry);
        if (worth <= 0) continue;""")
p.rep("""            crew = 0;
            for (int k = 2; k <= living && !crew; ++k)""", """            crew = 0;
            for (int k = 2; k <= living && !crew && job_mode == JOB_MODE_ALL; ++k)""")
p.rep("""        if (reward && u != spread_job && held_reward(u, &me)) continue;""",
      """        if (reward && u != spread_job && region_allies(u, &me) > 0) continue;""")
p.rep("""        cands.push_back({u, values[aggressive][u] * (1 + JOB_CREW_PULL * seated / crew), rounds, ready, walk, crew});""",
      """        cands.push_back({u, worth * (1 + JOB_CREW_PULL * seated / crew), rounds, ready, walk, crew});""")
p.rep("""                bool claims = false;
                for (int r = 0; r < JOB_CLAIM_PLACES && r < static_cast<int>(roster.size()); ++r)
                    if (roster[r] == allies[i]) claims = claim_seen[r] >= 1 && claim_seen[r] <= JOB_CLAIM_MAX_GROUP + 1;
                if (claims) continue;""", """                bool claims = false;
                for (int r = 0; r < JOB_CLAIM_PLACES; ++r)
                    if (claim_owner[r] >= 0 && claim_owner[r] < static_cast<int>(roster.size()) && roster[claim_owner[r]] == allies[i])
                        claims = claim_seen[r] >= 1 && claim_seen[r] <= JOB_CLAIM_MAX_GROUP + 1;
                if (claims) continue;""")
# in mode CARRY the others are not agents of the deal (they farm the shared target)
p.rep("""        for (std::size_t i = 0; i < allies.size() && i < tiles.size() && agent_n < static_cast<int>(agents.size()); ++i) {
            if (allies[i] == me || channelling(allies[i])) continue;
            // v221 (JOB_CLAIMS): one with a claim has its job (and its seat is counted).""", """        for (std::size_t i = 0; i < allies.size() && i < tiles.size() && agent_n < static_cast<int>(agents.size()); ++i) {
            if (job_mode != JOB_MODE_ALL) break;  // the carry alone takes jobs
            if (allies[i] == me || channelling(allies[i])) continue;
            // v221 (JOB_CLAIMS): one with a claim has its job (and its seat is counted).""")
# step: only takers
p.rep("""    if (!spread_on() || (!JOBS && spread_unit < 0) || route.empty()) return false;
    const int now = static_cast<int>(bc::round_number());""", """    if (!spread_on() || (!JOBS && spread_unit < 0) || route.empty()) return false;
    if (JOBS && !job_taker) return false;
    const int now = static_cast<int>(bc::round_number());""")
p.save()

# ---------------------------------------------------------------- route.cpp: the shared table
p = Patch(B + 'route.cpp')
p.rep("""    if (JOBS && spread_on() && pair_filter == 0) {
        bool other = false;
        for (int s = 0; s < n && !other; ++s)
            if (result.has(s) && !job_base[s]) other = true;
        if (other)
            for (int s = 0; s < n; ++s)
                if (result.has(s) && job_base[s]) {
                    result.steps[s] = -1;
                    --result.reachable;
                }
    }""", """    if (JOBS && spread_on() && pair_filter == 0 && job_mode == JOB_MODE_ALL) {
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
    // v221 (JOB_MODES): a group an ally claims is its claimant's (the carry's, in mode CARRY), unless the team already
    // started it.
    if (JOBS && spread_on() && pair_filter == 0 && claims_on())
        for (int s = 0; s < n && s < static_cast<int>(claim_count.size()); ++s)
            if (result.has(s) && claim_count[s] > 0 && !mid_clear(s)) {
                result.steps[s] = -1;
                --result.reachable;
            }""")
p.save()

# ---------------------------------------------------------------- turn.cpp: spawn.leave picks only for takers
p = Patch(B + 'turn.cpp')
p.rep("""            if (JOBS && spread_on() && mode == FARM && seen::champions(Team::enemy).empty()) {""",
      """            if (JOBS && spread_on() && job_taker && mode == FARM && seen::champions(Team::enemy).empty()) {""")
# bank bars: the job bar only for takers
p.rep("""    } else if (!building && (gems >= (JOBS && spread_on() ? job_bag_bar(position) : bank_bar())
                             || (idle && gems >= IDLE_BANK && !(JOBS && spread_on()))""", """    } else if (!building && (gems >= (JOBS && spread_on() && job_taker ? job_bag_bar(position) : bank_bar())
                             || (idle && gems >= IDLE_BANK && !(JOBS && spread_on() && job_taker))""")
p.rep("""    if (JOBS && spread_on() && now_mode == FARM && !holding && !building && idle && gems >= IDLE_BANK && carrier != mine""",
      """    if (JOBS && spread_on() && job_taker && now_mode == FARM && !holding && !building && idle && gems >= IDLE_BANK && carrier != mine""")
p.rep("""    if (JOBS && spread_on()) return gems >= job_bag_bar(position) && bank_gems(me, {}, "spawn.bank");""",
      """    if (JOBS && spread_on() && job_taker) return gems >= job_bag_bar(position) && bank_gems(me, {}, "spawn.bank");""")
p.save()
print('ok')
