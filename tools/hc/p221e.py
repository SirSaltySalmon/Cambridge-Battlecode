"""v221 step e: no jobs on monolith-run maps; the walk onto a half-cleared long group; a sticky hurt-band bank (run once)."""
import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch

B = 'bots/v221/src/'

p = Patch(B + 'constants.h')
p.after("""inline constexpr bool TRIBUTE_STAY = true;
inline constexpr double TRIBUTE_STAY_HEALTH = 0.4;""", """
// Jobs are off (the old shared target only) on gem-starved maps and maps with a gated altar: there the monoliths decide the
//   game and the team takes them together (00contest with jobs: five champions on gold clusters for 2,000 rounds, 0 gems).
// v221 (ROAD_WALK): a living group none of whose camps are in sight is walked to by the spawn tiles we do not see empty;
//   and standing on a dead member's tile of a living group holds nothing down. 01yellow-brick-road's 29-cluster road:
//   with its near end cleared, the walk went onto the dead tiles under us and the step-off rule sent us off again
//   (5,081 walk + 2,834 step-off turns, 15 gems).
inline constexpr bool ROAD_WALK = true;
// v221 (BAND_BANK_STICKY): the hurt-band bank walk (35-60% health, a bag, an altar within BAND_BANK_RANGE steps) keeps its
//   altar for this many rounds: at the range's edge it and the job's walk undid each other (04pinwheel: 548 turns).
inline constexpr int BAND_BANK_STICKY = 12;""")
p.save()

p = Patch(B + 'bot.h')
p.after("""    int job_crew = 1;
    std::vector<int> job_skip_until;""", """
    // v221 (ROAD_WALK): by slot, the last round we saw the group alive. (BAND_BANK_STICKY): the band's altar and round.
    std::vector<int> alive_seen;
    std::optional<vec2> band_altar;
    int band_altar_round = -1000;""")
p.save()

p = Patch(B + 'spread.cpp')
p.rep("""        jobs_ready = groups > 0;
        LOG(say(format("setup: jobs on, %d of %d groups are bare-solo", count, groups)););""", """        bool gated = false;
        for (const int g : gate_slots)
            if (g >= 0) gated = true;
        jobs_ready = groups > 0 && !gem_starved && !gated;
        LOG(say(format("setup: jobs %s, %d of %d groups are bare-solo", jobs_ready ? "on" : "off (monolith map)", count, groups)););""")
p.save()

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
        }""")
p.save()

p = Patch(B + 'combat.cpp')
p.rep("""        } else if (FIX_POCKET && tiles.is_set(me.position())) {
            // v103: due but not here: our own body on a spawn tile holds the group down (islands: two dashers stood on""",
      """        } else if (ROAD_WALK && s < static_cast<int>(alive_seen.size()) && alive_seen[s] >= now - 1) {
            // v221 (ROAD_WALK): alive, its living camps out of sight: toward the tiles we do not see empty.
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
        // v221 (BAND_BANK_STICKY): the altar we set out for stays the goal a while, in range or not.
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
print('ok')
