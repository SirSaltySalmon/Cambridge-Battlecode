// Shared memory, carried value, and who is threatening us.
#pragma once
#include "constants.h"
#include <optional>
#include <vector>

int emblem_count(const bc::champion& champion);
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
int camp_exit_cost(const bc::champion& me);

struct Shared {
    int mode, slot, capture_slot, collector, stamp, claimed;
    bool vesper_done, aggressive;
    // v86: the leader's phase and the split pair's target (NO_TARGET2 for none).
    int phase, target2;
};
Shared read_shared();
// v86: the pair's target bits in the phase byte are kept; the leader only writes the phase.
void write_shared(int mode, int slot, int capture_slot, int collector, int claimed, bool vesper_done,
                  bool aggressive, int carrier, int phase);
// v86: the phase alone (a split ended by anyone), and the pair's target alone (Issac).
void write_phase(int phase);
void write_target2(int target);
void mark_vesper_done();
int read_carrier();
void write_carrier(int carrier);

// One shared-memory byte.
int shared_byte(int offset);
void set_shared_byte(int offset, int value);
