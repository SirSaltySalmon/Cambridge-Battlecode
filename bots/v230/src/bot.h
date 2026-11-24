// Champion decisions. main.cpp dispatches here after the draft.
#pragma once
#include "actions.h"
#include "board.h"
#include "constants.h"
#include "draft.h"
#include <algorithm>
#include <array>
#include <exception>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

// v90-log: the decision log, built only inside debug_block (LOG in board.h). Statements, not expressions.
//   WHY(name, why)   the branch this turn took and why: one short clause, no stats
//   NOTE(text)       a side decision worth a line of its own
//   SUB(suffix, why) a sub-decision of the branch (a kit's choice): extends its name, appends its reason
#define WHY(name, why) LOG(branch((name), (why));)
#define NOTE(text) LOG(note(text);)
#define SUB(suffix, why) LOG(branch_name += (suffix); branch_why += std::string(branch_why.empty() ? "" : "; ") + (why);)

class Bot {
public:
    void play_turn();

private:
    bool ready = false;
    bool priority_set = false;
    // Route groups, pointing into draft::group_data(), which lives for the whole match.
    std::vector<const bc::map::camp_group*> route;
    // Each route group's spawn tiles as a board, built on first use: making them all in setup
    // pushed the first turn past its fuel on big maps.
    std::vector<std::optional<bc::bitboard>> spawns;
    const bc::bitboard& spawn_board(int slot);
    // Planned route slots for the normal and aggressive plans; every other group is a stop on the way.
    std::vector<int> cores[2];
    std::set<int> monoliths;
    // v120: by slot, whether the group is a monolith (a flag read in the per-slot loops instead of a set lookup).
    std::vector<char> monolith_flags;
    bool is_monolith(int s) const { return s >= 0 && s < static_cast<int>(monolith_flags.size()) && monolith_flags[s]; }
    // v137 (GOLEM_LAST): by slot, whether the group has a golem; any_golem: the map has one at all.
    // v142 (GOLEM_GATE): the flag is the group's golem count.
    std::vector<char> golem_flags;
    bool any_golem = false;
    bool is_golem(int s) const { return s >= 0 && s < static_cast<int>(golem_flags.size()) && golem_flags[s]; }
    int golem_held_round = -1;
    bool golem_held = false;
    // v137: mid_clear memo per slot, keyed on the vision generation (13 callers, two engine reads per camp).
    std::vector<std::uint64_t> mid_gen;
    std::vector<char> mid_val;
    // v137 (SHARE_LOOT_COOL, TARGET_HOLD): the loot we last pointed at and when; the shared target and since when.
    int loot_pointed = -1, loot_pointed_round = -1000;
    int hold_slot = -1, hold_since = -1000;
    // v137 (CAPTURE_NEWS).
    void announce_capture(int s);
    void read_capture_news(int collector_byte, int slot_byte);
    // v137 (BANK_OPEN).
    bool altar_usable(vec2 altar);
    bc::bitboard bank_seen{bc::uninitialized}, bank_camps{bc::uninitialized}, bank_free{bc::uninitialized};
    std::uint64_t bank_seen_gen = 0;
    int bank_seen_round = -1;
    // Most emblems seen on each ally, by champion; emblems are never lost.
    std::map<int, int> emblems;
    // v86: last round update_emblems() read the items.
    int emblem_read_round = -1000;
    // Each ally's emblem key, by roster index, once read.
    std::vector<std::optional<int>> ally_keys;
    std::vector<vec2> centers;
    // v99: views of draft::group_data()'s spawn lists, which live for the whole match (copying them cost an
    // allocation per group at setup). Indexing gives the list itself, as before.
    struct SpawnLists {
        std::vector<const std::vector<vec2>*> lists;
        const std::vector<vec2>& operator[](std::size_t i) const { return *lists[i]; }
        const std::vector<vec2>& back() const { return *lists.back(); }
        void reserve(std::size_t n) { lists.reserve(n); }
        void push_back(const std::vector<vec2>& list) { lists.push_back(&list); }
        std::size_t size() const { return lists.size(); }
    };
    SpawnLists spawn_lists;
    // Spawn tile to slot (-1 for none), for reading visible camps without a pass over every group.
    // A flat grid indexed by tile: a map would allocate a node per tile during setup.
    std::vector<std::int16_t> slot_grid;
    int slot_at(vec2 tile) const {
        if (tile.x < 0 || tile.y < 0 || tile.x >= bc::map::width() || tile.y >= bc::map::height()) return -1;
        return slot_grid[static_cast<std::size_t>(tile.y) * bc::map::width() + tile.x];
    }
    bc::bitboard route_tiles;
    std::size_t route_tile_count = 0;
    // Walking steps between groups, cached per destination group since terrain never changes.
    // By slot; a flat vector so travel() needs no map lookup per group.
    std::vector<std::optional<Walk>> goal_fields;
    int goal_field_count = 0;
    // Most fuel one goal field has cost to build, for build_fields() to budget the next one.
    std::uint64_t field_cost = 0;
    // Walking steps from group start's spawn tiles to group goal's, by start * route size + goal (v120: a flat cache;
    // GAP_UNKNOWN until asked, GAP_NONE for no path), instead of a map node per pair.
    static constexpr int GAP_UNKNOWN = -2, GAP_NONE = -1;
    std::vector<int> gap_cache;
    // Per-slot value while farming (gold weighted above gems), then the original rating.
    std::vector<double> values[2];
    // v144 (CONTEST_VALUE): per slot, the value multiplier for how contested the group is (1 when off or unknown).
    std::vector<double> contest_mult;
    void apply_contest();
    // v105 (STRONG_EARLY): the same values in parts, and the strong-camp weight values[] was last worked out with.
    std::vector<draft::ValueParts> parts[2];
    double priced_extra = 0;
    void reprice();
    // v177 (ECON_PHASES, CLOSE_OUT): the phase values[] was last worked out for, and the phase now.
    bool priced_built = false, priced_close = false, priced_once = false;
    bool econ_built() const;
    bool close_out() const;
    // v177 (CLOSE_OUT): p is nearer the enemy spawn than ours (spawn centres read once).
    mutable std::optional<std::pair<vec2, vec2>> spawn_centres;
    bool enemy_half(vec2 p) const;
    std::vector<double> clears;
    // v97 (FIX_WALK_TARGET): per slot, draft::group_load().
    std::vector<double> loads;
    // v72: route groups that give no gems (gold clusters), by slot.
    std::vector<char> gemless;
    // v123 (PRISM_CLAIM): route groups whose reward is gems with no gold (gem clusters), by slot.
    std::vector<char> pure_gems;
    // v114 (HARD_GATE): per slot, team emblems the group asks for before the free part (elks, golems); the most needed
    // that travel() let through because nothing easier was reachable (-1: none).
    std::vector<int> hard_need;
    int hard_floor = -1;
    // v114 (PASS_FIX): per slot, whether every camp is a cluster.
    std::vector<char> clusters_only;
    // v134 (PROSPECTOR_SWAP): gem and gold cluster camps on the route, and the build with Prospector swapped out.
    int cluster_camps = 0;
    // v134 (CAPTURE_PRICING): rounds a capture takes, set each turn by update_emblems.
    int capture_rounds = 5;
    mutable std::vector<Item> swapped_build;
    mutable bool swap_logged = false;
    // v86 (V86_ZONES): per slot, the zone, whether a pair can clear it, and walking steps from our spawn and from
    // theirs (draft memory); whether the block was there, and whether any slot is safe (if none, uncertain is open).
    std::vector<std::uint8_t> zone_of;
    std::vector<char> pair_ok;
    std::vector<int> home_steps, enemy_steps;
    bool zones_known = false, any_safe = false;
    int zone(int s) const {
        return zones_known && s >= 0 && s < static_cast<int>(zone_of.size()) ? zone_of[s] : ZONE_UNCERTAIN;
    }
    // v86: the shared phase, the pair's target, the phase seen last turn (a split ending starts the cooldown), and
    // the highest zone the target pick may use this call (set from the phase before every pick).
    int phase = PHASE_GROUP;
    int target2 = NO_TARGET2;
    int phase_seen = PHASE_GROUP;
    int split_end_round = -1000;
    // v183 (PRISM_SAT): the round this champion last opened a split as leader.
    int split_open_round = -1000;
    int zone_cap = ZONE_UNSAFE;
    // Slot travel() leaves out besides its skip: the pair's target for the main team, and the reverse for the pair.
    int pair_filter = 0;  // 0 none, 1 main team (drop the pair's slot), 2 pair (safe pair groups away from the main target)
    // The main team's target (for the pair's filter), and the reward being captured that travel() keeps us near (-1).
    int main_slot = -1;
    int capture_near = -1;
    int zone_cap_for(int ph) const;
    bool zone_open(int s) const { return !zones_known || zone(s) <= zone_cap; }
    // v86 kills: per enemy roster index, the round it was first seen dead (-1 alive), and remembered emblems.
    std::vector<int> enemy_dead_since;
    std::vector<int> enemy_emblems, enemy_emblem_round;
    void update_enemies();
    int enemies_alive() const;
    int allies_alive() const;
    // Earliest respawn round of a dead enemy (-1 for none dead), and whether one died within RAID_RECENT rounds.
    int first_respawn() const;
    bool recent_kill() const;
    // Last round this champion saw a near enemy champion, and each ally's last sighting (round, tile) by champion + 1.
    int last_enemy_round = -1000;
    std::array<std::optional<std::pair<int, vec2>>, 16> ally_seen{};
    // Living allies with us: visible within TEAM_RANGE, not channelling, at RECALL_HEALTH or over (with `healthy`, at
    // RAID_HEALTH or over), plus allies last seen beside a bush within ALLY_GRACE rounds; us included.
    int together(const bc::champion& me, bool healthy);
    bool emblem_edge();
    // Our emblems per champion minus theirs (as emblem_edge counts them); none while it cannot be told.
    std::optional<double> emblem_lead();
    bool ally_near(const bc::champion& me);
    int choose_phase(const bc::champion& me, int s, int shared_phase, int& target, int capture, std::string& why);
    bool centre_ok(const bc::champion& me, int shared_phase);
    std::optional<int> raid_target(const bc::champion& me, int shared_phase, int s, int capture, std::string& why);
    std::optional<int> split_candidate(int s, vec2 origin);
    // v104 (INVADE): kills and deaths counted from roster life (global, so every champion counts the same), the
    // baseline taken when the aggressive route first comes on, and the round invading is off until after a revert.
    int kills_seen = 0, deaths_seen = 0;
    std::vector<int> ally_dead_since;
    int invade_base_round = -1, base_kills = 0, base_deaths = 0, base_ours = 0, base_theirs = 0;
    int invade_off_until = -1;
    // Per slot: the most enemy champions seen within INVADE_RISK_RANGE of it, and the round.
    std::vector<int> risk_seen, risk_round;
    void update_invasion();
    double invade_rate(int u, int steps, int now, bool ahead) const;
    // v104 (INVADE): unsafe, or walked to more quickly from their spawn than from ours.
    bool enemy_side(int u) const {
        if (zone(u) == ZONE_UNSAFE) return true;
        return u < static_cast<int>(home_steps.size()) && enemy_steps[u] < home_steps[u];
    }
    std::optional<int> invade_target(const bc::champion& me, int s, int normal, int capture, std::string& why);
    // The split pair (Cassi & Issac): whether we are in it, its target (Issac picks), Cassi's capture, partner absence.
    std::optional<bool> in_pair_cache;
    // v99: whether this team drafted Cassi & Issac, read once.
    std::optional<bool> pair_drafted_cache;
    bool in_pair();
    bool pair_split() { return V86_SPLIT && phase == PHASE_SPLIT && in_pair(); }
    // v150 (SPLIT_SMALL): whether a champion of this kind is in the split pair on this team, and whether it leads it.
    bool split_member(Champ kind) const;
    bool split_lead(Champ kind) const;
    bool split_solo() const { return satellite_on() || (SPLIT_SMALL && roster_size() <= 3); }
    int split_apart() const {
        if (satellite_on()) return SAT_APART;
        return SPLIT_SMALL && roster_size() <= 4 ? SPLIT_APART_SMALL : SPLIT_APART;
    }
    int split_zone() const {
        if (satellite_on()) return SAT_MIDDLE ? ZONE_UNCERTAIN : SAT_ZONE;
        return SPLIT_SMALL && roster_size() <= 4 ? SPLIT_ZONE_SMALL : ZONE_SAFE;
    }
    // v159 (SATELLITE): the satellite's kind on this team (read once from the roster), whether the satellite split
    // runs here, the per-slot solo facts (setup), the leader's readiness check and the satellite's own pick.
    mutable std::optional<std::optional<Champ>> sat_kind_cache;
    std::optional<Champ> sat_kind() const;
    bool satellite_on() const { return SATELLITE && !spread_on() && sat_kind().has_value(); }
    // v183 (PRISM_SAT): every satellite kind (two on a Prism team of PRISM_SAT_PAIR_TEAM), and the satellite's group.
    // On the Prism team target2 is her cash-in altar, so the group stays private there (sat_target); elsewhere it is target2.
    mutable std::vector<Champ> sat_kinds;
    bool sat_member(Champ kind) const;
    bool prism_sat() const { return PRISM_SAT && prism_team() && satellite_on(); }
    int sat_target = NO_TARGET2;
    int sat_group() const { return prism_sat() ? (pair_split_now() ? sat_target : NO_TARGET2) : target2; }
    bool pair_split_now() const { return V86_SPLIT && phase == PHASE_SPLIT && in_pair_cache.value_or(false); }
    void set_sat_group(int s);
    int sat_range() const { return prism_sat() ? PRISM_SAT_RANGE : SAT_RANGE; }
    int sat_bag() const { return prism_sat() ? PRISM_SAT_BAG : SAT_BAG; }
    // v171 (PRISM_AS_V170): the Prism team plays its maps as v170.
    bool as_v170() const { return PRISM_AS_V170 && prism_team(); }
    bool invade_on() const { return INVADE || as_v170(); }
    // v177 (ECON_PHASES): built, a gold-only group on the way is taken only within BUILT_GOLD_ONLY_NEAR.
    int gold_only_near() const {
        if (as_v170()) return V170_GOLD_ONLY_NEAR;
        return ECON_PHASES && priced_built ? BUILT_GOLD_ONLY_NEAR : GOLD_ONLY_NEAR;
    }
    // v202 (SPREAD): roles (by champion + 1: solo unit index, -1 core), territories (by slot: owning unit, -1 none), the
    // solo's job, its enemy cool-off, the units alive this turn, and the danger marks (round, tile).
    // v204 (OUTCLASS): the local emblem gap, the leave's end round and the enemies' last tiles.
    int outclass_gap(const bc::champion& me, bool& near, bool& finish);
    int outclass_until = -1;
    std::vector<vec2> outclass_spots;
    bool outclass_step(const bc::champion& me);
    bool spread_on() const;
    bool spread_ready = false;
    std::array<std::int8_t, 16> spread_unit_of{};
    int spread_unit = -1, spread_units = 0;
    std::vector<std::int8_t> spread_owner;
    int spread_job = -1, spread_pick_round = -1000, spread_cool_until = -1;
    bool spread_farming = false;
    std::array<bool, 8> spread_alive{};
    std::array<std::pair<int, vec2>, 32> spread_marks{};
    int spread_mark_next = 0;
    std::array<bool, 16> spread_ally_was_alive{};
    void spread_setup();
    void spread_update(const bc::champion& me);
    void spread_mark(vec2 tile, int round);
    bool spread_danger(int s, int now) const;
    // The leader's estimate of what each solo unit clears alone (its kind's base stats and our emblem record of it).
    std::vector<char> spread_feasible;
    int spread_feasible_round = -1000;
    std::array<Champ, 8> spread_unit_kind{};
    void spread_estimate();
    bool spread_blocks(int s) const {
        return s >= 0 && s < static_cast<int>(spread_owner.size()) && spread_owner[s] >= 0 && spread_owner[s] != spread_unit
            && spread_alive[spread_owner[s]] && s < static_cast<int>(spread_feasible.size()) && spread_feasible[s];
    }
    bool spread_out() const { return spread_job >= 0; }
    double spread_clear(int s, double damage, double& rounds);
    int spread_pick(const bc::champion& me);
    void spread_drop(const char* why);
    bool spread_step(const bc::champion& me, int now_mode);
    // v230 (ROAD_WALK, RESPAWN_CLEAR): by slot, the last round we saw the group alive / respawning.
    std::vector<int> alive_seen, respawn_seen;
    // v230 (BAND_BANK_STICKY): the hurt band's altar and the round it was chosen.
    std::optional<vec2> band_altar;
    int band_altar_round = -1000;
    std::vector<char> sat_ok, sat_elk;
    std::vector<int> sat_hp, sat_str;
    bool sat_ready() const;
    int sat_pick(const bc::champion& me, int main, int exclude);
    // Stage 3 (SAT_BANK_OWN): own-side and middle altars (built on first use), and those within SAT_BANK_RANGE of us.
    std::optional<std::vector<vec2>> sat_altar_list;
    std::vector<vec2> sat_altars_near;
    const std::vector<vec2>& sat_bank_altars(vec2 position);
    bool split_equipped() const;
    int pair_pick_round = -100;
    int pair_capture = -1, pair_capture_from = 0;
    int partner_gone = 0;
    int pick_pair_target(const bc::champion& me, int main_slot, int exclude);
    void play_pair(const bc::champion& me);
    void end_split(const char* why);
    // v86 per-champion steps.
    bool band_step(const bc::champion& me, int now_mode);
    bool finish_ok(const bc::champion& me, std::string& why);
    bool farm_alone(const bc::champion& me);
    // v90-log: why, when given, gets the reason (a literal; no fuel).
    bool mage_cheese(const bc::champion& me, const Target& camp, const char** why = nullptr);
    // Round each route group is next expected up; 0 means up or never seen.
    std::vector<int> ready_at;
    // Route groups last seen cleared with their reward still uncollected.
    std::set<int> rewards;
    // Scratch list of route slots for observe() and visible_loot(), kept so it never reallocates.
    std::vector<int> slot_scratch;
    int mode = FARM;
    bool win_tribute = false;
    int slot = 0;
    int capture_slot = NO_CAPTURE;
    int collector = 0;
    int claimed = 0;
    bool tributed = false;
    bool tp_recall_checked = false;  // v215 (TP_RECALL): evaluated since the last tribute
    // v94: whether Vesper has finished her core build (shared flag), and whether this team drafted her at all
    // (without Vesper nothing holds the team back). Ren had these from v70 to v93.
    bool vesper_done = false;
    std::optional<bool> has_vesper;
    // v123: whether the draft got Prism (read once; collect_rank is const).
    mutable std::optional<bool> has_prism;
    bool drafted_prism() const;
    // v158: the Prism team (prism_map, and the draft got her), read once. prism_built: her two emblems are done (the shared
    // flag Vesper uses on other teams). prism_cash_on: a safety start has ended her hold for the game.
    mutable std::optional<bool> prism_team_cache;
    bool prism_team() const;
    bool prism_built = false;
    bool prism_cash_on = false;
    int race_old_round = -1, race_old_gems = 0, race_new_round = -1, race_new_gems = 0;
    // Whether a living Prism is free to take gem rewards (no cash-in running).
    bool prism_free() const;
    // v167: a gem reward allies leave to her (PRISM_CLAIM_NEAR: only with her near it), and Willow's walk after her.
    bool prism_wants(int s) const;
    bool willow_follow(const bc::champion& me);
    // v169 (PRISM_STAY_BACK): a true return means she moved or held behind her melee allies this turn.
    bool prism_stay_back(const bc::champion& me);
    // Whether an ally in sight still lacks Prospector.
    bool ally_lacks_prospector() const;
    // Prism's turn: safety starts, the built flag, the guard call. Everyone's: walk to and guard her altar.
    void update_prism_cash(const bc::champion& me);
    bool prism_cash_step(const bc::champion& me, int gems);
    // An ally on a cleared gem reward held for Prism steps off: five rounds there would capture it.
    bool leave_prism_reward(const bc::champion& me);
    // v159: Prism shops in the gaps: not while the team's gem target has live camps in her sight.
    bool prism_mid_clear(const bc::champion& me);
    // v160: PRISM_EARLY_CASH's state and our gem total at the race samples; PRISM_FIGHT_NEAR's checks and walk.
    bool prism_early = false;
    int race_old_ours = 0, race_new_ours = 0;
    bool prism_away(const bc::champion& me) const;
    bool run_to_prism(const bc::champion& me);
    // v161 (PRISM_PICKERS): our private claim, the group gems and their weighted median, when each reward was first seen.
    int picker_slot = -1, picker_since = 0;
    mutable std::vector<int> reward_gems;
    mutable int gem_median = 0;
    std::vector<int> reward_seen;
    mutable int picker_alive_round = -1;
    mutable bool picker_alive = false;
    bool picker_reward(int r) const;
    void update_pickers();
    bool picker_collect(const bc::champion& me);
    int reward_stay_slot = -1, reward_stay_turns = 0;
    bool aggressive = false;
    // Set from spawn until we reach the shared target again; everyone starts on spawn.
    bool rejoining = true;
    // v76: the camp a ranged champion is holding back from, since when, and the last round it held (FIX_STUCK).
    std::optional<vec2> hold_spot;
    int hold_from = 0, hold_last = -10;
    // v143 (WILLOW_REGROUP): the round Willow gave up holding and went to find the team (-1: not regrouping).
    int regroup_since = -1;
    bool willow_regroup(const bc::champion& me);
    // v76: last round we left the enemy chase ring, and spawn.heal waits in a row (FIX_STUCK_RING / _HEAL).
    int left_ring_round = -10, spawn_waits = 0;
    // v116 (CHASE_PATIENCE): per enemy type, the best gap we closed to while it stayed out of reach, the round that gap
    // was last improved, and the round until which a dropped chase stays dropped.
    std::array<int, 16> chase_best{}, chase_since{}, chase_drop{};
    std::array<int, 16> chase_seen{};
    bool chase_dropped(const bc::champion& me, const bc::champion& enemy);
    // v117 (TEAM_CHASE): the team's best gap to the near enemies, when it last improved, the round it was last read, and
    // the round until which fights are dropped.
    int team_gap_best = 0, team_gap_since = 0, team_gap_seen = -10, team_chase_until = -1;
    void update_team_chase(const bc::champion& me, int current_mode);
    bool team_chase_off() const { return TEAM_CHASE && static_cast<int>(bc::round_number()) < team_chase_until; }
    // v119 (CATCH_MODEL): rounds in a row nobody could close on any near enemy, and last round's team gap.
    int catch_miss = 0, catch_gap_last = 0;
    // v138 (CHASE_STEP / CATCH_ACROSS / RETREAT_LEAD): the enemy's step this round, or nullopt when not known (not seen last round, or it stood still).
    std::optional<vec2> enemy_step(const bc::champion& enemy);
    // The tiles it will stand on over its next `n` steps (out[0] = one step on); how many were written.
    std::size_t lead_path(vec2 from, vec2 step, int n, vec2* out);
    // v138 (CHASE_STEP): the tile a fleeing champion target's next step puts it on, or nullopt to chase it where it stands.
    std::optional<vec2> chase_step(const bc::champion& target, int reach);
    // v119 (CATCH_MODEL): the tiles of gap this chaser cannot close on this enemy (0: it can).
    int catch_short(const bc::champion& chaser, const bc::champion& enemy);
    // v119 (JOIN_FIRST): the first round of this contact we waited for an ally, and the last round we did.
    int join_since = -10, join_last = -10;
    bool join_first(const bc::champion& me, const bc::champion& target, const std::vector<bc::champion>& enemies,
                    const bc::bitboard* avoid = nullptr);
    // v119 (PING_GATHER): the last round a near enemy champion was in our sight.
    int near_seen_round = -10;
    // v117 (RECALL_CANCEL): our running recall was started for low health.
    bool hurt_recall = false;
    bool prearmed = false;  // v202 (PREARM)
    // v76: enemies dropped from near_enemies() for standing at their own spawn, and the list without them.
    int home_skipped = 0;
    // v77: the fresh support ping's route slot this turn, if any (FIX_PING).
    std::optional<int> ping_slot;
    // v78: our own ping's round and slot (we do not answer it), whether the leader's score counts a ping bonus
    // this call, and for the outcome log: our health and the dead enemies we saw when we pinged, and the last
    // ping round we logged an answer for.
    int ping_mine_round = -10, ping_mine_slot = -1, ping_mine_health = 0, ping_mine_dead = 0, ping_answered = -10;
    bool ping_bonus = false;
    std::vector<bc::champion> home_buf;
    // v74: alive, off spawn and not recalling at our last turn, so being on spawn now means we walked onto it.
    bool walked_last_turn = false;
    // Whether this champion settled the shared decision this round.
    bool led = false;
    // Champion + 1 of the escorted carrier, 0 for none, and how long we have waited for escorts.
    int carrier = 0;
    int escort_wait = 0;
    int carrier_since = 0;
    // Round the shared mode last changed, and this champion's own last fight decision.
    int mode_changed = 0;
    int local_prev = FARM;
    // Altars a heavy bag may visit alone (far from the centre, on our side) and the escorted rest.
    std::vector<vec2> solo_altars;
    // v98 (FIX_ALTAR_GATE): per altar (bc::map::altars() order) the route slot whose spawn tiles are its only
    // standable neighbours, or -1. v156 (GATE_ALL_GROUPS): one (altar, slot) pair per gate group, so an altar may repeat.
    std::vector<vec2> gate_altars;
    std::vector<int> gate_slots;
    // Round each route slot was last seen with its camps up (state 1), for altar_open().
    std::vector<int> seen_up;
    bool altar_open(vec2 altar) const;
    bool all_altars_shut() const;
    // v156: every altar is gated shut (all_altars_shut, or GATE_TEAM's gates_closed), and whether s is a gate group.
    bool altars_gated() const;
    bool is_gate(int s) const;
    // v156: s is a monolith the team can only score through (gem-starved map, or a gate shutting every altar).
    bool sole_monolith(int s) const;
    // v157 (MONO_MEMORY): per route slot, the health share a monolith had when we last saw it (1 = full or unseen).
    std::vector<float> mono_left;
    float monolith_left(int s) const;
    // v157 (MONO_RESUME, MONO_SCALE): left at MONO_DAMAGED or less, and the emblems its remaining share needs.
    bool monolith_damaged(int s) const;
    int monolith_need(int s) const;
    // v158 (MONO_OWN_ONLY): per route slot, whether the last damage seen on a monolith was ours.
    std::vector<char> mono_ours;
    // v158 (MONO_SESSIONS): the session estimate (damage, and rounds the tank pool lasts), cached for the round.
    double mono_session(int s, double& rounds);
    bool monolith_ready(int s);
    int monolith_count(int s);
    bool mono_contested(int s);
    std::vector<int> mono_side;
    std::vector<int> mono_count;
    int session_round = -1;
    // Last round each monolith's session passed monolith_ready (start or keep-going).
    std::vector<int> session_on;
    double session_damage = 0, session_rounds = 0;
    std::vector<vec2> escort_altars;
    // v150: the map has one altar (both teams share it), and whether we carry an altar lock (LOCK_BUY, LOCK_PLACE).
    bool single_altar = false;
    bool carry_lock = false;
    // v150 (SMALL_BANK): whether the small bank bars apply here (set with single_altar), and the bars themselves.
    bool small_bank = false;
    // v151-f3 (ALTAR_FARM_BANK): most camp gems lie near an altar (split_altars), so bags stay small.
    bool altar_farm = false;
    int bank_bar() const {
        if (altar_farm) return ALTAR_FARM_BANK;
        const int bar = SMALL_BANK > 0 && small_bank ? SMALL_BANK : GEM_BANK;
        // v157 (EARLY_BANK): a lower bar before EARLY_BANK_ROUND.
        if (EARLY_BANK_GEMS > 0 && !as_v170() && static_cast<int>(bc::round_number()) < EARLY_BANK_ROUND) return std::min(bar, EARLY_BANK_GEMS);
        // v177 (CLOSE_OUT): ahead late, small bags go in.
        if (close_out()) return std::min(bar, CLOSE_OUT_BANK);
        return bar;
    }
    int hunted_bar() const { return SMALL_HUNTED > 0 && small_bank ? SMALL_HUNTED : CARRY_GEMS; }
    void buy_lock(const bc::champion& me);
    void buy_crystal(const bc::champion& me);  // v215 (TP)
    bool place_lock(const bc::champion& me);
    // Set for the rest of a turn after a channel is cancelled, so it is not started again.
    bool channel_cut = false;
    // v105 (CAMP_EXIT_REACT): awake camps near us this vision (round and seen::generation()), the reach of every one
    // of them, and the strength of those hitting us (we are the nearest champion in their reach).
    std::uint32_t heat_round = 0;
    std::uint64_t heat_gen = ~std::uint64_t{0};
    int heat_hit = 0;
    bool heat_any = false;
    bc::bitboard heat_reach, exit_blocked, exit_reach, exit_out, exit_away, exit_fast, exit_goal;
    int camp_heat(const bc::champion& me);
    bool camp_exit(const bc::champion& me, const std::vector<bc::champion>& enemies, const std::string& stem);
    // v93 (V93_CAMP_EXIT): set once camps made us leave; we stay hurt (recall when safe) until on spawn or healed to
    // BAND_HEALTH, so the farm step does not walk us straight back into the reach we just left.
    bool exit_hurt = false;
    // v159 (MONO_COVER, MONO_LEAVE, MONO_FIGHT_OUT): see constants.h. mono_rest: we left a monolith hurt and stay off
    // it until healed (MONO_BACK_HEALTH or the spawn); mono_zone: the awake monoliths' reach for fight()'s avoid.
    bool mono_cover(const bc::champion& me);
    int mono_leave_bar(const bc::champion& me);
    bool mono_leave(const bc::champion& me);
    bc::bitboard mono_fight_zone();
    bool mono_rest = false;
    bc::bitboard mono_zone;
    // v129 (CAMP_EXIT_ALWAYS): the group farm() last engaged, and when; whether an awake camp of no group of ours hits us.
    int farmed_slot = -1, farmed_round = -1000;
    // v131 (CAMP_WAIT): camps we stepped out of (spawn tile, reach, round), and the zone farm() keeps out of.
    struct WaitCamp { vec2 spot; int reach; int round; };
    std::vector<WaitCamp> wait_camps;
    bc::bitboard wait_zone, wait_avoid;
    void remember_hitting_camps(const bc::champion& me);
    const bc::bitboard* camp_wait_avoid(const bc::champion& me, const bc::bitboard* avoid);
    // v132 (CAMP_WAIT_FIGHT): the reach of every remembered camp not seen asleep, into fight_wait; false when there is
    // none or we already stand in it.
    bc::bitboard fight_wait;
    bool camp_wait_fight(const bc::champion& me);
    // v132d/e (CAMP_SHARE): whether an awake camp reaching us hits an ally (the nearest champion in its reach).
    bool ally_tanked_camp(const bc::champion& me);
    // v132f/g (WALK_GUARD): the awake camp reach walks keep out of this turn; whether it holds any.
    bc::bitboard guard_zone;
    bool guard_any = false;
    void build_walk_guard(const bc::champion& me);
    // v135 (SAFE_LAND): the barred landings of an area-landing dash this turn (sets the global land_bar).
    void build_land_bar(const bc::champion& me);
    bool area_ok(int s, int farm_s, bool engaged);
    std::vector<char> aoe_ok_memo;  // v135: aoe_scan's area_ok per group
    bool solo_win(int s);  // v135 (SOLO_LAND)
    std::vector<int> group_radius;  // v135: farthest spawn tile from each group centre
    bc::bitboard land_bar_board;
    bool foreign_camp_hit(const bc::champion& me);
    // v122 (BANK_STEPS): walking steps to each altar's submit tiles, by altar_list() index, built on first use.
    std::vector<std::optional<Walk>> altar_walks;
    bool altar_near(vec2 position, vec2 altar, int range);
    const Walk& altar_walk(std::size_t index);
    // v157 (BANK_ON_WAY): banks at an altar that adds at most ON_WAY_BANK_SLACK steps to the walk to the team's target.
    bool bank_on_way(const bc::champion& me, const std::string& stem);
    // v122 (BANK_STICKY): the altar bank.near last walked to, and when.
    std::optional<vec2> bank_altar;
    int bank_round = -1000;
    // v122 (FARM_KEEP): the group whose camps we last saw while farming, and when.
    int keep_slot = -1, keep_round = -1000;
    // v122 (SPAWN_RING): reused ring around a group's spawn tiles.
    bc::bitboard ring_board, ring_out;
    // Decision log for this turn: the shared plan, the branch taken, and why.
    std::string shared_why;
    std::string local_why;
    std::string branch_name;
    std::string branch_why;
    std::vector<std::string> notes;
    std::string score_why;
    // v90-log: what this champion last printed for the shared decision and its own disagreement with it, so a line
    // that repeats round after round is printed once (set only inside LOG).
    std::string lead_logged;
    int own_logged = -1, own_logged_shared = -1;
    // Last place each ally was seen, so a closer melee can be told from one walking off.
    // Indexed by champion + 1; a fixed array, as a map would allocate nodes every turn.
    std::array<std::optional<std::pair<int, vec2>>, 16> ally_at{};
    // v99: champ_id(type) + 1 by champion handle, filled on first read (types never change).
    mutable std::array<std::pair<std::uint32_t, int>, 16> ident_cache{};
    mutable std::size_t ident_count = 0;
    int cached_ident(const bc::champion& champion) const;
    std::optional<int> spawn_gap;
    int ally_in_ring = 0;
    std::vector<vec2> ally_spawn_list;
    std::vector<vec2> enemy_spawn_list;
    std::optional<bc::distance_field> spawn_steps, ally_spawn_steps;
    bc::bitboard spawn_zone;
    // v183 (DRAFT_MAP): the draft's map facts (zeros without its block) and each group's walking steps to the nearest
    // altar, by camp_groups() index (empty without the block).
    draft::MapFacts map_info;
    std::vector<std::uint8_t> altar_steps;
    // near_enemies() result when it had to drop someone, how many it dropped, and the vision
    // list and round it was worked out for.
    std::vector<bc::champion> near_buf;
    int far_skipped = 0;
    const std::vector<bc::champion>* near_src = nullptr;
    std::uint32_t near_round = 0;
    // near_enemies()' search field and boards, reused so no search allocates.
    std::optional<bc::distance_field> near_field;
    bc::bitboard near_roots, near_reached;
    // v123 (FIGHT_LOCAL_WALK): the tiles within FIGHT_LOCAL_RANGE walking steps of a tile, kept for two tiles a round
    // (ours, and the near enemy closest to us); the search reuses near_field. fight_allies: score_mode's assist list.
    struct LocalReach {
        std::uint32_t round = 0;
        vec2 from{};
        bool valid = false;
        bc::bitboard tiles;
    };
    std::array<LocalReach, 2> local_reach_slots;
    std::size_t local_reach_next = 0;
    std::vector<bc::champion> fight_allies;
    // v124: score_mode's weight per assist-list ally and per near enemy, kept with the cached score.
    std::vector<double> fight_ally_w, fight_enemy_w;
    int leavers_left = 0;  // v133: allies skipped by FIGHT_SKIP_LEAVERS on the last score (log only)
    // v71: health at the end of our last turn and its round, to spot damage nobody visible dealt; the round
    // the last such hit is remembered until, and the tiles an attacker hidden in a nearby bush can reach.
    int last_health = -1;
    // v130 (ESCAPE_DASH): the hurt dodge's dash landing ranking (empty when this champion does not escape-dash).
    std::function<std::optional<long>(vec2)> escape_rank(const bc::champion& me, const bc::bitboard& zone);
    bc::distance_field escape_field;
    bc::bitboard escape_walls{bc::uninitialized}, escape_goals{bc::uninitialized}, escape_seen{bc::uninitialized};
    // v133 (GO_KITE): Vesper's retreat from enemies without a ready dash: dash out, then fire instead of walking.
    bool vesper_retreat(const bc::champion& me, const bc::bitboard& goal);
    // v137 (VESPER_ESCAPE): the hurt dodge by the enemies' walking steps and reach; fires when no step gets out.
    bool vesper_escape(const bc::champion& me, const bc::bitboard& zone);
    // v131 (RETREAT_COOL): our last team-retreat turn, and the zone farm() keeps out of after it.
    int last_retreat_round = -1000;
    bool escape_tools(const bc::champion& me);
    bc::bitboard cool_zone;
    // v101: the round Willow last started a recall (WILLOW_CAMP_SAFETY).
    int willow_recall_round = -10;
    int last_health_round = -1;
    int hidden_until = -1;
    int hidden_lost = 0;
    bc::bitboard hidden_spots, hidden_zone;
    // Spawn tile, strength and reach of every route camp that hits (camps never move), for camps out of
    // sight. Built on first use: setup on a big map already needs most of its turn.
    struct CampSpot {
        vec2 at;
        int strength, reach;
    };
    std::optional<std::vector<CampSpot>> camp_spots;

