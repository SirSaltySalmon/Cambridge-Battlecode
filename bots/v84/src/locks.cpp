// v83 altar locks (LOCK_*): buy one with the gold left after shopping, place it on an altar an enemy tributes at
// (that cancels the channel) or one an enemy is near, and clear an enemy lock on the altar we bank at.
#include "bot.h"
#include "seen.h"
#include "state.h"
#include "trace.h"

namespace {
constexpr int LOCK_RANGE = 2;  // GameRules: placed on an altar within two tiles.
const bc::consumable_type LOCK = bc::consumable_type::altar_lock;
}  // namespace

void Bot::buy_lock(const bc::champion& me) {
    // After the build: a lock costs 10 of the gold the shards leave over.
    if (!LOCK_BUY) return;
    try {
        if (me.consumables()[LOCK].count() > 0) {
            lock_carried = true;
            return;
        }
        if (static_cast<int>(me.gold_count()) < LOCK_MIN_GOLD || !me.can_buy(LOCK)) return;
        me.buy(LOCK);
        lock_carried = true;
        trace::act("buy");
        NOTE("bought an altar lock");
    } catch (const bc::error&) {
    }
}

bool Bot::place_lock(const bc::champion& me) {
    // Costs no move, attack or cast. Cheap checks first: a lock in the bag, an altar within reach, an enemy in sight.
    if (!lock_carried || !LOCK_PLACE) return false;
    const auto position = me.position();
    if (LOCK_ANY)
        for (const auto altar : seen::altar_list())
            if (distance(position, altar) <= LOCK_RANGE && lock_altar(me, altar, "sparring: lock every altar")) return true;
    const auto& enemies = seen::champions(Team::enemy);
    if (enemies.empty()) return false;
    std::optional<vec2> pick;
    bool cancels = false;
    std::optional<bc::bitboard> locked;
    for (const auto altar : seen::altar_list()) {
        if (distance(position, altar) > LOCK_RANGE) continue;
        bool tributing = false, near = false;
        for (const auto& e : enemies) {
            const int d = distance(e.position(), altar);
            if (d <= bc::constants::submit_gems_range && e.is_submitting_gems()) tributing = true;
            if (d <= LOCK_DENY_RANGE && gems_carried(e) >= LOCK_DENY_GEMS) near = true;
        }
        if (!tributing && !(LOCK_DENY && near)) continue;
        if (!locked) locked = bc::vision::altar_lock_tiles();
        if (locked->is_set(altar)) continue;
        if (!pick || (tributing && !cancels)) {
            pick = altar;
            cancels = tributing;
        }
    }
    if (!pick) return false;
    try {
        if (!me.consumables()[LOCK].use(*pick)) return false;
    } catch (const bc::error& error) {
        NOTE(std::string("lock rejected: ") + error_text(error));
        lock_carried = false;
        return false;
    }
    lock_carried = false;
    trace::act("lock");
    NOTE(std::string(cancels ? "lock cancels an enemy tribute at " : "lock denies altar ") + at(*pick));
    say(format("round %u: champion %d locks altar (%d,%d)%s", bc::round_number(), champ_id(me.champion_name()), pick->x,
               pick->y, cancels ? ", enemy tribute cancelled" : ""));
    return true;
}

bool Bot::lock_altar(const bc::champion& me, vec2 altar, const char* why) {
    if (!lock_carried || distance(me.position(), altar) > LOCK_RANGE) return false;
    if (bc::vision::altar_lock_tiles().is_set(altar)) return false;
    try {
        if (!me.consumables()[LOCK].use(altar)) return false;
    } catch (const bc::error& error) {
        NOTE(std::string("lock rejected: ") + error_text(error));
        lock_carried = false;
        return false;
    }
    lock_carried = false;
    trace::act("lock");
    NOTE(std::string(why) + ": lock on " + at(altar));
    say(format("round %u: champion %d locks altar (%d,%d): %s", bc::round_number(), champ_id(me.champion_name()), altar.x,
               altar.y, why));
    return true;
}

bool Bot::break_lock(const bc::champion& me, vec2 altar) {
    // An enemy lock on the altar we bank at: hit it (3 basic attacks clear it), walking into attack range first.
    if (!LOCK_BREAK) return false;
    const auto enemy_locks = bc::vision::altar_lock_tiles(Team::enemy);
    if (!enemy_locks.is_set(altar)) return false;
    const auto position = me.position();
    if (distance(position, altar) <= me.attack_range()) {
        try {
            const auto lock = bc::vision::altar_lock_at(altar);
            me.attack(lock);
            trace::act("attack");
            BRANCH("bank.unlock", format("enemy lock on altar (%d,%d), %u hits left: hit it", altar.x, altar.y,
                                         lock.hits_remaining()));
            return true;
        } catch (const bc::error& error) {
            NOTE(std::string("lock attack rejected: ") + error_text(error));
        }
    }
    BRANCH("bank.unlock.walk", format("enemy lock on altar (%d,%d): walk into attack range", altar.x, altar.y));
    move_towards_closest(tiles_around(altar, static_cast<float>(std::min(me.attack_range(), bc::constants::submit_gems_range))));
    return true;
}
