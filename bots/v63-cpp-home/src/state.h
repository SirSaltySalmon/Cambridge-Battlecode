// Shared memory, carried value, and who is threatening us.
#pragma once
#include "constants.h"
#include <vector>

int emblem_count(const bc::champion& champion);
// Gold sunk into shards and emblems; other champions' purses can't be read.
int item_worth(const bc::champion& champion);
// Every ally is listed even out of vision, and liveness is readable for all of them.
bool team_alive();
int gems_carried(const bc::champion& champion);
bc::bitboard threat_zone(const std::vector<bc::champion>& enemies, bool include_camps);
// An enemy that can already hit us, or a damaged (awake) camp in its own range.
bool under_threat(const bc::champion& me);

struct Shared {
    int mode, slot, capture_slot, collector, stamp, claimed;
    bool vesper_done, aggressive;
};
Shared read_shared();
void write_shared(int mode, int slot, int capture_slot, int collector, int claimed, bool vesper_done,
                  bool aggressive, int carrier);
void mark_vesper_done();
int read_carrier();
void write_carrier(int carrier);

// One shared-memory byte.
int shared_byte(int offset);
void set_shared_byte(int offset, int value);
