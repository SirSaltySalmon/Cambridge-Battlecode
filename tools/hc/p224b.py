import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch
B = 'bots/v224/src/'
p = Patch(B + 'constants.h')
p.after("inline constexpr bool JOB_ALL_MAPS = false;", """
// JOB_CARRY_STRENGTH: the carry takes jobs only from this much Strength (0: from the start). Bare (35) she clears nothing
//   but clusters alone and the team's first camps miss her body and her capture: unopposed, jobs from round 1 banked 144
//   by r300 against 156 without jobs (452 against 426 by r600).
inline constexpr double JOB_CARRY_STRENGTH = 0;""")
p.save()
p = Patch(B + 'spread.cpp')
p.rep("    if (JOBS && !job_taker) return false;", """    if (JOBS && !job_taker) return false;
    if (JOBS && job_mode == JOB_MODE_CARRY && JOB_CARRY_STRENGTH > 0 && me.strength() < JOB_CARRY_STRENGTH) return false;""")
p.save()
p = Patch(B + 'turn.cpp')
p.rep("            if (JOBS && spread_on() && job_taker && mode == FARM && seen::champions(Team::enemy).empty()) {",
      "            if (JOBS && spread_on() && job_taker && mode == FARM && seen::champions(Team::enemy).empty()\n                && !(job_mode == JOB_MODE_CARRY && JOB_CARRY_STRENGTH > 0 && me.strength() < JOB_CARRY_STRENGTH)) {")
p.save()
