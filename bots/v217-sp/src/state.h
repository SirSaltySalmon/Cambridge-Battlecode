// Shared memory, carried value, and who is threatening us.
#pragma once
#include "constants.h"
#include <algorithm>
#include <optional>
#include <vector>

// One item read. It also refreshes the champion's cached emblems and families (owned_of).
int emblem_count(const bc::champion& champion);

// v134: finished emblem families, bit f for bc::item_family f.
using Passives = std::uint32_t;
struct Owned {
    int emblems = 0;
    Passives passives = 0;
};
// The last successful emblem_count read of this champion (zeros before any); no engine read.
Owned owned_of(const bc::champion& champion);
inline bool has(Passives p, bc::item_family f) { return (p >> static_cast<int>(f) & 1) != 0; }
// Collectors: 1% more damage per unique finished family, itself included.
inline double collectors_mult(Passives p) {
    return has(p, bc::item_family::collectors) ? 1.0 + 0.01 * __builtin_popcount(p) : 1.0;
}
// Damage dealt to camps (Monster) and to champions (Skirmish), both with Collectors (ITEM_DAMAGE).
inline double camp_dmg_mult(Passives p) {
    if (!ITEM_DAMAGE) return 1.0;
    return (has(p, bc::item_family::monster) ? 1.15 : 1.0) * collectors_mult(p);
}
inline double champ_dmg_mult(Passives p) {
    if (!ITEM_DAMAGE) return 1.0;
    return (has(p, bc::item_family::skirmish) ? 1.05 : 1.0) * collectors_mult(p);
}
// Damage taken from camps (Jungle, Durability) and from champions (Durability).
inline double camp_taken_mult(Passives p) {
    if (!ITEM_DAMAGE) return 1.0;
    return (has(p, bc::item_family::jungle) ? 0.7 : 1.0) * (has(p, bc::item_family::durability) ? 0.9 : 1.0);
}
inline double champ_taken_mult(Passives p) {
    if (!ITEM_DAMAGE) return 1.0;
    return has(p, bc::item_family::durability) ? 0.9 : 1.0;
}
// v181 (KILL_TIME): each kit's spell damage per 8-round cooldown in Focus (both slots that deal damage), and its attack
// resets per cooldown; the same table as mono_session's.
inline double kit_spell_mult(Champ kind) {
    switch (kind) {
        case Champ::stellar: case Champ::cassi: case Champ::marcella: return 2.0;
        case Champ::prism: return 2.5;  // both slots: 1.5 single + 1.0 area
        case Champ::tavi: return 1.6;
        case Champ::heart_of_ruin: return 1.5;
        case Champ::ren: return 1.1;
        case Champ::ryth: return 1.0;
        default: return 0.0;
    }
}
inline double kit_resets(Champ kind) {
    switch (kind) {
        case Champ::issac: case Champ::vesper_vale: case Champ::vendetta: case Champ::marcella: case Champ::tavi:
        case Champ::willow: case Champ::ryth: case Champ::varg: return 1.0;
        default: return 0.0;
    }
}
// Damage a champion deals per round: basic attacks (with resets) plus spells spread over their cooldown. Focus includes
// Prism's 1.5 per carried gem, so a heavy bag counts in full.
inline double round_damage(Champ kind, double strength, double focus) {
    return strength * (1.0 + kit_resets(kind) / 8.0) + focus * kit_spell_mult(kind) / 8.0;
}
// Channel lengths in owner turn ends (ITEM_CHANNELS): Home halves a recall, rounded up; Tribute cuts a third, rounded up.
inline int recall_ticks(Passives p) {
    const int base = bc::constants::recall_time;
    return ITEM_CHANNELS && has(p, bc::item_family::home) ? (base + 1) / 2 : base;
}
inline int tribute_ticks(int gems, Passives p) {
    const int base = std::max(1, (gems + 9) / 10);
    if (!ITEM_CHANNELS || !has(p, bc::item_family::tribute)) return base;
    return std::max(1, (base * 67 + 99) / 100);
}
// A bag whose tribute is long enough to need an escort: ESCORT_TICKS of channel (ITEM_CHANNELS), else ESCORT_GEMS.
inline bool escort_bag(int gems, Passives p) {
    if (NO_ESCORT) return false;
    return ITEM_CHANNELS ? gems > 0 && tribute_ticks(gems, p) >= ESCORT_TICKS : gems >= ESCORT_GEMS;
}
// Gold sunk into shards and emblems; other champions' purses can't be read.
int item_worth(const bc::champion& champion);
// Every ally is listed even out of vision, and liveness is readable for all of them.
bool team_alive();
int gems_carried(const bc::champion& champion);
bc::bitboard threat_zone(const std::vector<bc::champion>& enemies, bool include_camps);
// v82 (RECALL_SPELL_CAUTION): threat_zone, grown around casters to spell reach + RECALL_SPELL_STEP. Where a recall may start.
bc::bitboard recall_zone(const std::vector<bc::champion>& enemies, bool include_camps);
// How far an enemy can hit in one of its turns, before its own walk (RECALL_REACH_MODE).
int recall_reach(const bc::champion& enemy);
// The visible enemy whose grown zone alone covers the tile (threat_zone does not), for the log.
std::optional<bc::champion> recall_caster(const std::vector<bc::champion>& enemies, vec2 tile);
// An enemy that can already hit us, or a damaged (awake) camp in its own range.
bool under_threat(const bc::champion& me);
// v93 (V93_CAMP_EXIT): what walking out of the awake camps hitting us costs: their strength (camps whose nearest
// champion is us) times the steps to leave the furthest one's reach. 0 when none hits us.
// v148-normal (CAMP_TIE_ORDER): an ally level with us that acts first in our turn order is the camp's target.
int camp_exit_cost(const bc::champion& me);
// v148-normal (CAMP_TIE_ORDER): a camp hits the nearest champion, and on a tie between allies the one that acts first
// (higher priority, then draft order; measured 1253 of 1254 ties in 25 replays). Whether `ally` acts before us.
bool acts_before_me(const bc::champion& ally);

struct Shared {
    int mode, slot, capture_slot, collector, stamp, claimed;
    bool vesper_done, aggressive;
    // v86: the leader's phase and the split pair's target (NO_TARGET2 for none).
    int phase, target2;
    // v202 (PT_COMMIT): ENGAGED_FLAG: a champion hit a camp of the shared target since the leader last wrote.
    bool engaged;
};
Shared read_shared();
// v86: the pair's target bits in the phase byte are kept; the leader only writes the phase.
void write_shared(int mode, int slot, int capture_slot, int collector, int claimed, bool vesper_done,
                  bool aggressive, int carrier, int phase);
// v86: the phase alone (a split ended by anyone), and the pair's target alone (Issac).
void write_phase(int phase);
void write_target2(int target);
void mark_vesper_done();
// v202 (PT_COMMIT): sets ENGAGED_FLAG on the stamp byte (the leader's next write clears it).
void mark_engaged();
int read_carrier();
void write_carrier(int carrier);
// v135 (NO_ESCORT, TANK_SWAP): the carrier bits name the tank on duty instead: champion id + 1, 0 for none.
inline int shared_tank() { return read_carrier(); }
inline void write_tank(int ident) { write_carrier(ident & 0x0F); }

// v183 (FOCUS): the team's focus target, as an index in seen::roster(Team::enemy) (-1 for none).
int read_focus();
void write_focus(int index);
// v216 (FOCUS_KEEP): the whole focus nibble (index + 1 and the refresh bit), watched for changes.
int focus_nibble();

// One shared-memory byte.
int shared_byte(int offset);
void set_shared_byte(int offset, int value);
