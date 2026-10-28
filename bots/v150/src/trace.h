// One turn's action trace. The turn resets it; movement and casts append to it.
#pragma once
#include <string>
#include <vector>

namespace trace {

extern std::vector<std::string> acts;
extern std::string stall;   // empty means none
extern std::string attack;  // empty means none
// v135a audit: extra log-only lines (debug builds), printed under the turn line.
extern std::vector<std::string> audit;

void reset();
void act(const std::string& kind);
void moved(const std::string& kind);
void move_failed(const std::string& reason);
void attack_failed(const std::string& reason);

} // namespace trace
