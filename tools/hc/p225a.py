import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch
B = 'bots/v225/src/'
p = Patch(B + 'constants.h')
p.before("// v222 (HOR_RECALL_HEALTH):", """// v225 (DODGE_HIT): a melee champion stepping out of enemy reach under the recall bar hits the weakest enemy in reach
//   first (melee walk and hit in one turn, so the hit is free). v223 mirror, 128 games: 532 Heart of Ruin and 389 Ryth
//   deaths to champions, the commonest ending five dodge turns in a row (97 and 99): one tile a round, hit every round,
//   never hitting back.
// DODGE_STAND: after this many dodge turns in a row with an enemy champion still within DODGE_STAND_RANGE, a melee
//   champion fights instead (0 off): the walk is not getting it out.
inline constexpr bool DODGE_HIT = true;
inline constexpr int DODGE_STAND = 0, DODGE_STAND_RANGE = 2;

""")
p.save()
p = Patch(B + 'bot.h')
p.after("    int job_hist_n = 0;\n    bool job_turn = false;", "\n    // v225 (DODGE_STAND): the last round we dodged and how many in a row.\n    int dodge_last = -10, dodge_streak = 0;")
p.save()
p = Patch(B + 'turn.cpp')
p.rep("""        WHY("hurt.dodge", hurt + (channel_cut ? "; channel just cancelled, step out of reach" : "; in enemy reach, step out first"));
        bool dodged = false;""", """        // v225 (DODGE_STAND, DODGE_HIT): see constants.h.
        const bool melee = me.attack_range() <= 1;
        dodge_streak = dodge_last == round_now - 1 ? dodge_streak + 1 : 1;
        dodge_last = round_now;
        if (DODGE_STAND > 0 && melee && dodge_streak > DODGE_STAND) {
            bool close = false;
            for (const auto& e : enemies)
                if (distance(position, e.position()) <= DODGE_STAND_RANGE) close = true;
            if (close && fight(me)) {
                NOTE(format("dodged %d turns in a row with an enemy still on us: fights", dodge_streak - 1));
                return;
            }
        }
        WHY("hurt.dodge", hurt + (channel_cut ? "; channel just cancelled, step out of reach" : "; in enemy reach, step out first"));
        if (DODGE_HIT && melee && !enemies.empty()) {
            std::vector<bc::champion> reach;
            for (const auto& e : enemies)
                if (within(me, e, me.attack_range())) reach.push_back(e);
            if (const auto target = lowest(reach); target && attack_if_in_range(*target)) NOTE("parting hit on the weakest enemy in reach");
        }
        bool dodged = false;""")
p.save()
