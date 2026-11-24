#include "oldprobe.h"
#include "board.h"
#include "draft.h"
#include "seen.h"
#include <algorithm>

namespace oldc {

const Grid& grid() {
    static const Grid g = [] {
        Grid out;
        out.w = bc::map::width();
        out.h = bc::map::height();
        out.block = seen::fixed_blocks().export_linear_buffer();
        return out;
    }();
    return g;
}

namespace {

// Spread `one` (holding f) from the launch tiles a crystal helps: a tile takes v + 1 from a neighbour only when that
// beats what it holds, so the search dies wherever walking is already as good. Seeds pop in value order, merged with
// the FIFO of relaxed tiles (unit edges keep both sorted).
void spread(const Grid& g, std::vector<int>& launches, std::vector<std::uint16_t>& one) {
    std::sort(launches.begin(), launches.end(), [&](int a, int b) { return one[a] < one[b]; });
    std::vector<int> fifo;
    fifo.reserve(launches.size() * 4 + 64);
    std::size_t a = 0, b = 0;
    std::vector<std::uint16_t> at(launches.size());
    for (std::size_t k = 0; k < launches.size(); ++k) at[k] = one[launches[k]];
    std::vector<std::uint16_t> fv;
    fv.reserve(fifo.capacity());
    while (a < launches.size() || b < fifo.size()) {
        int i;
        std::uint16_t v;
        if (b < fifo.size() && (a >= launches.size() || fv[b] <= at[a])) {
            i = fifo[b];
            v = fv[b++];
        } else {
            i = launches[a];
            v = at[a++];
        }
        if (one[i] != v) continue;
        const int x = i % g.w, y = i / g.w;
        const std::uint16_t nv = static_cast<std::uint16_t>(v + 1);
        for (int dy = -1; dy <= 1; ++dy) {
            const int yy = y + dy;
            if (yy < 0 || yy >= g.h) continue;
            for (int dx = -1; dx <= 1; ++dx) {
                const int xx = x + dx;
                if ((dx == 0 && dy == 0) || xx < 0 || xx >= g.w) continue;
                const int j = yy * g.w + xx;
                if (g.block[j] || one[j] <= nv) continue;
                one[j] = nv;
                fifo.push_back(j);
                fv.push_back(nv);
            }
        }
    }
}

// Sliding minimum over a window of radius REACH along one line (van Herk / Gil-Werman: three reads a tile).
void line_min(const std::uint16_t* in, std::uint16_t* out, int len, int stride, std::vector<std::uint16_t>& pre,
              std::vector<std::uint16_t>& suf) {
    constexpr int W = 2 * REACH + 1;
    pre.resize(len);
    suf.resize(len);
    for (int i = 0; i < len; ++i) {
        const auto v = in[i * stride];
        pre[i] = i % W == 0 ? v : std::min(pre[i - 1], v);
    }
    for (int i = len - 1; i >= 0; --i) {
        const auto v = in[i * stride];
        suf[i] = (i % W == W - 1 || i == len - 1) ? v : std::min(suf[i + 1], v);
    }
    for (int i = 0; i < len; ++i) {
        const int lo = std::max(0, i - REACH), hi = std::min(len - 1, i + REACH);
        // [lo, hi] spans at most two blocks: the suffix of lo's block and the prefix of hi's.
        out[i * stride] = lo / W == hi / W ? (lo % W == 0 ? pre[hi] : std::min(suf[lo], pre[hi]) ) : std::min(suf[lo], pre[hi]);
    }
}

}  // namespace

void compute(const bc::bitboard& goals, Fields& out) {
    const auto& g = grid();
    const int n = g.w * g.h;
    bc::distance_field field;
    bc::bitboard reached;
    const auto roots = goals & ~seen::fixed_blocks();
    auto t0 = bc::fuel_remaining();
    field.compute(seen::fixed_blocks(), roots, reached);
    auto t1 = bc::fuel_remaining();
    out.f = field.export_linear_buffer();
    auto t2 = bc::fuel_remaining();
    say(format("TPSTAGE %dx%d compute %llu export %llu", g.w, g.h, (unsigned long long)(t0 - t1), (unsigned long long)(t1 - t2)));
    // Separable Chebyshev min filter of f (blocked tiles are unreached, so INF): rows, then columns.
    std::vector<std::uint16_t> row(n, INF), pre, suf;
    out.land.assign(n, INF);
    for (int y = 0; y < g.h; ++y) line_min(&out.f[y * g.w], &row[y * g.w], g.w, 1, pre, suf);
    for (int x = 0; x < g.w; ++x) line_min(&row[x], &out.land[x], g.h, g.w, pre, suf);
    auto t3 = bc::fuel_remaining();
    say(format("TPSTAGE filter %llu", (unsigned long long)(t2 - t3)));
    out.one = out.f;
    std::vector<int> launches;
    for (int i = 0; i < n; ++i) {
        if (g.block[i] || out.land[i] == INF) continue;
        const int s = out.land[i] + CHANNEL;
        if (s < out.one[i]) {
            out.one[i] = static_cast<std::uint16_t>(s);
            launches.push_back(i);
        }
    }
    auto t4 = bc::fuel_remaining();
    spread(g, launches, out.one);
    say(format("TPSTAGE seeds %llu (%zu launches) spread %llu", (unsigned long long)(t3 - t4), launches.size(), (unsigned long long)(t4 - bc::fuel_remaining())));
}

}  // namespace oldc

