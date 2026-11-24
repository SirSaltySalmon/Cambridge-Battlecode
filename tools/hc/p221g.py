"""v221 step g: the deal over allies in sight uses one distance for everyone, so both sides reach the same deal (run once)."""
import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch

B = 'bots/v221/src/'

p = Patch(B + 'constants.h')
p.after("""inline constexpr bool JOB_BREAKER = true;
inline constexpr int JOB_BREAK_ROUNDS = 15;""", """
// The deal is the same seen from either side: every agent's walk is its straight distance to the group's centre (our own
//   path steps only filter what we can reach), and whoever stands within JOB_NEAR of a group keeps it (x JOB_KEEP), us
//   included. Before (our path steps against their straight distance, a keep bonus only for our own job), 34% of solo-job
//   turns were spent on a group another champion also held, 15% of them hitting or capturing side by side.
inline constexpr int JOB_NEAR = 3;""")
p.save()

p = Patch(B + 'spread.cpp')
p.rep("""                const int walk = a == 0 ? k.steps : distance(agents[a].at, centers[k.s]);
                double rate = k.value / k.crew / (std::max(walk, k.ready) + k.rounds + capture);
                if (a == 0 && k.s == spread_job) rate *= JOB_KEEP;""", """                const int walk = distance(agents[a].at, centers[k.s]);
                double rate = k.value / k.crew / (std::max(walk, k.ready) + k.rounds + capture);
                if (walk <= JOB_NEAR) rate *= JOB_KEEP;
                else if (a == 0 && k.s == spread_job) rate *= JOB_KEEP;""")
p.save()
print('ok')
