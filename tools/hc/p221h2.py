import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch
B = 'bots/v221/src/'
p = Patch(B + 'bot.h')
p.rep("    bool claims_on() const { return JOBS && JOB_CLAIMS && jobs_ready && jobs_layout(); }", "    bool claims_on() const;")
p.save()
p = Patch(B + 'spread.cpp')
p.before("void Bot::claims_read(const bc::champion&) {", "bool Bot::claims_on() const {\n    return JOBS && JOB_CLAIMS && jobs_ready && jobs_layout();\n}\n\n")
p.save()
