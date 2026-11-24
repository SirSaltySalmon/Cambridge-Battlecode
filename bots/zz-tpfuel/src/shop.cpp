// Shard and emblem purchases.
#include "bot.h"
#include "crystal.h"
#include "state.h"
#include "seen.h"
#include "trace.h"
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

std::span<const Item> Bot::build_of(Champ kind) const {
    // v134 (PROSPECTOR_SWAP): Prospector only pays on cluster rewards, so with few cluster camps its shards become
    // Treasure (Power when the build already has Treasure), same stat. Items come in families of six: family, then stat.
    if (prism_team()) return prism_team_order(kind);  // v158
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
    // v150 (LOCK_BUY): on a one-altar map the lock comes before the build; elsewhere (2) from what the build leaves.
    if (LOCK_BUY >= 1 && single_altar) buy_lock(me);
    // v215 (TP): a crystal on maps where it pays, before the build (TP_FIRST) or from what it leaves.
    if (TP && TP_FIRST && crystal::map_on()) buy_crystal(me);
    // v134: a purchase refreshes our own emblems and families (owned_of) at once.
    if (shop(me, kind)) emblem_count(me);
    if (TP && !TP_FIRST && crystal::map_on()) buy_crystal(me);
    if (LOCK_BUY >= 2 && !single_altar && static_cast<int>(me.gold_count()) < EMBLEM_WORTH / 5) buy_lock(me);
}

void Bot::buy_lock(const bc::champion& me) {
    // v150 (LOCK_BUY): one lock at a time; the inventory is read once per shop visit until we hold one.
    if (carry_lock) return;
    const auto type = bc::consumable_type::altar_lock;
    try {
        if (me.consumables()[type].count() > 0) {
            carry_lock = true;
            return;
        }
        if (!me.can_buy(type)) return;
        me.buy(type);
        carry_lock = true;
        NOTE("bought an altar lock");
    } catch (const bc::error& error) {
        NOTE("altar lock purchase refused: " + error_text(error));
    }
}

void Bot::buy_crystal(const bc::champion& me) {
    // v215 (TP): one crystal at a time; the inventory is read on shop visits until we hold one.
    if (crystal::carrying) return;
    const auto type = bc::consumable_type::tp_crystal;
    try {
        if (me.consumables()[type].count() > 0) {
            crystal::carrying = true;
            return;
        }
        if (!me.can_buy(type)) return;
        me.buy(type);
        crystal::carrying = true;
        NOTE("bought a teleport crystal");
    } catch (const bc::error& error) {
        NOTE("teleport crystal purchase refused: " + error_text(error));
    }
}

bool Bot::place_lock(const bc::champion& me) {
    // v150 (LOCK_PLACE): an unlocked altar within LOCK_RANGE gets our lock. It costs no action, cancels enemy tributes
    // there, and our own team still tributes through it; the enemy needs five hits to clear it.
    if (!LOCK_PLACE || !carry_lock) return false;
    const auto origin = me.position();
    for (const auto altar : seen::altar_list()) {
        if (distance(origin, altar) > LOCK_RANGE) continue;
        try {
            if (bc::vision::has_altar_lock(altar)) continue;
            if (me.consumables()[bc::consumable_type::altar_lock].use(altar)) {
                carry_lock = false;
                NOTE("altar lock placed at " + at(altar));
                return true;
            }
        } catch (const bc::error& error) {
            NOTE("altar lock at " + at(altar) + " refused: " + error_text(error));
            if (error.code() == bc::error_code::invalid_action_use_consumable_not_owned) carry_lock = false;
        }
    }
    return false;
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
    const auto fallback = prism_team() && kind == Champ::prism ? std::span<const Item>(PRISM_TEAM_PRISM_FALLBACK) : shop_fallback(kind);
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
