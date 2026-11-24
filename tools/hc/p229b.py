import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch
B = 'bots/v229/src/'
p = Patch(B + 'route.cpp')
a = p.s.index("            // v229 (debug only): the leader's target is down for a while")
b = p.s.index("            });", a) + len("            });")
block = p.s[a:b]
p.s = p.s[:a].rstrip('\n') + p.s[b:]
p.s = p.s.replace("            s = pick_target(s, me.position(), capture);\n        } else if (V86_ZONES && zones_known) {",
                  "            s = pick_target(s, me.position(), capture);\n        } else if (V86_ZONES && zones_known) {")
anchor = """        // v137 (TARGET_HOLD): a new target holds TARGET_HOLD_ROUNDS before another switch, unless the held one went"""
assert p.s.count(anchor) == 1
p.s = p.s.replace(anchor, block.replace("            ", "        ", 1).replace("\n            ", "\n        ") + "\n" + anchor)
p.save()
