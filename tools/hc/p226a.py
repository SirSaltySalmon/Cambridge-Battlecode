import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch
B = 'bots/v226/src/'
p = Patch(B + 'constants.h')
p.before("// v225 (DODGE_HIT):", """// v226 (PRISM_WANT_RANGE): on the Prism team a cleared gem reward is hers (the allies wait off its tiles) only while she
//   is within this many tiles of it; further, whoever cleared it captures it (the usual claims). It was
//   VESPER_CLAIM_RANGE (8): the team moves as one group, so she was always "near" and every capture queued behind her
//   (unopposed, r1-600: 04gem-caverns 48% of turns "cleared and hers: wait off its tiles", 04atoll 67% waiting on
//   respawns; platform cluster maps: 300 gems collected by r300, opponents 387-420).
inline constexpr int PRISM_WANT_RANGE = 8;

""")
p.save()
p = Patch(B + 'route.cpp')
p.rep("        if (ally.champion_name() == Champ::prism) return distance(ally.position(), centers[s]) <= VESPER_CLAIM_RANGE;",
      "        if (ally.champion_name() == Champ::prism) return distance(ally.position(), centers[s]) <= PRISM_WANT_RANGE;")
p.save()
