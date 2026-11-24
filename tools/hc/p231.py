"""v231 = bots/v230 + RALLY and WILLOW_SHOOT (run once from the repo root, after copying bots/v230 to bots/v231).
Reasons and benches: bots/v231/README.md."""
import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch

B = 'bots/v231/src/'

# ---------------------------------------------------------------- constants.h
p = Patch(B + 'constants.h')
p.before("// ---- v230: survival and cluster-map fixes (bench: v230 README). ----", """// ---- v231: fight coordination (fight plan F2 / F5, rebuilt small; bench: v231 README). ----
// RALLY: a champion an enemy champion is hurting calls the team to the fight (the ping) whenever the enemies within
//   RALLY_FOE_RANGE tiles are at least as many as the allies within RALLY_MATE_RANGE (itself counted), in any mode
//   and with allies beside it. Off: only alone (nobody within PING_ALONE) and only outside fight mode.
inline constexpr bool RALLY = true;
inline constexpr int RALLY_FOE_RANGE = 6, RALLY_MATE_RANGE = 4;
// WILLOW_SHOOT: over WILLOW_SHOOT_HEALTH (share of health) Willow basic-attacks champions from outside their basic
//   reach (melee 2 tiles, ranged 3; awake and sleeping camps' reach too) and steps to such a tile within HEAL_REACH
//   of the front ally. Off: she only heals and stands THREAT_RANGE from every enemy, so she never fires.
inline constexpr bool WILLOW_SHOOT = true;
inline constexpr double WILLOW_SHOOT_HEALTH = 0.6;

""")
p.save()

# ---------------------------------------------------------------- bot.h
p = Patch(B + 'bot.h')
p.rep("    void willow_backline(const bc::champion& me, const bc::bitboard* avoid);",
      "    void willow_backline(const bc::champion& me, const bc::bitboard* avoid, const bc::champion* pick = nullptr);\n"
      "    bool willow_shoot(const bc::champion& me, const bc::champion* pick, std::optional<vec2> front, bool healed,\n"
      "                      const bc::bitboard* avoid);")
p.save()

# ---------------------------------------------------------------- kits.cpp
p = Patch(B + 'kits.cpp')
p.rep("""        willow_backline(me, avoid);
        return;""", """        willow_backline(me, avoid, target.champion());
        return;""")
p.rep("""void Bot::willow_backline(const bc::champion& me, const bc::bitboard* avoid) {
    heal_lowest_ally(me);
    const auto& foes = seen::champions(Team::enemy);""", """bool Bot::willow_shoot(const bc::champion& me, const bc::champion* pick, std::optional<vec2> front, bool healed,
                       const bc::bitboard* avoid) {
    // v231 (WILLOW_SHOOT): fire from outside every enemy's basic reach and every camp's; else step to such a tile in
    // range of the target and in heal range of the front. One reach board, read once.
    if (me.health() < me.max_health() * WILLOW_SHOOT_HEALTH) return false;
    const auto& foes = seen::champions(Team::enemy);
    auto reach_zone = enemy_reach(foes);
    reach_zone |= threat_zone({}, true);
    const vec2 position = seen::self_position();
    const int range = me.attack_range();
    if (!reach_zone.is_set(position) && me.can_attack()) {
        std::optional<bc::champion> target;
        if (pick && sighted(*pick) && alive(*pick) && within(me, *pick, range)) target = *pick;
        if (!target) {
            std::vector<bc::champion> in_range;
            for (const auto& e : foes)
                if (within(me, e, range)) in_range.push_back(e);
            target = lowest(in_range);
        }
        if (target && attack_if_in_range(*target)) {
            SUB(".shoot", "fires from outside enemy reach");
            if (!healed && alive(*target) && cast_self(ability_of(me, Kind::attack_reset))) {
                SUB("", "no ally to heal: reset for a second hit");
                attack_if_in_range(*target);
            }
            return true;
        }
    }
    if (!pick || !front || !sighted(*pick) || !alive(*pick)) return false;
    auto spots = tiles_around(pick->position(), static_cast<float>(range));
    spots &= ~reach_zone;
    spots &= tiles_around(*front, static_cast<float>(HEAL_REACH));
    if (spots.is_empty() || spots.is_set(position)) return false;
    if (avoid) reach_zone |= *avoid;
    if (!move_towards_closest(spots, &reach_zone)) return false;
    SUB(".close", "steps to a tile in range of the target, outside enemy reach, in heal range of the front");
    return true;
}

void Bot::willow_backline(const bc::champion& me, const bc::bitboard* avoid, const bc::champion* pick) {
    const bool healed = heal_lowest_ally(me);
    const auto& foes = seen::champions(Team::enemy);""")
p.rep("""    bc::bitboard goal;
    const char* where = "out of reach";
    if (front) {
        goal = tiles_around(*front, static_cast<float>(HEAL_REACH)) & ~zone;""", """    if (WILLOW_SHOOT && willow_shoot(me, pick, front, healed, avoid)) return;
    bc::bitboard goal;
    const char* where = "out of reach";
    if (front) {
        goal = tiles_around(*front, static_cast<float>(HEAL_REACH)) & ~zone;""")
p.save()

# ---------------------------------------------------------------- combat.cpp
p = Patch(B + 'combat.cpp')
p.rep("""    if (PING_FILTER && mode == FIGHT) return;
    const int lost = gather ? 0 : last_health - static_cast<int>(me.health());""", """    if (PING_FILTER && !RALLY && mode == FIGHT) return;
    const int lost = gather ? 0 : last_health - static_cast<int>(me.health());""")
p.rep("""    // v78 (PING_FILTER): an ally beside us is already in this fight; the ping would add nothing.
    if (PING_FILTER) {""", """    // v231 (RALLY): the call goes out unless we outnumber them here (tiles from the vision lists; no engine reads).
    int rally_foes = 0, rally_mates = 0;
    if (RALLY && !gather) {
        for (const auto tile : seen::positions(Team::enemy))
            if (distance(position, tile) <= RALLY_FOE_RANGE) ++rally_foes;
        for (const auto tile : seen::positions(Team::ally))
            if (distance(position, tile) <= RALLY_MATE_RANGE) ++rally_mates;
        if (rally_mates > rally_foes) return;
    }
    // v78 (PING_FILTER): an ally beside us is already in this fight; the ping would add nothing.
    if (PING_FILTER && !(RALLY && !gather)) {""")
p.rep("""    NOTE(format(gather ? "ping s%d: enemy champion in sight, nobody beside us" : "ping s%d: an enemy champion is hurting us, nobody beside us", best));""",
      """    LOG({
        if (RALLY && !gather)
            note(format("ping s%d: an enemy champion is hurting us, %d of us against %d here: rally", best, rally_mates, rally_foes));
        else
            note(format(gather ? "ping s%d: enemy champion in sight, nobody beside us" : "ping s%d: an enemy champion is hurting us, nobody beside us", best));
    });""")
p.save()
print('v231 patched')
