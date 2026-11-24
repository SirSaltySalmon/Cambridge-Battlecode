"""v233 = bots/v231 + VENDETTA_FOR_WILLOW (run once from the repo root, after copying bots/v231 to bots/v233).
Reasons and benches: bots/v233/README.md."""
import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch

B = 'bots/v233/src/'
p = Patch(B + 'constants.h')
p.before("// ---- v231: fight coordination", """// ---- v233: roster (bench: v233 README). ----
// VENDETTA_FOR_WILLOW: Vendetta takes Willow's pick on teams of four and five (not on the Prism team of gem-cluster maps).
inline constexpr bool VENDETTA_FOR_WILLOW = true;

""")
p.save()
p = Patch(B + 'draft.cpp')
p.before("bool varg_map() {", """// v233 (VENDETTA_FOR_WILLOW): the same line-ups with Vendetta in Willow's pick.
constexpr Draft ROSTER_5_V[] = {Draft::ryth, Draft::heart_of_ruin, Draft::vendetta, Draft::vesper_vale, Draft::marcella};
constexpr Draft ROSTER_5_PRISM_V[] = {Draft::ryth, Draft::heart_of_ruin, Draft::vendetta, Draft::vesper_vale, Draft::prism};
constexpr Draft ROSTER_5_VARG_V[] = {Draft::ryth, Draft::heart_of_ruin, Draft::vendetta, Draft::vesper_vale, Draft::varg};
constexpr Draft ROSTER_4_V[] = {Draft::heart_of_ruin, Draft::vesper_vale, Draft::vendetta, Draft::ryth};
constexpr Draft ROSTER_4_PRISM_V[] = {Draft::heart_of_ruin, Draft::vesper_vale, Draft::vendetta, Draft::prism};

""")
p.rep("""    if (size == 4) return gems >= PRISM_MIN_GEMS_4 ? std::span<const Draft>(ROSTER_4_PRISM) : ROSTER_4;
    if (gems >= PRISM_MIN_GEMS_5 && PRISM_OVER_VARG) return ROSTER_5_PRISM;
    static const bool varg = varg_map();
    if (gems >= PRISM_MIN_GEMS_5 && !varg) return ROSTER_5_PRISM;
    return varg ? std::span<const Draft>(ROSTER_5_VARG) : ROSTER_5;""", """    if (VENDETTA_FOR_WILLOW) {
        if (size == 4) return gems >= PRISM_MIN_GEMS_4 ? std::span<const Draft>(ROSTER_4_PRISM_V) : ROSTER_4_V;
        if (gems >= PRISM_MIN_GEMS_5 && PRISM_OVER_VARG) return ROSTER_5_PRISM_V;
        static const bool varg_v = varg_map();
        if (gems >= PRISM_MIN_GEMS_5 && !varg_v) return ROSTER_5_PRISM_V;
        return varg_v ? std::span<const Draft>(ROSTER_5_VARG_V) : ROSTER_5_V;
    }
    if (size == 4) return gems >= PRISM_MIN_GEMS_4 ? std::span<const Draft>(ROSTER_4_PRISM) : ROSTER_4;
    if (gems >= PRISM_MIN_GEMS_5 && PRISM_OVER_VARG) return ROSTER_5_PRISM;
    static const bool varg = varg_map();
    if (gems >= PRISM_MIN_GEMS_5 && !varg) return ROSTER_5_PRISM;
    return varg ? std::span<const Draft>(ROSTER_5_VARG) : ROSTER_5;""")
p.save()
print('v233 patched')
