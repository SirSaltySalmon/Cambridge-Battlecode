"""v230 = bots/v220 + the seven switches v228 keeps, without the experiments that lost (run once from the repo root,
after copying bots/v220 to bots/v230). Reasons and benches: bots/v230/README.md."""
import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch

B = 'bots/v230/src/'

# ---------------------------------------------------------------- constants.h
p = Patch(B + 'constants.h')
p.before("// ---- v202 (SPREAD): spread-by-default farming", """// ---- v230: survival and cluster-map fixes (bench: v230 README). ----
// TRIBUTE_STAY: an enemy in reach ends a tribute only under TRIBUTE_STAY_HEALTH (share of our health); damage never
//   breaks a tribute.
inline constexpr bool TRIBUTE_STAY = true;
inline constexpr double TRIBUTE_STAY_HEALTH = 0.4;
// DODGE_HIT: a melee champion stepping out of enemy reach under the recall bar hits the weakest enemy in reach first.
inline constexpr bool DODGE_HIT = true;
// ROAD_WALK: a living group with no camp in sight is walked to by its tiles not seen empty; a dead member's tile of a
//   living group holds nothing down.
inline constexpr bool ROAD_WALK = true;
// RESPAWN_CLEAR: a group seen respawning is never walked onto; while an ally stands on one of its tiles the rest wait
//   RESPAWN_WAY_RANGE tiles off it (WAIT_RANGE otherwise) so the ally can step off.
inline constexpr bool RESPAWN_CLEAR = true;
inline constexpr int RESPAWN_WAY_RANGE = 4;
// BAND_BANK_STICKY: rounds the hurt-band bank walk keeps the altar it set out for (0 off).
inline constexpr int BAND_BANK_STICKY = 12;
// PRISM_WANT_RANGE: on the Prism team a cleared gem reward is hers only while she is within this many tiles of the
//   group's nearest tile (was VESPER_CLAIM_RANGE, 8, from its centre); further off, whoever cleared it captures it.
inline constexpr int PRISM_WANT_RANGE = 1;

""")
p.rep("inline constexpr bool HOR_DIVE = true, HOR_SHIELD = true, HOR_HOLD = true, HOR_CAMP_DASH = true;",
      "inline constexpr bool HOR_DIVE = true, HOR_SHIELD = true, HOR_CAMP_DASH = true;\n"
      "// v230: HOR_HOLD off (he stayed in fights under the recall bar until one burst from dead; bench: v230 README).\n"
      "inline constexpr bool HOR_HOLD = false;")
p.save()

# ---------------------------------------------------------------- bot.h
p = Patch(B + 'bot.h')
p.after("    bool spread_step(const bc::champion& me, int now_mode);", """
    // v230 (ROAD_WALK, RESPAWN_CLEAR): by slot, the last round we saw the group alive / respawning.
    std::vector<int> alive_seen, respawn_seen;
    // v230 (BAND_BANK_STICKY): the hurt band's altar and the round it was chosen.
    std::optional<vec2> band_altar;
    int band_altar_round = -1000;""")
p.save()

# ---------------------------------------------------------------- route.cpp
p = Patch(B + 'route.cpp')
p.rep("""        if (FIX_ALTAR_GATE && state == 1) {
            if (seen_up.size() != route.size()) seen_up.assign(route.size(), -GATE_MEMORY - 1);
            seen_up[index] = now;
        }""", """        if (FIX_ALTAR_GATE && state == 1) {
            if (seen_up.size() != route.size()) seen_up.assign(route.size(), -GATE_MEMORY - 1);
            seen_up[index] = now;
        }
        if (ROAD_WALK && state == 1) {
            if (alive_seen.size() != route.size()) alive_seen.assign(route.size(), -1000);
            alive_seen[index] = now;
        }
        if (RESPAWN_CLEAR && state == 3) {
            if (respawn_seen.size() != route.size()) respawn_seen.assign(route.size(), -1000);
            respawn_seen[index] = now;
        }""")
p.rep("        if (ally.champion_name() == Champ::prism) return distance(ally.position(), centers[s]) <= VESPER_CLAIM_RANGE;",
      """        if (ally.champion_name() == Champ::prism) {
            // v230 (PRISM_WANT_RANGE): to the group's nearest tile (a 20-cluster group's centre is tiles from its edge).
            int near = distance(ally.position(), centers[s]);
            for (const auto t : spawn_lists[s]) near = std::min(near, distance(ally.position(), t));
            return near <= PRISM_WANT_RANGE;
        }""")
p.save()

# ---------------------------------------------------------------- combat.cpp (farm: the group is down or out of sight)
p = Patch(B + 'combat.cpp')
p.rep("""        if (ready_at[s] > now) {
            const int wait = ready_at[s] - now;""", """        // v230 (RESPAWN_CLEAR): the group is down and an ally stands on one of its tiles: give it room to step off.
        if (RESPAWN_CLEAR && !tiles.is_set(me.position()) && !rewards.count(s)
            && (ready_at[s] > now || (s < static_cast<int>(respawn_seen.size()) && respawn_seen[s] == now))) {
            auto held = tiles;
            held &= bc::vision::champion_tiles(Team::ally);
            if (!held.is_empty()) {
                bc::bitboard far, near;
                for (const auto tile : spawn_lists[s]) {
                    far.draw_chebyshev(tile, static_cast<float>(RESPAWN_WAY_RANGE + 1));
                    near.draw_chebyshev(tile, static_cast<float>(RESPAWN_WAY_RANGE - 1));
                }
                far &= ~near;
                WHY(stem + ".way", format("s%d ", s) + at(center) + " is down and an ally stands on its tile: wait further off, leave it room");
                if (!far.is_set(me.position()) && !move_towards_closest(far, avoid)) move_towards_closest(far);
                return;
            }
        }
        if (ready_at[s] > now) {
            const int wait = ready_at[s] - now;""")