    // turn.cpp
    void play(const bc::champion& me);

    // route.cpp
    void setup();
    int steps_to_enemy_spawn(vec2 position);
    // v141: walking steps from a tile to our spawn (around walls, holes, altars and their spawn).
    int steps_to_ally_spawn(vec2 position);
    // With the draft's per-altar solo flags (bc::map::altars() order) the rule is not worked out again.
    void split_altars(const std::vector<bool>* solo = nullptr);
    void observe();
    int wait_at(int slot, vec2 origin) const;
    void update_emblems();
    int team_emblems() const;
    // v106: team emblems needed before a monolith (MONOLITH_BY_SIZE).
    int monolith_emblems() const;
    bool allowed(int slot);
    // v114 (HARD_GATE): whether the team owns the emblems the group needs (or nothing easier is in reach).
    bool hard_ok(int slot) const;
    int hard_left(int slot) const;
    // v133t: hard_left's free part (it read the roster and the round on every call, in loops over every group).
    int hard_free() const;
    // v142 (GOLEM_GATE): emblems a group's golems ask of n champions present; the n hard_free counts.
    int golem_left(int slot, int n) const;
    int gate_count() const;
    mutable int self_ranged = -1;
    // v142 (NOTHING_ROAM): the target when the table is empty.
    int roam_target(int slot, int skip);
    // v135 (PRESENT_GATE): visible allies (us included) not channelling within `range` of `at`, and their emblems.
    struct Present {
        int n = 0, emblems = 0;
    };
    Present present_near(vec2 at, int range) const;
    const Present& team_present() const;
    mutable Present present_cache;
    mutable int present_round = -1, present_gen = -1;
    // v135 (PRESENT_GATE): the group a champion is staging outside of, and since when.
    int stage_slot = -1, stage_since = 0, stage_last = -10;
    // v135 (TANK_SWAP): see swap.cpp.
    struct AwakeCamp {
        vec2 at;
        int reach, strength;
        bool ours;
    };
    std::size_t awake_near(vec2 at, int margin, int s, std::array<AwakeCamp, 16>& out) const;
    bool swap_hurt(const bc::champion& me);
    // v148-normal (SWAP_STEP): rounds in a row swap_hurt stood covered without a hit.
    int swap_idle = 0, swap_idle_round = -10;
    // v148-normal (RANGED_COVER / SWAP_STEP): a step to a tile a sturdy ally covers; `hurt` allows a tile we cannot hit from.
    bool swap_behind(const bc::champion& me, bool hurt);
    // v148-normal (FINISH_ORDER): what finishing group s costs us in health (camps that hit us, kill order), and the rounds.
    double finish_take(const bc::champion& me, int s, double& rounds, bool* tanked = nullptr);
    // v135 (TEAM_ABANDON): per slot, the round until which the group is off the table (0: not abandoned).
    std::vector<int> abandon_until;
    int bank_walk_round = -10;  // v135 (BANK_KEEP): last round rule 7 walked a bag to an altar
    // turn_round: this turn's round, read once at the start of play() (no engine read per group or per call).
    int turn_round = 0;
    bool abandoned(int s) const {
        return TEAM_ABANDON && s >= 0 && s < static_cast<int>(abandon_until.size()) && abandon_until[s] > turn_round;
    }
    void update_abandon(const bc::champion& me, int s);
    bool swap_tank_step(const bc::champion& me, int s);
    bool stage_group(const bc::champion& me, int s, const std::vector<bc::camp>& camps, const bc::bitboard* avoid,
                     const std::string& stem);
    bool monolith_grouped(int slot);
    // No route group but the monoliths carries gems: the monoliths are the game, not a last resort.
    bool gem_starved = false;
    // Drops monoliths from a travel table unless they are the last resort (or already under attack).
    struct Travel;
    void drop_monoliths(Travel& travel);
    // v151-f4 (SEALED_DROP): groups only reachable across monolith tiles (built once), and whether a monolith blocks.
    std::vector<char> sealed;
    bool sealed_built = false;
    bool monolith_up(int m) const;
    void drop_sealed(Travel& travel);
    // v151-f4 (GATE_TEAM): every altar is gated by a group not known to be down.
    bool gates_closed() const;
    void drop_hard(Travel& travel);
    // v202 (PT_GATE): per slot, the group's threat with every camp up (camps killed weakest first: strength x health
    // spent by the time each dies), its full health and its camp count. See constants.h.
    std::vector<double> pt_threat, pt_full_hp;
    std::vector<int> pt_camps;
    std::vector<char> pt_gate_flags;
    // pt_failing(): the gated groups over their bar (slot, ratio) and a flag per slot, per round and vision generation.
    std::vector<std::pair<int, double>> pt_fail_memo;
    std::vector<char> pt_fail_flags;
    int pt_fail_round = -1;
    std::uint64_t pt_fail_gen = 0;
    const std::vector<std::pair<int, double>>& pt_failing();
    // mid_clear's slots with a camp in sight, per vision generation.
    std::vector<char> sight_slots;
    std::uint64_t sight_slots_gen = 0;
    // The easiest non-golem ratio drop_pt let through because nothing in the table passed (-1: none).
    double pt_floor = -1;
    // One visible ally's camp damage per round and health over camp hits, read once a round; its type once.
    struct AllyStat {
        std::uint32_t handle = 0;
        Champ kind = Champ::ryth;
        bool fresh = false, ok = false;
        double dpr = 0, ehp = 0;
    };
    mutable std::vector<AllyStat> ally_stats;
    mutable int ally_stats_round = -1;
    AllyStat ally_stat(const bc::champion& ally) const;
    struct Power {
        double dpr = 0, ehp = 0;
        int n = 0;
    };
    Power power_near(vec2 at, int range) const;
    const Power& team_power() const;
    mutable Power power_cache;
    mutable int power_round = -1;
    // Off on the Prism team (its own gem-cluster game): v170's rules there.
    bool pt_on() const { return !prism_team(); }
    bool pt_gated(int s) const;
    double pt_budget(int s) const;
    double pt_threat_of(int s) const;
    double pt_ratio(int s, const Power& p) const;
    bool pt_started(int s);
    double pt_bar(int s);
    bool pt_ok(int s);
    bool mono_ready(int m);
    void drop_pt(Travel& travel);
    // v202 (PT_SCAR): per slot, the round a started group was last seen with every camp accounted for (-1: none), and its
    // remaining health and threat then.
    std::vector<int> scar_round;
    std::vector<double> scar_hp, scar_threat;
    bool scar_known(int s) const;
    // v202 (PT_COMMIT): mid_clear's sight board for dead camps (one read per vision generation).
    bc::bitboard sight_board{bc::uninitialized};
    std::uint64_t sight_gen = 0;
    // The shared target a champion hit since the leader's last write (-1: none), and the group we hit last (our own commit).
    int committed_slot = -1;
    int commit_slot = -1, commit_round = -10;
    // v137 (GOLEM_LAST).
    void drop_golems(Travel& travel);
    bool golems_held();
    bool golem_keeps(int s);
    bool can_take_camp(const bc::camp& camp);
    const Walk& group_field(int slot);
    void build_fields();
    std::optional<int> gap(int start, int goal);
    bool mid_clear(int slot);
    // v120: filled into a pooled list (valid until the next call).
    std::vector<int> loot_list;
    std::vector<int>& visible_loot(int skip);
    double route_rate_steps(int slot, int steps) const;
    // v99: the same with the round passed in, for loops (round_number() is an engine read).
    double route_rate_steps(int slot, int steps, int now) const;
    // Walking steps to each route group by slot; -1 where a group is skipped, not allowed or unreachable.
    // A flat vector: a map allocated a node for every group on every call.
    struct Travel {
        std::vector<int> steps;
        int reachable = 0;
        bool has(int s) const { return s >= 0 && s < static_cast<int>(steps.size()) && steps[s] >= 0; }
        int at(int s) const { return steps[s]; }
        bool empty() const { return reachable == 0; }
    };
    // v99: travel()'s raw steps (before the monolith and zone filters) for the round, origin, skip and field count.
    Travel travel_raw;
    // v120: travel()'s filtered table, reused (valid until the next travel() call).
    Travel travel_out;
    std::uint32_t travel_raw_round = 0;
    vec2 travel_raw_origin{-1, -1};
    int travel_raw_skip = -2, travel_raw_fields = -1;
    int best_goal(const Travel& travel) const;
    const Travel& travel(vec2 origin, int skip);
    int pick_target(int slot, vec2 origin, int skip, bool seen_only = false);
    void update_rejoining(const bc::champion& me, int slot);
    // v74: banks from spawn when an off-spawn bank rule would (FIX_SPAWN_BANK).
    bool bank_from_spawn(const bc::champion& me);
    std::optional<std::string> lead_block(const bc::champion& me, int stamp, int now, int target);
    void share_loot(const bc::champion& me);
    void update_capture(const bc::champion& me, int& capture_slot, int& collector, int& claimed);
    void sync_decision(const bc::champion& me);
    bool is_collector(const bc::champion& me) const;
    // v71: gives our claim back so the leader can hand the reward to someone free.
    void release_claim(const char* why);
    void hand_claim(const bc::champion& me, int index);  // v135 (CLAIM_HANDOFF)
    // v135 (DASH_REACH): walkable, or (a dasher) reachable by hopping thin holes.
    bool can_reach(int s, vec2 from, bool dasher);
    static bool dash_kind(Champ kind) {
        return kind == Champ::vesper_vale || kind == Champ::ren || kind == Champ::issac || kind == Champ::vendetta
            || kind == Champ::heart_of_ruin;
    }
    // v71: an enemy champion (seen, or hidden and hitting us) can reach us.
    bool enemy_threat(const bc::champion& me) const;
    void vesper_claim(const bc::champion& me);
    // v71: claims a reward only we can see when no claim is open and nobody visible is nearer.
    void self_claim(const bc::champion& me);
    // v94: Vesper collects first until she owns VESPER_FUNNEL_EMBLEMS (v106; v105 VESPER_CORE_EMBLEMS).
    bool vesper_funnel(const bc::champion& champion) const;
    // v150 (SMALL_FUNNEL_TEAM): the team is small enough for the small-map funnel and claim rules.
    bool small_team() const;
    mutable std::optional<bool> small_team_cache;
    // v150: champions on our team (read once).
    int roster_size() const;
    // v150: the HoR small-team rules' team sizes (HOR_SMALL_FOUR: four only).
    bool hor_small() const { return HOR_SMALL_FOUR ? roster_size() == 4 : roster_size() <= 4; }
    mutable int roster_size_cache = 0;
    // v110 (MARCELLA_CLAIM): Marcella collects second, after a funnelled Vesper.
    bool marcella_collects(const bc::champion& champion) const;
    // v123 (PRISM_CLAIM): who collects slot s first (1), second (2) or as anyone else (0). Gem-only rewards: Prism, then
    // a funnelled Vesper; others: a funnelled Vesper, then Prism or Marcella. kind_rank works from the type alone (a
    // collector out of sight: a Vesper counts as funnelled).
    int collect_rank(const bc::champion& champion, int s) const;
    int kind_rank(Champ kind, int s, bool funnelled) const;
    // v123 (PRISM_HOLD): Prism keeps her bag (no tribute) until PRISM_CASH_ROUND or a tribute that wins.
    bool prism_holds(const bc::champion& me) const;
    bool drafted_vesper();
    bool collect(const bc::champion& me);
    // v134 (CAPTURE_HELP): a Capture emblem owner steps onto an ally's capture until the timer is cut, then off.
    bool capture_help(const bc::champion& me);
    // v202d (CAPTURE_HYGIENE): see constants.h. keep_hold: the reward we held last turn; hit_slot/hit_round: the group our
    // last farm hit went for, and when.
    int keep_hold = -1;
    int hit_slot = -1, hit_round = -1000;
    // Visible allies (but `except`) standing on group s's spawn tiles, from the cached ally tiles.
    int region_allies(int s, const bc::champion* except = nullptr) const;
    // A reward we know of with an ally (but `except`) standing on it: it is being captured.
    bool held_reward(int s, const bc::champion* except = nullptr) const;
    // Free to stand on a reward for its capture (CAPTURE_KEEP).
    bool keep_free(const bc::champion& me);
    // The capture's remaining timer for group s in sight: ally_capturing's countdown, the full time when it is free to
    // start, -1 when contested, taken by the enemy or out of sight.
    int capture_left(int s);
    bool keep_capture(const bc::champion& me);
    // CAPTURE_RECALL: whether step 7a/8 would send us home to shop once s's reward lands; the recall from its tile
    // (left: capture phases to go, -1 when not read yet).
    bool home_after_capture(const bc::champion& me, int s);
    bool capture_recall(const bc::champion& me, int s, int left, const std::string& stem);
    bool hurt_capture_hold(const bc::champion& me);
    int score_mode(std::optional<vec2> origin, int current = FARM);
    // v81: score_mode's origin-free terms and their reason text, for the round and vision lists they were read from.
    struct ScoreBase {
        std::uint32_t round = 0;
        const void* enemies = nullptr;
        const void* allies = nullptr;
        double head = 0, tail = 0;
        std::string head_text, tail_text;
        bool valid = false;
        bool behind_both = false;  // v134 (EMB_STAY_STRICT): less health and less power than the near enemies
    };
    ScoreBase score_base;
    bool squishy_overextended(const std::vector<bc::champion>& allies, const std::vector<bc::champion>& enemies);

