import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch
B = 'bots/v222/src/'
p = Patch(B + 'constants.h')
p.rep("inline constexpr bool JOBS = true;", "inline constexpr bool JOBS = false;")
p.before("// ---- v221 (JOBS): every champion farms its own group", """// ---- v222 (DRAFT_SET): rosters with Cassi & Issac (2026-11-10 platform study). ----
// 100 unranked games (20 opponents ranked 5-160 x 5 maps, results/plat/base218): the live bot went 23-77. In its 77
//   losses the opponents dealt 8.7k camp damage in the first 100 rounds to our 6.5k (+33%, before any item or kill), had
//   collected 72 gems to 50 and banked 187 to 66 by round 300. Their rosters hit harder from round 1: basic attacks, resets
//   and spells a round are about C&I 106 (one pick, two bodies), Vendetta 73, Willow 62, Ryth 63, Marcella 60, Prism 63,
//   HoR 45, Vesper 39 (after patch 4.0.0). Ours (Ryth, HoR, Willow, Vesper, Marcella) is 269; The Onkeys' (Marcella,
//   Vendetta, Vesper, C&I, Ryth) 341, Intercontinental Fish's (Marcella, Vendetta, HoR, C&I, Willow) 346. The top three
//   teams take C&I in nearly every draft (top-16 pick rate 58%, win rate 55%).
// DRAFT_SET 0: the old rosters. 1: five C&I/HoR/Willow/Vesper/Marcella, four C&I/HoR/Vesper/Willow, three
//   C&I/HoR/Vesper. 2: five Marcella/Vendetta/Vesper/C&I/Ryth, four C&I/Vendetta/Vesper/Marcella, three C&I/Vendetta/Vesper.
//   3: five C&I/HoR/Vendetta/Vesper/Willow, four C&I/HoR/Vesper/Vendetta, three C&I/HoR/Vesper.
// DRAFT_SET_CLUSTER: the set also replaces the Prism team on cluster maps.
inline constexpr int DRAFT_SET = 1;
inline constexpr bool DRAFT_SET_CLUSTER = false;

""")
p.rep("inline constexpr bool PRISM_TEAM = !JOBS;  // v221 (JOBS): no single-collector team",
      "inline constexpr bool PRISM_TEAM = !JOBS && !(DRAFT_SET > 0 && DRAFT_SET_CLUSTER);  // v221 (JOBS), v222: no single-collector team")
p.save()
p = Patch(B + 'draft.cpp')
p.rep("""std::span<const Draft> roster(std::uint32_t size) {
    if (PROXY_DRAFT)""", """// v222 (DRAFT_SET): see constants.h.
constexpr Draft SET1_5[] = {Draft::cassi_and_issac, Draft::heart_of_ruin, Draft::willow, Draft::vesper_vale, Draft::marcella};
constexpr Draft SET1_4[] = {Draft::cassi_and_issac, Draft::heart_of_ruin, Draft::vesper_vale, Draft::willow};
constexpr Draft SET1_3[] = {Draft::cassi_and_issac, Draft::heart_of_ruin, Draft::vesper_vale};
constexpr Draft SET2_5[] = {Draft::marcella, Draft::vendetta, Draft::vesper_vale, Draft::cassi_and_issac, Draft::ryth};
constexpr Draft SET2_4[] = {Draft::cassi_and_issac, Draft::vendetta, Draft::vesper_vale, Draft::marcella};
constexpr Draft SET2_3[] = {Draft::cassi_and_issac, Draft::vendetta, Draft::vesper_vale};
constexpr Draft SET3_5[] = {Draft::cassi_and_issac, Draft::heart_of_ruin, Draft::vendetta, Draft::vesper_vale, Draft::willow};
constexpr Draft SET3_4[] = {Draft::cassi_and_issac, Draft::heart_of_ruin, Draft::vesper_vale, Draft::vendetta};

std::span<const Draft> roster(std::uint32_t size) {
    if (DRAFT_SET > 0 && !prism_map()) {
        if (DRAFT_SET == 1) return size <= 3 ? std::span<const Draft>(SET1_3) : size == 4 ? std::span<const Draft>(SET1_4) : SET1_5;
        if (DRAFT_SET == 2) return size <= 3 ? std::span<const Draft>(SET2_3) : size == 4 ? std::span<const Draft>(SET2_4) : SET2_5;
        return size <= 3 ? std::span<const Draft>(SET1_3) : size == 4 ? std::span<const Draft>(SET3_4) : SET3_5;
    }
    if (PROXY_DRAFT)""")
p.save()
