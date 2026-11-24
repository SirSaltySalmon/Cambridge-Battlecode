import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch
B = 'bots/v222/src/'
p = Patch(B + 'constants.h')
p.before("// ---- v222 (DRAFT_SET):", """// v222 (HOR_RECALL_HEALTH): Heart of Ruin leaves a fight under this share of his health (0: RECALL_HEALTH, 35%).
//   Platform baseline, 26 decoded games: he is 40% of our deaths to champions (59 of 147, 2.3 a game; the next is Ryth
//   with 25), 72% of them in fights where we had as many bodies within 5 tiles or more, a median 13 rounds after he
//   was last at 90%. HOR_HOLD keeps him in under the bar until one burst from dead, and patch 4.0.0 cut his shield and
//   dash by a fifth (Focus 100 -> 80).
inline constexpr double HOR_RECALL_HEALTH = 0;

""")
p.save()
p = Patch(B + 'turn.cpp')
p.rep("""    if (PAIR_RECALL_HEALTH > 0 && (me.champion_name() == Champ::cassi || me.champion_name() == Champ::issac))
        recall_share = std::max(recall_share, PAIR_RECALL_HEALTH);""", """    if (PAIR_RECALL_HEALTH > 0 && (me.champion_name() == Champ::cassi || me.champion_name() == Champ::issac))
        recall_share = std::max(recall_share, PAIR_RECALL_HEALTH);
    if (HOR_RECALL_HEALTH > 0 && me.champion_name() == Champ::heart_of_ruin) recall_share = std::max(recall_share, HOR_RECALL_HEALTH);""")
p.save()