    // log.cpp
    void branch(const std::string& name, const std::string& why);
    void note(const std::string& text);
    std::string tile_flags(vec2 tile) const;
    std::string slot_text() const;
    std::string shared_tags() const;
    void begin_trace();
    bool in_spawn_probe(const bc::champion& me) const;
    std::string where_text(const bc::champion& me) const;
    std::vector<std::string> outcome_chunks(const bc::champion& me) const;
    void log_turn(const bc::champion& me);

    // warding.cpp
    // v72: moves our ward beside us every round (FIX_SCOUT_WARD).
    bool scout_ward(const bc::champion& me);

    // tribute.cpp
    bool bank_gems(const bc::champion& me, const std::vector<vec2>& altars = {}, const std::string& stem = "bank");
    bool carrier_alive() const;
    void update_carrier(const bc::champion& me);
    std::optional<vec2> escort_altar() const;
    bool carry(const bc::champion& me);
    bool escort(const bc::champion& me);
    bool guard(const bc::champion& me, vec2 altar);
    // v180 (PRISM_GUARD): bodyguard Prism's cash-in at this altar.
    bool prism_guard(const bc::champion& me, vec2 altar);
    // Her tile when in sight, else her altar; and whether an enemy in sight is within PRISM_GUARD_REACH of it.
    vec2 prism_anchor(vec2 altar) const;
    bool prism_threatened(vec2 altar) const;
    bool recall_to_shop(const bc::champion& me);