namespace {

// The launch-now policy: walk down f; launch the first time a crystal from here beats walking by `save` turns.
int greedy_trip(const oldc::Grid& g, const oldc::Fields& fs, int start, int save) {
    int p = start, steps = 0;
    while (fs.f[p] != 0 && fs.f[p] != oldc::INF) {
        if (fs.land[p] != oldc::INF && static_cast<int>(fs.f[p]) - oldc::CHANNEL - static_cast<int>(fs.land[p]) >= save)
            return steps + oldc::CHANNEL + fs.land[p];
        const int x = p % g.w, y = p / g.w;
        int next = p;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const int xx = x + dx, yy = y + dy;
                if (xx < 0 || yy < 0 || xx >= g.w || yy >= g.h) continue;
                const int j = yy * g.w + xx;
                if (fs.f[j] < fs.f[next]) next = j;
            }
        if (next == p) break;
        p = next;
        ++steps;
    }
    return steps;
}

}  // namespace

void tp_probe_old() {
    const auto start = bc::fuel_remaining();
    const auto& g = oldc::grid();
    const auto after_grid = bc::fuel_remaining();
    bc::bitboard ring;
    for (const auto a : bc::map::altars().positions()) ring.draw_chebyshev(a, 1.0f);
    oldc::Fields alt, home;
    if (bc::fuel_remaining() < 3000000) { say("TPSKIP low fuel"); return; }
    oldc::compute(ring, alt);
    const auto after_alt = bc::fuel_remaining();
    say(format("TPFUEL1 altar-fields %llu", static_cast<unsigned long long>(after_grid - bc::fuel_remaining())));
    if (bc::fuel_remaining() < 3000000) { say("TPSKIP low fuel 2"); return; }
    oldc::compute(bc::map::spawn_region(Team::ally), home);
    const auto after_home = bc::fuel_remaining();
    // Check our BFS against the engine's walking field: seeds at the goals only must give f back.
    {
        std::vector<std::uint16_t> seed(alt.f.size(), oldc::INF), check;
        for (std::size_t i = 0; i < alt.f.size(); ++i)
            if (alt.f[i] == 0) seed[i] = 0;
        int worse = 0, helped = 0;
        for (std::size_t i = 0; i < alt.f.size(); ++i) {
            if (alt.one[i] > alt.f[i]) ++worse;
            if (alt.one[i] < alt.f[i]) ++helped;
        }
        say(format("TPCHK one>f tiles %d, helped tiles %d of %zu", worse, helped, alt.f.size()));
    }
    auto idx = [&](vec2 t) { return t.y * g.w + t.x; };
    {
        int mism = 0;
        for (int y = 0; y < g.h; ++y) for (int x = 0; x < g.w; ++x) if ((g.block[y * g.w + x] != 0) != seen::fixed_blocks().is_set({x, y})) ++mism;
        bc::distance_field fld; bc::bitboard rch; fld.compute(seen::fixed_blocks(), ring & ~seen::fixed_blocks(), rch);
        int fm = 0; for (int y = 0; y < g.h; ++y) for (int x = 0; x < g.w; ++x) if (fld.get({x, y}) != alt.f[y * g.w + x]) ++fm;
        say(format("OLDCHK block layout mismatches %d, field layout mismatches %d", mism, fm));
        const auto& d2 = draft::group_data();
        for (std::size_t i = 0; i < d2.groups.size() && i < 12; ++i) { if (d2.spawn_lists[i].empty()) continue; auto t = d2.spawn_lists[i][0];
            say(format("OLDGRP %zu (%d,%d) alt walk %d one %d", i, t.x, t.y, (int)alt.f[idx(t)], (int)alt.one[idx(t)])); }
    }
    // Spawn to the nearest altar.
    int sw = oldc::INF, so = oldc::INF;
    for (const auto t : bc::map::spawn_region(Team::ally).positions()) {
        sw = std::min<int>(sw, alt.f[idx(t)]);
        so = std::min<int>(so, alt.one[idx(t)]);
    }
    // Groups: to the nearest altar (from the group's best tile) and from the spawn, gem-weighted.
    const auto& data = draft::group_data();
    double gw = 0, a_walk = 0, a_one = 0, a_g3 = 0, a_g5 = 0, s_walk = 0, s_one = 0;
    double a_save4 = 0, s_save4 = 0, a_pocket = 0, s_pocket = 0;
    int best_a = 0, best_s = 0;
    for (std::size_t i = 0; i < data.groups.size(); ++i) {
        const double gems = data.groups[i].gem_value();
        if (gems <= 0 || data.spawn_lists[i].empty()) continue;
        int aw = oldc::INF, ao = oldc::INF, at_tile = -1, hw = oldc::INF, ho = oldc::INF;
        for (const auto t : data.spawn_lists[i]) {
            const int k = idx(t);
            if (alt.one[k] < ao || (alt.one[k] == ao && alt.f[k] < aw)) at_tile = k;
            aw = std::min<int>(aw, alt.f[k]);
            ao = std::min<int>(ao, alt.one[k]);
            hw = std::min<int>(hw, home.f[k]);
            ho = std::min<int>(ho, home.one[k]);
        }
        if (ao == oldc::INF || ho == oldc::INF) continue;
        gw += gems;
        if (aw == oldc::INF) a_pocket += gems;
        if (hw == oldc::INF) s_pocket += gems;
        const int awc = std::min(aw, 200), hwc = std::min(hw, 200);
        a_walk += gems * awc;
        a_one += gems * ao;
        s_walk += gems * hwc;
        s_one += gems * ho;
        if (awc - ao >= 4) a_save4 += gems;
        if (hwc - ho >= 4) s_save4 += gems;
        best_a = std::max(best_a, awc - ao);
        best_s = std::max(best_s, hwc - ho);
        if (at_tile >= 0 && aw != oldc::INF) {
            a_g3 += gems * greedy_trip(g, alt, at_tile, 3);
            a_g5 += gems * greedy_trip(g, alt, at_tile, 5);
        } else {
            a_g3 += gems * awc;
            a_g5 += gems * awc;
        }
    }
    const auto after_stats = bc::fuel_remaining();
    if (gw <= 0) gw = 1;
    say(format("TPSTAT map %dx%d groups %zu altars %zu | spawn->altar walk %d one %d | group->altar walk %.1f one %.1f greedy3 %.1f greedy5 %.1f save4 %.0f%% pocket %.0f%% best %d | spawn->group walk %.1f one %.1f save4 %.0f%% pocket %.0f%% best %d",
               g.w, g.h, data.groups.size(), bc::map::altars().positions().size(), sw, so, a_walk / gw, a_one / gw,
               a_g3 / gw, a_g5 / gw, 100 * a_save4 / gw, 100 * a_pocket / gw, best_a, s_walk / gw, s_one / gw,
               100 * s_save4 / gw, 100 * s_pocket / gw, best_s));
    say(format("TPFUEL grid %llu altar-fields %llu spawn-fields %llu stats %llu left %llu",
               static_cast<unsigned long long>(start - after_grid), static_cast<unsigned long long>(after_grid - after_alt),
               static_cast<unsigned long long>(after_alt - after_home), static_cast<unsigned long long>(after_home - after_stats),
               static_cast<unsigned long long>(after_stats)));
}
