#include "board.h"
#include "seen.h"
#include <algorithm>
#include <charconv>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {

// Appends to a fixed stack buffer, so a formatted string costs one allocation at the end
// (every std::string growth is a malloc, ~300+ fuel each in WebAssembly).
struct Out {
    char data[768];
    std::size_t size = 0;
    std::string spill;  // only used if a line outgrows the buffer
    void put(char c) {
        if (size < sizeof data) data[size++] = c;
        else spill += c;
    }
    void put(const char* text) {
        while (*text) put(*text++);
    }
    void put(const char* begin, const char* end) {
        while (begin != end) put(*begin++);
    }
    std::string str() const { return spill.empty() ? std::string(data, size) : std::string(data, size) + spill; }
};

// Fixed-point text of a double with correct round-half-even on its exact binary value: the digits
// printf and Python's format() give, using 128-bit integer arithmetic instead of software long double.
void put_fixed(Out& out, double value, int precision) {
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    const bool negative = bits >> 63;
    const int exponent_field = static_cast<int>(bits >> 52 & 0x7FF);
    const std::uint64_t fraction = bits & ((std::uint64_t{1} << 52) - 1);
    char buffer[64];
    if (exponent_field == 0x7FF || precision > 9 || (exponent_field != 0 && exponent_field - 1075 > 20)) {
        // Inf, NaN, huge numbers or long fractions never occur in the log; keep them correct anyway.
        const auto result = std::to_chars(buffer, buffer + sizeof buffer, value, std::chars_format::fixed, precision);
        out.put(buffer, result.ptr);
        return;
    }
    std::uint64_t scale = 1;
    for (int i = 0; i < precision; ++i) scale *= 10;
    std::uint64_t rounded = 0;
    if (exponent_field != 0) {  // subnormals round to zero at these precisions
        const std::uint64_t mantissa = fraction | std::uint64_t{1} << 52;
        const int shift = exponent_field - 1075;  // value = mantissa * 2^shift
        // 64x64 -> 128-bit product and shifts are cheap; 128-bit division is emulated, so it is avoided.
        const unsigned __int128 scaled = static_cast<unsigned __int128>(mantissa) * scale;
        unsigned __int128 wide = 0;
        if (shift >= 0) {
            wide = scaled << shift;
        } else if (-shift < 120) {
            const int down = -shift;
            wide = scaled >> down;
            const unsigned __int128 rest = scaled & ((static_cast<unsigned __int128>(1) << down) - 1);
            const unsigned __int128 half = static_cast<unsigned __int128>(1) << (down - 1);
            if (rest > half || (rest == half && (wide & 1))) ++wide;
        }
        if (wide >> 63) {  // too big for the 64-bit digit loop
            const auto result = std::to_chars(buffer, buffer + sizeof buffer, value, std::chars_format::fixed, precision);
            out.put(buffer, result.ptr);
            return;
        }
        rounded = static_cast<std::uint64_t>(wide);
    }
    if (negative) out.put('-');
    std::uint64_t whole = rounded / scale;
    std::uint64_t part = rounded % scale;
    char digits[24];
    int n = 0;
    do {
        digits[n++] = static_cast<char>('0' + whole % 10);
        whole /= 10;
    } while (whole);
    while (n) out.put(digits[--n]);
    if (precision > 0) {
        out.put('.');
        for (int i = precision - 1; i >= 0; --i) {
            digits[i] = static_cast<char>('0' + part % 10);
            part /= 10;
        }
        out.put(digits, digits + precision);
    }
}

template <class Int>
void put_int(Out& out, Int value) {
    char buffer[32];
    const auto result = std::to_chars(buffer, buffer + sizeof buffer, value);
    out.put(buffer, result.ptr);
}

} // namespace

// A small printf subset (%d %+d %u %zu %llu %s %.Nf %%) for log text. vsnprintf formats floats
// through software 128-bit long double in WebAssembly and grows strings piecemeal.
std::string format(const char* pattern, ...) {
    Out out;
    va_list args;
    va_start(args, pattern);
    for (const char* p = pattern; *p; ++p) {
        if (*p != '%') {
            out.put(*p);
            continue;
        }
        ++p;
        if (*p == '%') {
            out.put('%');
            continue;
        }
        const bool plus = *p == '+';
        if (plus) ++p;
        int precision = 6;
        if (*p == '.') {
            precision = 0;
            for (++p; *p >= '0' && *p <= '9'; ++p) precision = precision * 10 + (*p - '0');
        }
        int longs = 0;
        bool size = false;
        if (*p == 'z') {
            size = true;
            ++p;
        }
        while (*p == 'l') {
            ++longs;
            ++p;
        }
        switch (*p) {
        case 'd': {
            const int value = va_arg(args, int);
            if (plus && value >= 0) out.put('+');
            put_int(out, value);
            break;
        }
        case 'u':
            if (size) put_int(out, va_arg(args, std::size_t));
            else if (longs >= 2) put_int(out, va_arg(args, unsigned long long));
            else if (longs == 1) put_int(out, va_arg(args, unsigned long));
            else put_int(out, va_arg(args, unsigned));
            break;
        case 's': out.put(va_arg(args, const char*)); break;
        case 'f': put_fixed(out, va_arg(args, double), precision); break;
        default: out.put('?'); break;
        }
    }
    va_end(args);
    return out.str();
}

