import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch
B = 'bots/v229/src/'
p = Patch(B + 'route.cpp')
p.rep("""            s = pick_target(s, me.position(), capture);""", """            s = pick_target(s, me.position(), capture);
            // v229 (debug only): the leader's target is down for a while: say why each group that is up was not taken.
            LOG({
                const int n_groups = static_cast<int>(route.size());
                const int now_round = static_cast<int>(bc::round_number());
                if (s >= 0 && s < n_groups && ready_at[s] - now_round > 10 && now_round % 10 == 0) {
                    std::string text = format("waits %d rounds on s%d; up now:", ready_at[s] - now_round, s);
                    const auto& table = travel(me.position(), capture);
                    for (int u = 0; u < n_groups; ++u) {
                        if (ready_at[u] > now_round) continue;
                        text += format(" s%d(%d,%d)z%d", u, centers[u].x, centers[u].y, zone(u));
                        if (table.has(u)) text += format(":in-table,%d steps", static_cast<int>(table.at(u)));
                        else text += std::string(":dropped") + (!zone_open(u) ? ",zone-closed" : "") + (is_monolith(u) ? ",monolith" : "")
                            + (gemless[u] ? ",gemless" : "") + (pt_gated(u) && !pt_ok(u) ? ",pt-gate" : "") + (abandoned(u) ? ",abandoned" : "");
                    }
                    note(text);
                }
            });""")
p.save()
