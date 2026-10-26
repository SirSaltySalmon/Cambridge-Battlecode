#include "trace.h"
#include "seen.h"

namespace trace {

std::vector<std::string> acts;
std::string stall;
std::string attack;
std::string guard;
std::string via;

void reset() {
    acts.clear();
    stall.clear();
    attack.clear();
    guard.clear();
    via.clear();
    seen::new_turn();
}

void act(const std::string& kind) {
    acts.push_back(kind);
    seen::reset();
}

void moved(const std::string& kind) {
    stall.clear();
    acts.push_back(kind);
    seen::reset();
}

void move_failed(const std::string& reason) {
    stall = reason;
    // A rejected step may have revealed a hidden unit in a bush.
    if (reason.rfind("engine rejected", 0) == 0) seen::reset();
}

void attack_failed(const std::string& reason) {
    attack = reason;
}

} // namespace trace
