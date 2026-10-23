// Champion decisions. main.cpp dispatches here after the draft.
#pragma once
#include "actions.h"
#include "board.h"
#include "constants.h"
#include "draft.h"
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
    std::map<std::pair<int, int>, std::optional<int>> gaps;
    // Per-slot value while farming (gold weighted above gems), then the original rating.
    std::vector<double> values[2];
    // v105 (STRONG_EARLY): the same values in parts, and the strong-camp weight values[] was last worked out with.
    std::vector<draft::ValueParts> parts[2];
    double priced_extra = 0;
    void reprice();
    std::vector<double> clears;
    // v97 (FIX_WALK_TARGET): per slot, draft::group_load().
    std::vector<double> loads;
    // v72: route groups that give no gems (gold clusters), by slot.
    std::vector<char> gemless;
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
    // v94: whether Vesper has finished her core build (shared flag), and whether this team drafted her at all
    // (without Vesper nothing holds the team back). Ren had these from v70 to v93.
    bool vesper_done = false;
    std::optional<bool> has_vesper;
    bool aggressive = false;
    std::vector<vec2> ward_spots;
    // Index of the ward spot our own ward covers, or none while it is free to move.
    std::optional<int> ward_slot;
    // Fixed anti-ward zone around each ward spot, and their union, built on first use.
    std::optional<std::vector<bc::bitboard>> ward_zones;
    bc::bitboard ward_union;
    // Key bushes by priority (near an altar, the centre, a route camp), built once at a turn end, and
    // their union grown by the ward placement range: one read tells whether one is in reach.
    std::optional<std::array<bc::bitboard, 3>> key_bushes;
    bc::bitboard key_bush_reach;
    // Set once our one ward has gone into a bush; it then stays there.
    bool bush_warded = false;
    // Route slot the bush ward was placed for. When the shared target moves to another slot, the ward
    // may be moved to a hidden bush near the new target (wards recharge every round and never expire).
    int ward_for_slot = -1;
    // Set from spawn until we reach the shared target again; everyone starts on spawn.
    bool rejoining = true;
    // v76: the camp a ranged champion is holding back from, since when, and the last round it held (FIX_STUCK).
    std::optional<vec2> hold_spot;
    int hold_from = 0, hold_last = -10;
    // v76: last round we left the enemy chase ring, and spawn.heal waits in a row (FIX_STUCK_RING / _HEAL).
    int left_ring_round = -10, spawn_waits = 0;
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
    // standable neighbours, or -1.
    std::vector<vec2> gate_altars;
    std::vector<int> gate_slots;
    // Round each route slot was last seen with its camps up (state 1), for altar_open().
    std::vector<int> seen_up;
    bool altar_open(vec2 altar) const;
    bool all_altars_shut() const;
    std::vector<vec2> escort_altars;
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
    std::optional<bc::distance_field> spawn_steps;
    bc::bitboard spawn_zone;
    // near_enemies() result when it had to drop someone, how many it dropped, and the vision
    // list and round it was worked out for.
    std::vector<bc::champion> near_buf;
    int far_skipped = 0;
    const std::vector<bc::champion>* near_src = nullptr;
    std::uint32_t near_round = 0;
    // near_enemies()' search field and boards, reused so no search allocates.
    std::optional<bc::distance_field> near_field;
    bc::bitboard near_roots, near_reached;
    // v71: health at the end of our last turn and its round, to spot damage nobody visible dealt; the round
    // the last such hit is remembered until, and the tiles an attacker hidden in a nearby bush can reach.
    int last_health = -1;
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
    // With the draft's per-altar solo flags (bc::map::altars() order) the rule is not worked out again.
    void split_altars(const std::vector<bool>* solo = nullptr);
    void observe();
    int wait_at(int slot, vec2 origin) const;
    void update_emblems();
    int team_emblems() const;
    // v106: team emblems needed before a monolith (MONOLITH_BY_SIZE).
    int monolith_emblems() const;
    bool allowed(int slot);
    bool monolith_grouped(int slot);
    // No route group but the monoliths carries gems: the monoliths are the game, not a last resort.
    bool gem_starved = false;
    // Drops monoliths from a travel table unless they are the last resort (or already under attack).
    struct Travel;
    void drop_monoliths(Travel& travel);
    bool can_take_camp(const bc::camp& camp);
    const Walk& group_field(int slot);
    void build_fields();
    std::optional<int> gap(int start, int goal);
    bool mid_clear(int slot);
    std::vector<int> visible_loot(int skip);
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
    std::uint32_t travel_raw_round = 0;
    vec2 travel_raw_origin{-1, -1};
    int travel_raw_skip = -2, travel_raw_fields = -1;
    int best_goal(const Travel& travel) const;
    Travel travel(vec2 origin, int skip);
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
    // v71: an enemy champion (seen, or hidden and hitting us) can reach us.
    bool enemy_threat(const bc::champion& me) const;
    void vesper_claim(const bc::champion& me);
    // v71: claims a reward only we can see when no claim is open and nobody visible is nearer.
    void self_claim(const bc::champion& me);
    // v94: Vesper collects first until she owns VESPER_FUNNEL_EMBLEMS (v106; v105 VESPER_CORE_EMBLEMS).
    bool vesper_funnel(const bc::champion& champion) const;
    bool drafted_vesper();
    bool collect(const bc::champion& me);
    int score_mode(std::optional<vec2> origin, int current = FARM);
    // v81: score_mode's origin-free terms and their reason text, for the round and vision lists they were read from.
    struct ScoreBase {
        std::uint32_t round = 0;
        const void* enemies = nullptr;
        const void* allies = nullptr;
        double head = 0, tail = 0;
        std::string head_text, tail_text;
        bool valid = false;
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
    std::vector<int> open_ward_spots(const bc::champion& me);
    bool place_ward(const bc::champion& me);
    void build_bush_boards();
    bool bush_ward(const bc::champion& me);
    bool follow_ward(const bc::champion& me, int target);
    // v72: moves our ward beside us every round (FIX_SCOUT_WARD).
    bool scout_ward(const bc::champion& me);
    bool ward_detour(const bc::champion& me, int gems);

    // tribute.cpp
    bool bank_gems(const bc::champion& me, const std::vector<vec2>& altars = {}, const std::string& stem = "bank");
    bool carrier_alive() const;
    void update_carrier(const bc::champion& me);
    std::optional<vec2> escort_altar() const;
    bool carry(const bc::champion& me);
    bool escort(const bc::champion& me);
    bool guard(const bc::champion& me, vec2 altar);
    bool recall_to_shop(const bc::champion& me);

    // combat.cpp
    bool fight(const bc::champion& me);
    // Visible enemies within FIGHT_STEPS walking steps of us or a visible ally: the ones that count
    // for the fight score and fight targets. The list stays valid until the next call.
    const std::vector<bc::champion>& near_enemies();
    const std::vector<bc::champion>& near_by_steps();
    std::pair<bool, std::string> chase_line(const bc::champion& me, const bc::champion& enemy);
    bool retreat(const bc::champion& me);
    // v77: reads, ages and sends the support ping; answers it on a fight turn with no enemy in sight.
    void update_ping(const bc::champion& me);
    bool answer_ping(const bc::champion& me);
    // v79: whether this champion may answer the current ping (health, bag, distance, not its own).
    bool can_answer(const bc::champion& champion, int now);
    // v81: our ping is fresh and we are outnumbered: walk toward help (PING_RETREAT).
    bool retreat_to_allies(const bc::champion& me);
    // v72: the hurt-dodge step: dash out, stun a chaser (Cassi), walk toward an ally, melee hits back (FIX_DODGE).
    bool escape(const bc::champion& me, const bc::bitboard& zone);
    std::vector<bc::camp> group_camps();
    // v97 (FIX_WALK_CLAIM): whether slot s's reward tiles can be walked to from `from`; true while its field is unbuilt.
    bool walkable(int s, vec2 from) const;
    // v97 (FIX_WALK_TARGET): the best group we can walk to instead of target s, or -1 when s is fine or none fits.
    int walk_target(const bc::champion& me, int s);
    // v103 (FIX_POCKET): a dasher in a hole pocket recalls out when hopping to target s is slower; whether it did.
    bool pocket_recall(const bc::champion& me, int s, const std::string& stem);
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
    bool ryth_stun(const bc::champion& me, const Target& target, bool is_camp);
    bool stellar_cast(const bc::champion& me, const Target& target, bool is_camp);
    bool cassi_stun(const bc::champion& me, const std::vector<bc::champion>& enemies);
    bool heal_lowest_ally(const bc::champion& me);
    void engage_vesper(const bc::champion& me, const Target& target, const std::vector<bc::champion>& enemies,
                       bool is_camp, const bc::bitboard* avoid);
    bool vesper_kite(const bc::champion& me, const std::vector<bc::champion>& enemies, const Target& target, int reach);
    // v94 (VARG_SPLASH): the camp in reach whose splash hits the most other camps.
    Target varg_splash_target(const bc::champion& me, const Target& target);
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
    // v81 mages (MAGE_STANDOFF, MAGE_LEAVE, MAGE_KITE, MAGE_MONOLITH).
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
    bool mages_drafted();
    // Whether monoliths are the mages' job (MAGE_MONOLITH with a mage drafted; MONOLITH_TEAM_STARVED keeps the old
    // whole-team monolith on gem-starved maps).
    bool monolith_delegated();
    std::optional<bool> mage_team;
    // Reach of every visible monolith already hit (melees keep out of it).
    bc::bitboard awake_monoliths();
    // The monolith slot this mage is working on, -1 for none.
    int mono_job = -1;
    // The monolith we last worked on and the round, so a job interrupted by a fight or a bank is resumed.
    int mono_last = -1, mono_hit = -1000;
    bool mage_monolith(const bc::champion& me);
    // Puts the ward into a hidden monolith's bush so spells can see it.
    bool mono_ward(const bc::champion& me, int s);
    // Tiles within MAGE_CAST_RANGE that see one of the monolith's tiles, by slot, built on first use.
    std::vector<std::optional<bc::bitboard>> mono_sights;
    bc::bitboard mono_sight(int s);
    bool monolith_guard(const bc::champion& me, int s, const bc::bitboard* avoid, const std::string& stem);

    // shop.cpp
    int vesper_recall_gold(const bc::champion& me) const;
    void buy_items(const bc::champion& me);
    void shop(const bc::champion& me, Champ kind);
};

// Ward spots near the map centre and on a ring around the enemy spawn.
std::vector<vec2> compute_ward_spots();
