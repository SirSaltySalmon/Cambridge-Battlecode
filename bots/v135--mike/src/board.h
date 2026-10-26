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

// v99 fuel helpers. Measured (fuelbench, -O3 release): a host call is cheap (is_set ~25 over loop overhead, get ~32)
// while guest instructions are dear: reading a board or field straight from its chunked storage cost ~50 and ~95, so
// is_set()/get() stay. What costs is allocation: a new bitboard ~210-300, a new distance_field ~1,550.
namespace fast {
// Board results written into existing storage instead of a new board. out may be a or b.
inline void and_into(const bc::bitboard& a, const bc::bitboard& b, bc::bitboard& out) {
    bc::detail::check(bc::detail::sdk_Bitboard_Intersect(a.ptr(), b.ptr(), out.ptr()));
}
inline void not_into(const bc::bitboard& a, bc::bitboard& out) { bc::detail::check(bc::detail::sdk_Bitboard_Not(a.ptr(), out.ptr())); }
inline void copy_into(const bc::bitboard& a, bc::bitboard& out) {
    bc::detail::check(bc::detail::sdk_Bitboard_Union(a.ptr(), a.ptr(), out.ptr()));
}
// out = the tiles within radius of center (tiles_around without a new board).
inline void around_into(vec2 center, float radius, bc::bitboard& out) {
    out.clear();
    out.draw_chebyshev(center, radius);
}
// The 8-neighbour dilation of in, into out (tmp and h are scratch; out may be in).
inline void dilate_into(const bc::bitboard& in, bc::bitboard& tmp, bc::bitboard& h, bc::bitboard& out) {
    copy_into(in, h);
    in.shift_right_into(1, tmp);
    h |= tmp;
    in.shift_right_into(-1, tmp);
    h |= tmp;
    copy_into(h, out);
    h.shift_up_into(1, tmp);
    out |= tmp;
    h.shift_up_into(-1, tmp);
    out |= tmp;
}
// The engine's tile order (what positions() lists): 8x8 chunks, chunk_stride chunks per chunk row, row-major within a
// chunk (checked against positions() on 50, 60 and 100 wide maps).
inline std::uint32_t rank(vec2 p) {
    const auto word = static_cast<std::uint32_t>(p.y >> 3) * bc::detail::layout().chunk_stride + static_cast<std::uint32_t>(p.x >> 3);
    return word * 64 + static_cast<std::uint32_t>((p.y & 7) * 8 + (p.x & 7));
}
}  // namespace fast

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
// v99: the same into existing storage (the vision boards are read into one scratch board, not five new ones).
void blocked_tiles_into(bc::bitboard& out);

std::string no_step_reason(vec2 origin, const bc::bitboard* reached);
