"""vendetta_buyer.py - Maximally optimized purchasing routine for Vendetta."""
import cambc as bc


# Cached static references (initialized once)
_SHARDS = None  # (monster_shard, power_shard, durability_shard)
_EMBLEMS = None  # (monster_emblem, power_emblem, durability_emblem)


def _init_catalog():
    global _SHARDS, _EMBLEMS
    m_shard = p_shard = d_shard = None
    m_emb = p_emb = d_emb = None

    for i in range(64):
        try:
            item = bc.ItemType(i)
            if not item.is_purchasable():
                continue
            res = item.result()
            if res is None:
                continue

            name = item.name.lower()
            if item.strength_points() > 0 and "monster" in name:
                m_shard, m_emb = item, res
            elif item.strength_points() > 0 and "power" in name:
                p_shard, p_emb = item, res
            elif item.health_points() > 0 and "durability" in name:
                d_shard, d_emb = item, res

            if m_shard and p_shard and d_shard:
                break
        except Exception:
            continue

    _SHARDS = (m_shard, p_shard, d_shard)
    _EMBLEMS = (m_emb, p_emb, d_emb)


def vendetta_buy_items():
    """Runs in < 50 fuel when not shopping, and 1-2 engine calls when shopping."""
    me = bc.me()

    # Fast bailouts: only Vendetta buys, and only if we can afford a shard (30g)
    if me.gold_count() < 30 or "vendetta" not in str(me.champion_name()).lower():
        return

    # Check legality immediately without recomputing bitboards
    if not me.can_buy(bc.ConsumableType(0)) and not (not me.is_alive() or bc.game_map.spawn_region(me.team()).is_set(me.position())):
        return

    global _SHARDS
    if _SHARDS is None:
        _init_catalog()

    m_shard, p_shard, d_shard = _SHARDS
    m_emb, p_emb, d_emb = _EMBLEMS
    if not m_shard:
        return

    # Fetch inventory counts once
    counts = me.item_count()
    gold = me.gold_count()

    def get_count(item):
        idx = int(item)
        return counts[idx] if idx < len(counts) else 0

    m_emb_count = get_count(m_emb)
    p_emb_count = get_count(p_emb)
    d_emb_count = get_count(d_emb)

    # 1. First priority: 1 Monster Strength Emblem (needs 5 shards total)
    if m_emb_count < 1:
        current_shards = get_count(m_shard)
        needed = 5 - current_shards
        buy_amount = min(needed, gold // 30)
        if buy_amount > 0 and me.can_buy(m_shard, buy_amount):
            me.buy(m_shard, buy_amount)
            gold -= buy_amount * 30
            if gold < 30:
                return

    # 2. Cycle: 2 Power Emblems per 1 Durability Emblem
    # Target power emblems for the current durability tier
    target_power = 2 * (d_emb_count + 1)
    
    if p_emb_count < target_power:
        # Buy Power Strength shards
        needed_shards = (target_power - p_emb_count) * 5 - get_count(p_shard)
        buy_amount = min(max(1, needed_shards), gold // 30)
        if buy_amount > 0 and me.can_buy(p_shard, buy_amount):
            me.buy(p_shard, buy_amount)
    else:
        # Buy Durability Health shards (needs 5 shards to complete the next durability emblem)
        needed_shards = 5 - get_count(d_shard)
        buy_amount = min(max(1, needed_shards), gold // 30)
        if buy_amount > 0 and me.can_buy(d_shard, buy_amount):
            me.buy(d_shard, buy_amount)