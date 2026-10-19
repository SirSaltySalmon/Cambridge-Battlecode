// Shard and emblem purchases.
#include "bot.h"
#include "state.h"
#include <map>
#include <set>

int Bot::vesper_recall_gold(const bc::champion& me) const {
    // Recall once the purse covers what is left of this batch, or the rest of the build if smaller.
    const int owned = emblem_count(me);
    if (owned >= VESPER_CORE_EMBLEMS) return VESPER_RECALL_GOLD;
    bc::enum_map<Item> counts;
    try {
        counts = me.item_count();
    } catch (const bc::error&) {
        return VESPER_RECALL_GOLD;
    }
    std::set<int> build;
    for (const auto shard : shop_order(Champ::vesper_vale)) build.insert(static_cast<int>(shard));
    for (const auto shard : shop_fallback(Champ::vesper_vale)) build.insert(static_cast<int>(shard));
    int loose = 0;
    for (const int shard : build) {
        const Item item{static_cast<std::uint32_t>(shard)};
        loose += static_cast<int>(counts[item] * item.cost());
    }
    const int left = EMBLEM_WORTH * (VESPER_CORE_EMBLEMS - owned) - loose;
    return std::max(static_cast<int>(Item(Item::power_shard_of_strength).cost()),
                    std::min(VESPER_RECALL_GOLD - batch_spent, left));
}

void Bot::buy_items(const bc::champion& me) {
    const auto kind = me.champion_name();
    const int before = static_cast<int>(me.gold_count());
    shop(me, kind);
    if (kind == Champ::vesper_vale) {
        const int total = batch_spent + before - static_cast<int>(me.gold_count());
        batch_spent = ((total % VESPER_RECALL_GOLD) + VESPER_RECALL_GOLD) % VESPER_RECALL_GOLD;
    }
}

void Bot::shop(const bc::champion& me, Champ kind) {
    // A family listed twice wants a second emblem, so its shards are bought again once the first is done.
    std::map<int, int> wanted;
    std::optional<bc::enum_map<Item>> counts;
    for (const auto shard : shop_order(kind)) {
        if (!counts) {
            try {
                counts = me.item_count();
            } catch (const bc::error&) {
                return;
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
        if (buy(me, shard, std::min(need, afford))) counts.reset();
        if (afford < need) return;
    }
    const auto fallback = shop_fallback(kind);
    while (!fallback.empty()) {
        bool bought_any = false;
        for (const auto shard : fallback) {
            bc::enum_map<Item> now;
            try {
                now = me.item_count();
            } catch (const bc::error&) {
                return;
            }
            const int need = std::max(0, static_cast<int>(shard.combine_count()) - static_cast<int>(now[shard]));
            if (need == 0) continue;
            const int afford = static_cast<int>(me.gold_count() / shard.cost());
            if (buy(me, shard, std::min(need, afford))) bought_any = true;
            if (afford < need) return;
        }
        if (!bought_any) return;
    }
}
