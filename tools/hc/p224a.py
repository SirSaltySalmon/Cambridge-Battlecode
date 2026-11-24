import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch
B = 'bots/v224/src/'
p = Patch(B + 'constants.h')
p.rep("inline constexpr int JOB_MODE_OFF = 0, JOB_MODE_CARRY = 1, JOB_MODE_ALL = 2;", """inline constexpr int JOB_MODE_OFF = 0, JOB_MODE_CARRY = 1, JOB_MODE_ALL = 2;
// v224 (JOB_ALL_MAPS): false keeps the Prism team (no jobs) on cluster maps. Platform, 26 cluster-map games: everyone on
//   jobs went 2-24 where the Prism team went 8-18 (7.0 deaths by r600 against 2.7: lone champions lose the 1v1s and
//   1v2s), though it collected as fast (299 gems by r300 against 300; opponents 420).
inline constexpr bool JOB_ALL_MAPS = false;""")
p.rep("inline constexpr bool PRISM_TEAM = !JOBS && !(DRAFT_SET > 0 && DRAFT_SET_CLUSTER);",
      "inline constexpr bool PRISM_TEAM = !(JOBS && JOB_ALL_MAPS) && !(DRAFT_SET > 0 && DRAFT_SET_CLUSTER);")
p.save()
p = Patch(B + 'spread.cpp')
p.rep("        job_mode = !jobs_ready ? JOB_MODE_OFF : draft::cluster_map() ? JOB_MODE_ALL : JOB_MODE_CARRY;",
      "        job_mode = !jobs_ready ? JOB_MODE_OFF : draft::cluster_map() ? (JOB_ALL_MAPS ? JOB_MODE_ALL : JOB_MODE_OFF) : JOB_MODE_CARRY;")
p.save()
