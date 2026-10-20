// Tiles, walking distance, and what blocks a step.
#pragma once
#include "constants.h"
#include <optional>
#include <string>
#include <vector>

// printf-style formatting into a std::string, for log lines.
std::string format(const char* pattern, ...) __attribute__((format(printf, 1, 2)));
// Error text as Python prints it: the engine message alone, without the "engine error N: " prefix.
inline std::string error_text(const std::exception& error) {
    if (const auto* engine = dynamic_cast<const bc::error*>(&error)) return engine->message();
    return error.what();
}
// Print one line to the bot's stdout (the replay's team-a/b.stdout).
void say(const std::string& line);
void say(const std::string& prefix, const std::string& line);
// v90-log: log-only statements, run only inside debug_block (no fuel in debug builds); a failed read while building
// the text is dropped, so logging can never cost fuel or end a turn. See WHY and NOTE in bot.h.
#define LOG(...) do { debug_block({ try { __VA_ARGS__ } catch (const std::exception&) {} }); } while (0)

// Python 3.12+ sum() of floats uses Neumaier compensated summation; matching it keeps every float
// score bit-identical to the Python bot (mirrored pockets tie exactly instead of by rounding luck).
struct PySum {
    double total = 0, carry = 0;
    void add(double x) {
        const double t = total + x;
        carry += (total >= 0 ? total : -total) >= (x >= 0 ? x : -x) ? (total - t) + x : (x - t) + total;
        total = t;
    }
    double value() const { return carry != 0 && carry - carry == 0 ? total + carry : total; }
};

inline int distance(vec2 a, vec2 b) { return static_cast<int>(a.chebyshev_distance(b)); }

std::string at(vec2 position);

std::string step_room(vec2 origin);
std::string goal_cover(const bc::bitboard& raw, const bc::bitboard& blocked);

bc::bitboard tiles_around(vec2 position, float radius);

// Walking steps from the roots around terrain; camps and champions move or die, so they don't block.
struct Walk {
    bc::distance_field field;
    bc::bitboard reached;
};
Walk walk_field(const bc::bitboard& roots);
std::optional<int> steps_to(const Walk& walk, const std::vector<vec2>& tiles);

// Combine terrain and visible occupants using native bitboard operations.
bc::bitboard blocked_tiles();

std::string no_step_reason(vec2 origin, const bc::bitboard* reached);
