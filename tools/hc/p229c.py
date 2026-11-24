import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch
B = 'bots/v229/src/'
p = Patch(B + 'constants.h')
p.before("// v227 (RESPAWN_CLEAR):", """// v229 (FILL_SUSTAINED): while the goal is down, what a filler's detour costs the goal is the goal's sustained rate (its
//   value over one whole cycle: respawn + clear + capture) times the rounds we arrive late, not its rate from where we
//   stand (value over the short wait in front of us). The old price made any walk longer than the wait cost more than a
//   far group is worth, so the team stood beside a respawn: unopposed 02tangle 50% of turns waiting (648 gems in 2,000
//   rounds; six elk groups never touched, 44-150 walking steps away in the maze), 02famine 50%, 03twisty 32%,
//   04terraces 28%; over all maps 12.5%.
inline constexpr bool FILL_SUSTAINED = true;

""")
p.save()
p = Patch(B + 'route.cpp')
p.rep("""        const double dest_rate = route_rate_steps(dest, steps.at(dest), now);
        const int until = std::max(ready_at[dest] - now, static_cast<int>(steps.at(dest)));""", """        const double dest_rate = FILL_SUSTAINED
            ? values[aggressive][dest] / std::max(1.0, route[dest]->max_respawn_timer() + clears[dest] + capture_rounds)
            : route_rate_steps(dest, steps.at(dest), now);
        const int until = std::max(ready_at[dest] - now, static_cast<int>(steps.at(dest)));""")
p.save()