    // combat.cpp
    bool fight(const bc::champion& me);
    // Visible enemies within FIGHT_STEPS walking steps of us or a visible ally: the ones that count
    // for the fight score and fight targets. The list stays valid until the next call.
    const std::vector<bc::champion>& near_enemies();
    const std::vector<bc::champion>& near_by_steps();
    // v123 (FIGHT_LOCAL): an ally at `ally` can assist a fight at `from` (FIGHT_LOCAL_RANGE tiles, or walking steps).
    bool assists(vec2 ally, vec2 from);
    const bc::bitboard& local_reach(vec2 from);
    std::pair<bool, std::string> chase_line(const bc::champion& me, const bc::champion& enemy);
    bool retreat(const bc::champion& me);
    // v77: reads, ages and sends the support ping; answers it on a fight turn with no enemy in sight.
    void update_ping(const bc::champion& me);
    bool answer_ping(const bc::champion& me);
    void commit_ping(const bc::champion& me);
    bool ping_busy(const bc::champion& me);
    // v79: whether this champion may answer the current ping (health, bag, distance, not its own).
    bool can_answer(const bc::champion& champion, int now);
    // v81: our ping is fresh and we are outnumbered: walk toward help (PING_RETREAT).
    bool retreat_to_allies(const bc::champion& me);
    std::vector<bc::camp> group_camps();
    // v97 (FIX_WALK_CLAIM): whether slot s's reward tiles can be walked to from `from`; true while its field is unbuilt.
    bool walkable(int s, vec2 from) const;
    // v97 (FIX_WALK_TARGET): the best group we can walk to instead of target s, or -1 when s is fine or none fits.
    int walk_target(const bc::champion& me, int s);
    // v103 (FIX_POCKET): a dasher in a hole pocket recalls out when hopping to target s is slower; whether it did.
    bool pocket_recall(const bc::champion& me, int s, const std::string& stem);
    // v135 (CAPTURE_STAGE): a non-collector waits off the reward being captured (or at its edge); true when handled.
    bool clear_collector_path(const bc::champion& me, int cs, bc::bitboard& goal);
    int collect_stuck = 0, collect_stuck_round = -10;  // v135 (CLAIM_HANDOFF)
    bool stage_capture(const bc::champion& me, int s, bool no_camps, const bc::bitboard* avoid, const std::string& stem);
    // v103 (FIX_POCKET): in a hole pocket while an ally that cannot dash lives: we then neither lead nor point the
    // team at loot, since our walking reach is the pocket's.
    bool pocketed(const bc::champion& me);
    std::vector<bc::camp> camps_at(int slot);
    void farm(const bc::champion& me, const bc::bitboard* avoid = nullptr, const std::string& stem = "farm");
    // v71: whether someone hidden is hitting us (worked out once at turn start), and the answer to it.
    void check_hidden(const bc::champion& me);
    bool hidden_active() const { return hidden_until >= static_cast<int>(bc::round_number()); }
    bool answer_hidden(const bc::champion& me);
    void remember_health(const bc::champion& me);

