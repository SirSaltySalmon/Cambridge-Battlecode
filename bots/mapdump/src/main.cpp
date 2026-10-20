// Prints the map as ASCII in the draft, then idles.
#include <cambc.h>
#include <iostream>
#include <string>
namespace bc = cambc;
int main() {
    bool done = false;
    while (true) {
        if (bc::is_draft() && !done) {
            done = true;
            const int w = bc::map::width(), h = bc::map::height();
            std::vector<std::string> g(h, std::string(w, '.'));
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    const bc::vec2 p{x, y};
                    if (bc::map::walls().is_set(p)) g[y][x] = '#';
                    else if (bc::map::holes().is_set(p)) g[y][x] = 'o';
                    else if (bc::map::bushes().is_set(p)) g[y][x] = '"';
                    if (bc::map::altars().is_set(p)) g[y][x] = 'A';
                    if (bc::map::spawn_region(bc::team::ally).is_set(p)) g[y][x] = 'S';
                    if (bc::map::spawn_region(bc::team::enemy).is_set(p)) g[y][x] = 'E';
                }
            for (const auto& group : bc::map::camp_groups())
                for (const auto& c : group.camps()) {
                    const char* k = "WKGgdM";
                    char ch = '?';
                    switch (c.kind()) {
                    case bc::camp_type::wolf: ch = 'W'; break;
                    case bc::camp_type::elk: ch = 'K'; break;
                    case bc::camp_type::golem: ch = 'G'; break;
                    case bc::camp_type::gem_cluster: ch = '*'; break;
                    case bc::camp_type::gold_cluster: ch = '$'; break;
                    case bc::camp_type::monolith: ch = 'M'; break;
                    default: break;
                    }
                    (void)k;
                    g[c.spawn_position().y][c.spawn_position().x] = ch;
                }
            std::string out = "MAP\n";
            for (int y = 0; y < h; ++y) out += (y < 10 ? " " : "") + std::to_string(y) + " " + g[y] + "\n";
            bc::distance_field field;
            bc::bitboard reached;
            const auto blocked = bc::map::walls() | bc::map::holes() | bc::map::altars();
            field.compute(blocked, bc::map::spawn_region(bc::team::ally), reached);
            bc::bitboard camp_tiles;
            int gi = 0;
            for (const auto& group : bc::map::camp_groups()) {
                bool any = false;
                for (const auto& c : group.camps()) {
                    camp_tiles.set(c.spawn_position());
                    if (reached.is_set(c.spawn_position())) any = true;
                }
                if (!group.camps().empty() && !any)
                    out += "UNREACHABLE group " + std::to_string(gi) + " at " + std::to_string(group.camps()[0].spawn_position().x) + "," + std::to_string(group.camps()[0].spawn_position().y) + " gems " + std::to_string(group.gem_value()) + " gold " + std::to_string(group.gold_value()) + '\n';
                ++gi;
            }
            for (const auto a : bc::map::altars().positions()) {
                int free_tiles = 0, camp_only = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        const bc::vec2 t{a.x + dx, a.y + dy};
                        if (t.x < 0 || t.y < 0 || t.x >= w || t.y >= h || !reached.is_set(t)) continue;
                        if (camp_tiles.is_set(t)) ++camp_only; else ++free_tiles;
                    }
                if (free_tiles == 0)
                    out += "ALTAR " + std::to_string(a.x) + "," + std::to_string(a.y) + " free " + std::to_string(free_tiles) + " camp-only " + std::to_string(camp_only) + '\n';
            }
            std::cout << out << std::flush;
        }
        bc::end_turn();
    }
}
