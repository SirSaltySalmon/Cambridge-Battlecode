// Shard and emblem purchases.
#include "bot.h"
#include "state.h"
#include <map>
#include <set>

int Bot::vesper_recall_gold(const bc::champion& me) const {
    // v94: while building, recall once the purse covers the next step: the Treasure emblem first (the shop has bought
    // only its shards so far), then the rest of the core in one trip. After the core, at VESPER_RECALL_GOLD.
    const int owned = emblem_count(me);
    if (owned >= VESPER_CORE_EMBLEMS) return VESPER_EMBLEM_RECALL ? RECALL_GOLD : VESPER_RECALL_GOLD;
    bc::enum_map<Item> counts;
    try {
        counts = me.item_count();
    } catch (const bc::error&) {
        return VESPER_RECALL_GOLD;
    }
    std::set<int> build;
    for (const auto shard : build_of(Champ::vesper_vale)) build.insert(static_cast<int>(shard));
    for (const auto shard : shop_fallback(Champ::vesper_vale)) build.insert(static_cast<int>(shard));
    int loose = 0;
    for (const int shard : build) {
        const Item item{static_cast<std::uint32_t>(shard)};
        loose += static_cast<int>(counts[item] * item.cost());
    }
    const int left = EMBLEM_WORTH * (VESPER_CORE_EMBLEMS - owned) - loose;
    const int step = owned == 0 ? EMBLEM_WORTH - loose : left;
    return std::max(static_cast<int>(Item(Item::power_shard_of_strength).cost()), step);
}

int Bot::core_gold_left(const bc::champion& me, Champ kind, int core) const {
    // v148 (VARG_PLAN): what the rest of a `core`-emblem build costs, less the shards already bought toward it.
    const int owned = emblem_count(me);
    if (owned >= core) return 0;
    bc::enum_map<Item> counts;
    try {
        counts = me.item_count();
    } catch (const bc::error&) {
        return EMBLEM_WORTH * (core - owned);
    }
    std::set<int> build;
    for (const auto shard : build_of(kind)) build.insert(static_cast<int>(shard));
    int loose = 0;
    for (const int shard : build) {
        const Item item{static_cast<std::uint32_t>(shard)};
        loose += static_cast<int>(counts[item] * item.cost());
    }
    return std::max(static_cast<int>(Item(Item::power_shard_of_strength).cost()), EMBLEM_WORTH * (core - owned) - loose);
}

std::span<const Item> Bot::fallback_of(Champ kind) const {
    if (varg_plan && kind == Champ::varg) return VARG_PLAN_FALLBACK;
    return shop_fallback(kind);
}

std::span<const Item> Bot::build_of(Champ kind) const {
    // v148 (VARG_PLAN): Varg's plan build, by team size (no Prospector in it).
    if (varg_plan && kind == Champ::varg)
        return team_size >= 4 ? std::span<const Item>(VARG_PLAN_BUILD_45) : std::span<const Item>(VARG_PLAN_BUILD_3);
    // v134 (PROSPECTOR_SWAP): Prospector only pays on cluster rewards, so with few cluster camps its shards become
    // Treasure (Power when the build already has Treasure), same stat. Items come in families of six: family, then stat.
    const auto order = shop_order(kind);
    if (!PROSPECTOR_SWAP || route.empty() || cluster_camps >= PROSPECTOR_MIN_CLUSTERS) return order;
    auto family = [](Item item) { return static_cast<bc::item_family>(static_cast<int>(item) / 6); };
    bool prospector = false, treasure = false;
    for (const auto shard : order) {
        if (family(shard) == bc::item_family::prospector) prospector = true;
        if (family(shard) == bc::item_family::treasure) treasure = true;
    }
    if (!prospector) return order;
    swapped_build.assign(order.begin(), order.end());
    const auto into = treasure ? bc::item_family::power : bc::item_family::treasure;
    for (auto& shard : swapped_build)
        if (family(shard) == bc::item_family::prospector)
            shard = bc::get_item_enum(into, static_cast<bc::item_stat>(static_cast<int>(shard) % 6 / 2));
    if (!swap_logged) {
        swap_logged = true;
        LOG(say(format("build: %d cluster camps (< %d), prospector -> %s", cluster_camps, PROSPECTOR_MIN_CLUSTERS,
                       treasure ? "power" : "treasure")););
    }
    return swapped_build;
}

void Bot::buy_items(const bc::champion& me) {
    const auto kind = me.champion_name();
    // v134: a purchase refreshes our own emblems and families (owned_of) at once.
    if (shop(me, kind)) emblem_count(me);
}

bool Bot::shop(const bc::champion& me, Champ kind) {
    // A family listed twice wants a second emblem, so its shards are bought again once the first is done.
    std::map<int, int> wanted;
    std::optional<bc::enum_map<Item>> counts;
    bool bought = false;
    for (const auto shard : build_of(kind)) {
        if (!counts) {
            try {
                counts = me.item_count();
            } catch (const bc::error&) {
                return bought;
            }
        }
        const auto emblem = shard.result();
        if (emblem) {
            const int key = static_cast<int>(*emblem);
            ++wanted[key];
            if (static_cast<int>((*counts)[*emblem]) >= wanted[key]) continue;
        }
        const int need = std::max(0, static_cast<int>(shard.combine_count()) - static_cast<int>((*counts)[shard]));
        const int afford = static_cast<int>(me.gold_count() / shard.cost());
        if (buy(me, shard, std::min(need, afford))) {
            counts.reset();
            bought = true;
        }
        if (afford < need) return bought;
    }
    const auto fallback = fallback_of(kind);
    while (!fallback.empty()) {
        bool bought_any = false;
        for (const auto shard : fallback) {
            bc::enum_map<Item> now;
            try {
                now = me.item_count();
            } catch (const bc::error&) {
                return bought;
            }
            const int need = std::max(0, static_cast<int>(shard.combine_count()) - static_cast<int>(now[shard]));
            if (need == 0) continue;
            const int afford = static_cast<int>(me.gold_count() / shard.cost());
            if (buy(me, shard, std::min(need, afford))) bought_any = bought = true;
            if (afford < need) return bought;
        }
        if (!bought_any) return bought;
    }
    return bought;
}