    // kits.cpp
    void engage(const bc::champion& me, const Target& target, const std::vector<bc::champion>& enemies, bool is_camp,
                const bc::bitboard* avoid = nullptr);
    void engage_melee(const bc::champion& me, const Target& target, bool is_camp, const bc::bitboard* avoid);
    void engage_ranged(const bc::champion& me, const Target& target, const std::vector<bc::champion>& enemies,
                       bool is_camp, const bc::bitboard* avoid);
    bool backing_off_camp(const bc::champion& me, const Target& target, bool is_camp, int gap);
    int camp_standoff(const Target& camp) const;
    bool should_hold_back(const bc::champion& me, const Target& camp);
    // v71: a sturdy ally already inside the camp's reach and closer to it takes its hits.
    bool camp_tanked(const bc::champion& me, const Target& camp);
    std::vector<bc::champion> visible_melees(const bc::champion& me);
    std::vector<bc::champion> closer_melees(const bc::champion& me, const Target& camp);
    std::optional<int> camp_slot(const Target& camp) const;
    bool coming_to_camp(const bc::champion& ally, const Target& camp);
    void remember_allies();
    void hold_camp(const bc::champion& me, const Target& camp, const bc::bitboard* avoid);
    // v101: Willow's fight turn: heal, then hold out of reach in heal range of the front (WILLOW_BACKLINE).
    // v101: a reward Willow should leave to a sturdier ally (WILLOW_SAFE_CLAIM).
    // v101: an awake camp reaches Willow with no sturdy ally closer to it (WILLOW_CAMP_SAFETY).
    bool willow_exposed(const bc::champion& me);
    bool willow_claim_risky(int s);
    void willow_backline(const bc::champion& me, const bc::bitboard* avoid);
    bool willow_follow(const bc::champion& me, const Target& camp, const bc::bitboard& danger, const bc::bitboard* avoid);
    bool marcella_cast(const bc::champion& me, const Target& target, bool is_camp);
    bool ryth_stun(const bc::champion& me, const Target& target, bool is_camp);
    bool stellar_cast(const bc::champion& me, const Target& target, bool is_camp);
    bool cassi_stun(const bc::champion& me, const std::vector<bc::champion>& enemies);
    bool heal_lowest_ally(const bc::champion& me);
    void engage_vesper(const bc::champion& me, const Target& target, const std::vector<bc::champion>& enemies,
                       bool is_camp, const bc::bitboard* avoid);
    bool vesper_kite(const bc::champion& me, const std::vector<bc::champion>& enemies, const Target& target, int reach);
    // v133 (GO_KITE): after firing, dash after a target that walked away from us (fled), onto a tile no other enemy reaches.
    bool vesper_follow(const bc::champion& me, const std::vector<bc::champion>& enemies, const Target& target, int reach);
    bool fled(const bc::champion& me, const bc::champion& enemy) const;
    void track_enemies();
    std::array<vec2, 16> enemy_at{}, enemy_was{};
    std::array<int, 16> enemy_at_round{}, enemy_was_round{};
    // v140 (GHOST): residual vision of the champion we last fought: its tracker slot and the round we engaged it.
    int ghost_k = -1, ghost_round = -100;
    // v216 (FOCUS_KEEP): the focus nibble as last watched, the round it last changed, and the round it was last watched.
    int focus_seen_nibble = -1, focus_change_round = -100, focus_watch_round = -100;
    void watch_focus();
    // Writes the focus and records our own write as a change.
    void set_focus(int index);
    // v216 (PEEL): an enemy melee on our carry that our ready stun reaches (debug logs record chances with PEEL off).
    std::optional<bc::champion> peel_target(const bc::champion& me, const std::vector<bc::champion>& enemies);
    // Debug only: the carry a peel (or a peel chance) covered last turn and its health then, reported a round later.
    int peel_round = -100, peel_carry = -1;
    float peel_carry_hp = 0;
    bool peel_acted = false;
    void peel_report();
    // v216 (FOCUS_VALUE): writes the near enemy with the best focus score when the focus is ours to set, or moves a
    // focus we see to one FOCUS_KEEP_MARGIN better. Updates focus.
    void focus_by_value(const bc::champion& me, const std::vector<bc::champion>& enemies, std::optional<bc::champion>& focus);
    // The unseen tiles it may stand on this turn (ghost_where fills it).
    bc::bitboard ghost_hide, ghost_tmp;
    void mark_ghost(const bc::champion& target);
    // Where its last step leads now (aim) and ghost_hide; false (and the ghost dropped) when there is no ghost to chase.
    bool ghost_where(const bc::champion& me, vec2& aim);
    bool ghost_ward(const bc::champion& me);
    bool chase_ghost(const bc::champion& me);
    // v94 (VARG_SPLASH): the camp in reach whose splash hits the most other camps.
    Target varg_splash_target(const bc::champion& me, const Target& target);
    Target varg_fight_target(const bc::champion& me, const Target& target);
    // v120 (AOE_CLEAN): what an area hit on `target` should cover (its group's camps, or the enemy champions) and the
    // camps it must keep clear of (other groups that hit back). Pooled lists, filled in one pass over the camps.
    std::vector<vec2> aoe_places, aoe_clear;
    void aoe_scan(const Target& target, bool is_camp);
    bool clear_of(vec2 center, int radius) const;
    // v120 (STRAY_EXIT): awake camps of groups other than the farm slot (round, vision, slot cached), their reach, and
    // whether one of them hits us.
    std::uint32_t stray_round = 0;
    std::uint64_t stray_gen = ~std::uint64_t{0};
    int stray_slot = -1;
    bool stray_hit = false;
    bc::bitboard stray_reach, stray_out, stray_goal, stray_step, stray_avoid;
    bool stray_heat(const bc::champion& me, int s);
    bool stray_exit(const bc::champion& me, int s, const std::vector<bc::camp>& camps, const bc::bitboard* avoid,
                    const std::string& stem);
    // v101 Heart of Ruin (hor.cpp).
    struct HorThreat {
        vec2 at;
        int hit_reach = 0, spell_reach = -1;
        double basic = 0, reset = 0, spell = 0;
    };
    struct HorCamp {
        vec2 at;
        int reach = 0;
        double strength = 0;
        bool awake = false;
    };
    std::array<HorThreat, 8> hor_threats{};
    std::array<HorCamp, 16> hor_camps{};
    std::size_t hor_threat_n = 0, hor_camp_n = 0;
    int hor_scan_round = -1;
    void hor_scan(const bc::champion& me);
    double hor_burst(vec2 at, double* camp_part = nullptr) const;
    bool hor_shield(const bc::champion& me);
    bool hor_dive(const bc::champion& me);
    bool hor_camp_dash(const bc::champion& me, const Target& target);
    void engage_hor(const bc::champion& me, const Target& target, const std::vector<bc::champion>& enemies, bool is_camp,
                    const bc::bitboard* avoid);
    bool hor_holds(const bc::champion& me);
    double hor_shield_hp(const bc::champion& champion);
    // v81 mages (MAGE_STANDOFF, MAGE_LEAVE, MAGE_KITE).
    bool mage_spell_ready(const bc::champion& me);
    bool mage_cast(const bc::champion& me, const Target& target, bool is_camp);
    // Reach of the camp's group and of every awake camp in sight.
    bc::bitboard camp_danger(const Target& camp);
    // v94 (MAGE_OUTRANGE): a spell outranges every hitting camp of the group in sight.
    bool outranged(const Target& camp);
    // Whether finishing the camp with basic attacks is cheap, and the hits it would cost.
    bool group_hits(const Target& camp);
    std::pair<bool, int> finish_cost(const bc::champion& me, const Target& camp);
    void mage_camp(const bc::champion& me, const Target& target, const bc::bitboard* avoid);
    // Tiles enemy champions can hit with a basic attack next turn (melee walk-and-hit, ranged shot).
    bc::bitboard enemy_reach(const std::vector<bc::champion>& enemies);
    void mage_fight(const bc::champion& me, const Target& target, const std::vector<bc::champion>& enemies,
                    const bc::bitboard* avoid);