p.rep("""        } else if (FIX_POCKET && tiles.is_set(me.position())) {
            // v103: due but not here: our own body on a spawn tile holds the group down (islands: two dashers stood on""",
      """        } else if (RESPAWN_CLEAR && s < static_cast<int>(respawn_seen.size()) && respawn_seen[s] == now) {
            // v230 (RESPAWN_CLEAR): due, and we see it still respawning: someone stands on a tile. Off them, all of us.
            WHY(stem + ".clear", format("s%d ", s) + at(center) + " is due but still respawning in our sight: keep off its tiles");
            bc::bitboard ring;
            for (const auto tile : spawn_lists[s]) ring.draw_chebyshev(tile, WAIT_RANGE);
            ring &= ~tiles;
            if (!ring.is_set(me.position()) && !move_towards_closest(ring, avoid)) move_towards_closest(ring);
        } else if (ROAD_WALK && s < static_cast<int>(alive_seen.size()) && alive_seen[s] >= now - 1) {
            // v230 (ROAD_WALK): alive, its living camps out of sight: toward the tiles we do not see empty.
            WHY(stem + ".walk", format("s%d ", s) + at(center) + tile_flags(center) + " is alive, its camps out of sight: walk to the unseen tiles");
            bc::vision::visible_tiles_into(ring_out);
            bc::vision::camp_tiles_into(ring_board);
            fast::not_into(ring_board, ring_board);
            ring_out &= ring_board;  // tiles in sight with no camp on them
            fast::not_into(ring_out, ring_out);
            ring_out &= tiles;
            if (ring_out.is_empty() || !move_towards_closest(ring_out, avoid)) move_towards_closest(tiles, avoid);
        } else if (FIX_POCKET && tiles.is_set(me.position())) {
            // v103: due but not here: our own body on a spawn tile holds the group down (islands: two dashers stood on""")
p.save()

# ---------------------------------------------------------------- phase.cpp (band bank)
p = Patch(B + 'phase.cpp')
p.rep("""    if (gems >= NEAR_BANK_GEMS && !prism_holds(me) && seen::champions(Team::enemy).empty() && !under_threat(me)) {
        const auto& spots = escort_bag(gems, owned_of(me).passives) ? solo_altars : seen::altar_list();
        std::vector<vec2> close;
        for (const auto altar : spots)
            if (altar_near(position, altar, BAND_BANK_RANGE)) close.push_back(altar);
        if (!close.empty() && bank_gems(me, close, "band.bank")) {""", """    if (gems < NEAR_BANK_GEMS) band_altar.reset();
    if (gems >= NEAR_BANK_GEMS && !prism_holds(me) && seen::champions(Team::enemy).empty() && !under_threat(me)) {
        const auto& spots = escort_bag(gems, owned_of(me).passives) ? solo_altars : seen::altar_list();
        std::vector<vec2> close;
        for (const auto altar : spots)
            if (altar_near(position, altar, BAND_BANK_RANGE)) close.push_back(altar);
        // v230 (BAND_BANK_STICKY): the altar we set out for stays the goal a while, in range or not.
        const int round = static_cast<int>(bc::round_number());
        if (BAND_BANK_STICKY > 0 && close.empty() && band_altar && round - band_altar_round <= BAND_BANK_STICKY) close.push_back(*band_altar);
        if (!close.empty() && bank_gems(me, close, "band.bank")) {
            if (BAND_BANK_STICKY > 0 && !(band_altar && round - band_altar_round <= BAND_BANK_STICKY)) {
                band_altar = close.front();
                for (const auto altar : close)
                    if (distance(position, altar) < distance(position, *band_altar)) band_altar = altar;
                band_altar_round = round;
            }""")
p.save()

# ---------------------------------------------------------------- turn.cpp
p = Patch(B + 'turn.cpp')
p.rep("""        // v71: someone hidden is hitting us, and keeps doing so until the channel would end: stop it.""",
      """        // v230 (TRIBUTE_STAY): for everyone, above TRIBUTE_STAY_HEALTH.
        if (TRIBUTE_STAY && threat && kind == "tribute" && me.health() >= me.max_health() * TRIBUTE_STAY_HEALTH) {
            threat = false;
            LOG(why = "stays on the tribute (damage does not break it): " + why;);
        }
        // v71: someone hidden is hitting us, and keeps doing so until the channel would end: stop it.""")
p.rep("""        WHY("hurt.dodge", hurt + (channel_cut ? "; channel just cancelled, step out of reach" : "; in enemy reach, step out first"));
        bool dodged = false;""", """        WHY("hurt.dodge", hurt + (channel_cut ? "; channel just cancelled, step out of reach" : "; in enemy reach, step out first"));
        // v230 (DODGE_HIT): melee walk and hit in one turn, so the parting hit is free.
        if (DODGE_HIT && me.attack_range() <= 1 && !enemies.empty()) {
            std::vector<bc::champion> reach;
            for (const auto& e : enemies)
                if (within(me, e, me.attack_range())) reach.push_back(e);
            if (const auto target = lowest(reach); target && attack_if_in_range(*target)) NOTE("parting hit on the weakest enemy in reach");
        }
        bool dodged = false;""")
p.save()
print('ok')