void say(const std::string& line) {
    std::fwrite(line.data(), 1, line.size(), stdout);
    std::fputc('\n', stdout);
}

void say(const std::string& prefix, const std::string& line) {
    std::fwrite(prefix.data(), 1, prefix.size(), stdout);
    say(line);
}

std::string at(vec2 position) {
    return format("(%d,%d)", position.x, position.y);
}

std::string step_room(vec2 origin) {
    // Tiles a single step can reach, and which rule blocks each one. Enemy spawn counts as a wall.
    // In-place board operations: each operator that returns a new board allocates one.
    auto& me = bc::self();
    auto rest = tiles_around(origin, static_cast<float>(me.base_stats().base_move_speed));
    rest.set(origin, false);
    const auto total = rest.count();
    bc::bitboard part;
    auto take = [&](const bc::bitboard& mask) {
        part.clear();
        part |= rest;
        part &= mask;
        rest ^= part;
        return part.count();
    };
    const auto n_enemy = take(bc::map::spawn_region(Team::enemy));
    const auto n_terrain = take(seen::terrain());
    const auto n_champ = take(bc::vision::champion_tiles());
    const auto n_camp = take(bc::vision::camp_tiles());
    const auto n_altar = take(bc::map::altars());
    return format("step-room %u: free %u, enemy-spawn %u, terrain %u, champion %u, camp %u, altar %u",
                  total, rest.count(), n_enemy, n_terrain, n_champ, n_camp, n_altar);
}

std::string goal_cover(const bc::bitboard& raw, const bc::bitboard& blocked) {
    const auto& enemy = bc::map::spawn_region(Team::enemy);
    const auto on_spawn = (raw & enemy).count();
    const auto other = (raw & blocked & ~enemy).count();
    std::string text;
    if (on_spawn) text = format("%u goals on enemy spawn", on_spawn);
    if (other) text += (text.empty() ? "" : ", ") + format("%u goals otherwise blocked", other);
    return text.empty() ? "goals filtered empty" : text;
}

bc::bitboard tiles_around(vec2 position, float radius) {
    bc::bitboard tiles;
    tiles.draw_chebyshev(position, radius);
    return tiles;
}

Walk walk_field(const bc::bitboard& roots) {
    Walk walk;
    walk.field.compute(seen::terrain(), roots, walk.reached);
    return walk;
}

std::optional<int> steps_to(const Walk& walk, const std::vector<vec2>& tiles) {
    // compute() writes every entry and leaves unreached tiles at `unreachable`, so one read per tile
    // answers both "reached?" and "how far?" (a reached.is_set() read costs ~4x a field read).
    std::optional<int> best;
    for (const auto tile : tiles) {
        const auto steps = walk.field.get(tile);
        if (steps != bc::distance_field::unreachable && (!best || steps < *best)) best = steps;
    }
    return best;
}

void blocked_tiles_into(bc::bitboard& out) {
    // In place: every `|` would make a new board (an allocation and a full-board copy), and vision::champion_tiles()
    // with no team is three boards.
    static bc::bitboard part{bc::uninitialized};
    fast::copy_into(seen::fixed_blocks(), out);
    bc::vision::champion_tiles_into(Team::ally, part);
    out |= part;
    bc::vision::champion_tiles_into(Team::enemy, part);
    out |= part;
    bc::vision::camp_tiles_into(part);
    out |= part;
    bc::vision::teleport_tiles_into(part);
    out |= part;
    out.set(bc::self().position(), false);
}

bc::bitboard blocked_tiles() {
    bc::bitboard blocked{bc::uninitialized};
    blocked_tiles_into(blocked);
    return blocked;
}

std::string no_step_reason(vec2 origin, const bc::bitboard* reached) {
    std::string text = "no closer step";
    if (reached && !reached->is_set(origin)) text += "; path never reached this tile (boxed in)";
    if (bc::map::spawn_region(Team::enemy).is_set(origin)) text += "; standing on enemy spawn";
    if (bc::map::spawn_region(Team::ally).is_set(origin)) text += "; standing on ally spawn";
    return text;
}
