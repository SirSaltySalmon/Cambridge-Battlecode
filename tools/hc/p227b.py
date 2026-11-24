import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch
B = 'bots/v227/src/'
p = Patch(B + 'constants.h')
p.before("// v225 (DODGE_HIT):", """// v227 (RESPAWN_CLEAR): a group we see respawning this turn (its timer run out or not) is never walked onto: any body on
//   a spawn tile holds the whole group down. The old rule walked onto the tiles whenever one of them was out of sight,
//   which a 20-cluster group's always is (adgato-gemcombat5 unopposed: Prism boxed in on a tile at the respawn, then the
//   team walked on and stepped off in turns for 1,875 rounds; the group never came back).
inline constexpr bool RESPAWN_CLEAR = true;

""")
p.save()
p = Patch(B + 'bot.h')
p.after("    std::vector<int> alive_seen;", "\n    std::vector<int> respawn_seen;  // v227 (RESPAWN_CLEAR): by slot, the last round we saw the group respawning")
p.save()
p = Patch(B + 'route.cpp')
p.rep("""        if (JOB_CLAIMS && index < static_cast<int>(state_seen.size())) state_seen[index] = now;""",
      """        if (JOB_CLAIMS && index < static_cast<int>(state_seen.size())) state_seen[index] = now;
        if (RESPAWN_CLEAR && state == 3) {
            if (respawn_seen.size() != route.size()) respawn_seen.assign(route.size(), -1000);
            respawn_seen[index] = now;
        }""")
p.save()
p = Patch(B + 'combat.cpp')
p.rep("""        } else if (ROAD_WALK && s < static_cast<int>(alive_seen.size()) && alive_seen[s] >= now - 1) {""",
      """        } else if (RESPAWN_CLEAR && s < static_cast<int>(respawn_seen.size()) && respawn_seen[s] == now) {
            // v227 (RESPAWN_CLEAR): due, and we see it still respawning: someone stands on a tile. Off them, all of us.
            WHY(stem + ".clear", format("s%d ", s) + at(center) + " is due but still respawning in our sight: keep off its tiles");
            bc::bitboard ring;
            for (const auto tile : spawn_lists[s]) ring.draw_chebyshev(tile, WAIT_RANGE);
            ring &= ~tiles;
            if (!ring.is_set(me.position()) && !move_towards_closest(ring, avoid)) move_towards_closest(ring);
        } else if (ROAD_WALK && s < static_cast<int>(alive_seen.size()) && alive_seen[s] >= now - 1) {""")
p.save()
