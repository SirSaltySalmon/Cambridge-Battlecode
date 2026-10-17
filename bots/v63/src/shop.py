"""Shard and emblem purchases."""
import cambc

from actions import buy
from constants import (
    Champ,
    EMBLEM_WORTH,
    Item,
    SHOP_FALLBACK,
    SHOP_ORDER,
    VESPER_CORE_EMBLEMS,
    VESPER_RECALL_GOLD,
)
from state import emblem_count


class Shop:
    # Shopping

    def vesper_recall_gold(self, me):
        # Recall once the purse covers what is left of this batch, or the rest of the build if smaller.
        if emblem_count(me) >= VESPER_CORE_EMBLEMS:
            return VESPER_RECALL_GOLD
        try:
            counts = me.item_count()
        except cambc.Error:
            return VESPER_RECALL_GOLD
        build = set(SHOP_ORDER[Champ.VESPER_VALE]) | set(SHOP_FALLBACK[Champ.VESPER_VALE])
        loose = sum(counts[shard] * shard.cost() for shard in build)
        left = EMBLEM_WORTH * (VESPER_CORE_EMBLEMS - emblem_count(me)) - loose
        return max(Item.POWER_SHARD_OF_STRENGTH.cost(), min(VESPER_RECALL_GOLD - self.batch_spent, left))

    def buy_items(self, me):
        kind = me.champion_name()
        before = me.gold_count()
        self.shop(me, kind)
        if kind == Champ.VESPER_VALE:
            self.batch_spent = (self.batch_spent + before - me.gold_count()) % VESPER_RECALL_GOLD

    def shop(self, me, kind):
        # A family listed twice wants a second emblem, so its shards are bought again once the first is done.
        wanted = {}
        counts = None
        for shard in SHOP_ORDER.get(kind, ()):
            if counts is None:
                try:
                    counts = me.item_count()
                except cambc.Error:
                    return
            emblem = shard.result()
            if emblem is not None:
                wanted[emblem] = wanted.get(emblem, 0) + 1
                if counts[emblem] >= wanted[emblem]:
                    continue
            need = max(0, shard.combine_count() - counts[shard])
            afford = me.gold_count() // shard.cost()
            if buy(me, shard, min(need, afford)):
                counts = None
            if afford < need:
                return
        fallback = SHOP_FALLBACK.get(kind, ())
        while fallback:
            bought_any = False
            for shard in fallback:
                try:
                    counts = me.item_count()
                except cambc.Error:
                    return

                need = max(0, shard.combine_count() - counts[shard])
                if need == 0:
                    continue
                afford = me.gold_count() // shard.cost()
                if buy(me, shard, min(need, afford)):
                    bought_any = True
                if afford < need:
                    return
            if not bought_any:
                return      
