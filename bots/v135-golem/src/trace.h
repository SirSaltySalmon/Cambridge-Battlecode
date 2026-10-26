// One turn's action trace. The turn resets it; movement and casts append to it.
#pragma once
#include <string>
#include <vector>

namespace trace {

extern std::vector<std::string> acts;
extern std::string stall;   // empty means none
extern std::string attack;  // empty means none
// v134-move: a walk this turn went around the walk guard (awake camps), and what the guard held (log only).
extern std::string guard;
// v134-move: the route a move took when not the plain walk ("hop"), for the log.
extern std::string via;

void reset();
void act(const std::string& kind);
void moved(const std::string& kind);
void move_failed(const std::string& reason);
void attack_failed(const std::string& reason);

} // namespace trace
