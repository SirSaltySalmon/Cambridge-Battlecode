import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch
B = 'bots/v227/src/'
p = Patch(B + 'constants.h')
p.rep("inline constexpr bool RESPAWN_CLEAR = true;", """inline constexpr bool RESPAWN_CLEAR = true;
// RESPAWN_WAY: while an ally stands on a spawn tile of a group that is down, the rest wait RESPAWN_WAY_RANGE tiles off
//   it instead of WAIT_RANGE (2), so the ally has a free tile to step onto. gemcombat5's 20-cluster group sits in a
//   walled pocket with a three-tile mouth: the waiters filled the mouth and Prism could not get off her tile.
inline constexpr int RESPAWN_WAY_RANGE = 4;""")
p.save()
p = Patch(B + 'combat.cpp')
p.rep("""        if (ready_at[s] > now) {
            const int wait = ready_at[s] - now;""", """        // v227 (RESPAWN_WAY): the group is down and an ally stands on one of its tiles: give it room to step off.
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
p.save()
