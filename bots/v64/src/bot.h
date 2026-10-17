// Champion decisions. main.cpp dispatches here after the draft.
#pragma once
#include "actions.h"
#include "board.h"
#include "constants.h"
#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

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
    // Each ally's emblem key, by roster index, once read.
    std::vector<std::optional<int>> ally_keys;
    std::vector<vec2> centers;
    std::vector<std::vector<vec2>> spawn_lists;
    // Spawn tile to slot (-1 for none), for reading visible camps without a pass over every group.
    // A flat grid indexed by tile: a map would allocate a node per tile during setup.
    std::vector<std::int16_t> slot_grid;
    int slot_at(vec2 tile) const {
        if (tile.x < 0 || tile.y < 0 || tile.x >= bc::map::width() || tile.y >= bc::map::height()) return -1;
        return slot_grid[static_cast<std::size_t>(tile.y) * bc::map::width() + tile.x];
    }
    bc::bitboard route_tiles;
    // Walking steps between groups, cached per destination group since terrain never changes.
    // By slot; a flat vector so travel() needs no map lookup per group.
    std::vector<std::optional<Walk>> goal_fields;
    int goal_field_count = 0;
    // Most fuel one goal field has cost to build, for build_fields() to budget the next one.
    std::uint64_t field_cost = 0;
    std::map<std::pair<int, int>, std::optional<int>> gaps;
    // Per-slot value while farming (gold weighted above gems), then the original rating.
    std::vector<double> values[2];
    std::vector<double> clears;
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
    bool vesper_done = false;
    bool aggressive = false;
    // Gold Vesper has spent toward the current VESPER_RECALL_GOLD batch.
    int batch_spent = 0;
    std::vector<vec2> ward_spots;
    // Index of the ward spot our own ward covers, or none while it is free to move.
    std::optional<int> ward_slot;
    // Fixed anti-ward zone around each ward spot, and their union, built on first use.
    std::optional<std::vector<bc::bitboard>> ward_zones;
    bc::bitboard ward_union;
    // Set from spawn until we reach the shared target again; everyone starts on spawn.
    bool rejoining = true;
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
    std::vector<vec2> escort_altars;
    // Set for the rest of a turn after a channel is cancelled, so it is not started again.
    bool channel_cut = false;
    // Decision log for this turn: the shared plan, the branch taken, and why.
    std::string shared_why;
    std::string local_why;
    std::string branch_name;
    std::string branch_why;
    std::vector<std::string> notes;
    std::string score_why;
    // Last place each ally was seen, so a closer melee can be told from one walking off.
    // Indexed by champion + 1; a fixed array, as a map would allocate nodes every turn.
    std::array<std::optional<std::pair<int, vec2>>, 16> ally_at{};
    std::optional<int> spawn_gap;
    int ally_in_ring = 0;
    std::vector<vec2> ally_spawn_list;
    std::vector<vec2> enemy_spawn_list;
    std::optional<bc::distance_field> spawn_steps;
    bc::bitboard spawn_zone;

    // turn.cpp
    void play(const bc::champion& me);

    // route.cpp
    void setup();
    int steps_to_enemy_spawn(vec2 position);
    void split_altars();
    void observe();
    int wait_at(int slot, vec2 origin) const;
    void update_emblems();
    int team_emblems() const;
    bool allowed(int slot) const;
    bool can_take_camp(const bc::camp& camp) const;
    const Walk& group_field(int slot);
    void build_fields();
    std::optional<int> gap(int start, int goal);
    bool mid_clear(int slot);
    std::vector<int> visible_loot(int skip);
    double route_rate_steps(int slot, int steps) const;
    // Walking steps to each route group by slot; -1 where a group is skipped, not allowed or unreachable.
    // A flat vector: a map allocated a node for every group on every call.
    struct Travel {
        std::vector<int> steps;
        int reachable = 0;
        bool has(int s) const { return s >= 0 && s < static_cast<int>(steps.size()) && steps[s] >= 0; }
        int at(int s) const { return steps[s]; }
        bool empty() const { return reachable == 0; }
    };
    int best_goal(const Travel& travel) const;
    Travel travel(vec2 origin, int skip);
    int pick_target(int slot, vec2 origin, int skip, bool seen_only = false);
    void update_rejoining(const bc::champion& me, int slot);
    std::optional<std::string> lead_block(const bc::champion& me, int stamp, int now);
    void share_loot(const bc::champion& me);
    void update_capture(const bc::champion& me, int& capture_slot, int& collector, int& claimed);
    void sync_decision(const bc::champion& me);
    bool is_collector(const bc::champion& me) const;
    void vesper_claim(const bc::champion& me);
    bool collect(const bc::champion& me);
    int score_mode(std::optional<vec2> origin, int current = FARM);
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
    std::pair<bool, std::string> chase_line(const bc::champion& me, const bc::champion& enemy);
    bool retreat(const bc::champion& me);
    std::vector<bc::camp> group_camps();
    std::vector<bc::camp> camps_at(int slot);
    void farm(const bc::champion& me, const bc::bitboard* avoid = nullptr, const std::string& stem = "farm");

    // kits.cpp
    void engage(const bc::champion& me, const Target& target, const std::vector<bc::champion>& enemies, bool is_camp,
                const bc::bitboard* avoid = nullptr);
    void engage_melee(const bc::champion& me, const Target& target, bool is_camp, const bc::bitboard* avoid);
    void engage_ranged(const bc::champion& me, const Target& target, const std::vector<bc::champion>& enemies,
                       bool is_camp, const bc::bitboard* avoid);
    bool backing_off_camp(const bc::champion& me, const Target& target, bool is_camp, int gap);
    int camp_standoff(const Target& camp) const;
    bool should_hold_back(const bc::champion& me, const Target& camp);
    std::vector<bc::champion> visible_melees(const bc::champion& me);
    std::vector<bc::champion> closer_melees(const bc::champion& me, const Target& camp);
    std::optional<int> camp_slot(const Target& camp) const;
    bool coming_to_camp(const bc::champion& ally, const Target& camp);
    void remember_allies();
    void hold_camp(const bc::champion& me, const Target& camp, const bc::bitboard* avoid);
    bool willow_follow(const bc::champion& me, const Target& camp, const bc::bitboard& danger, const bc::bitboard* avoid);
    bool ryth_stun(const bc::champion& me, const Target& target, bool is_camp);
    bool stellar_cast(const bc::champion& me, const Target& target, bool is_camp);
    bool cassi_stun(const bc::champion& me, const std::vector<bc::champion>& enemies);
    bool heal_lowest_ally(const bc::champion& me);
    void engage_vesper(const bc::champion& me, const Target& target, const std::vector<bc::champion>& enemies,
                       bool is_camp, const bc::bitboard* avoid);
    bool vesper_kite(const bc::champion& me, const std::vector<bc::champion>& enemies, const Target& target, int reach);

    // shop.cpp
    int vesper_recall_gold(const bc::champion& me) const;
    void buy_items(const bc::champion& me);
    void shop(const bc::champion& me, Champ kind);
};

// Ward spots near the map centre and on a ring around the enemy spawn.
std::vector<vec2> compute_ward_spots();