    // shop.cpp
    int vesper_recall_gold(const bc::champion& me) const;
    // v202 (SHOP_CADENCE): gold the shop needs to finish the emblem it is building (cached until the next purchase), the
    // purse that makes a trip worth it (SHOP_GOLD when the trip is cheap), and the income trip of the shop step.
    int emblem_rest(const bc::champion& me) const;
    mutable int rest_cache = -1;
    int shop_bar(const bc::champion& me, bool cheap) const;
    bool income_shop(const bc::champion& me, int now_mode);
    // v202: the round our last shop recall started, and the round collect() saw our own capture land.
    int shop_trip_round = -1000, captured_round = -1000;
    // v202 (SHOP_CAPTURE_RECALL): recall from the reward we capture; the slot, when its reward lands, and our gold plus
    // gems when we left (a capture that landed raised them), until the recall is over.
    bool capture_recall(const bc::champion& me, int s);
    void capture_home_arrival(const bc::champion& me);
    int capture_home = -1, capture_home_landed = 0, capture_home_purse = 0;
    // v202 (EARLY_GOLD): 1 with no team emblems, 0 from EARLY_GOLD_EMBLEMS a champion; gold-only values follow it.
    double gold_fade = 0;
    void update_gold_fade();
    void buy_items(const bc::champion& me);
    // Whether anything was bought.
    bool shop(const bc::champion& me, Champ kind);
    // v134: shop_order(kind), with Prospector swapped out on cluster-poor maps (PROSPECTOR_SWAP).
    std::span<const Item> build_of(Champ kind) const;
};
