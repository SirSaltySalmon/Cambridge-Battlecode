// Tunables, shop builds, and the shared-memory layout.
#pragma once
#include <cambc.h>
#include "evo_constants.h"  // fight-score numbers tools/evotune varies

// ---- v158: v155 + the Prism team on gem-cluster maps (draft.cpp prism_map, Bot::prism_team). ----
// PRISM_TEAM_BAR: above this share of a map's camps being gem clusters we draft Prism, HoR, Marcella, Ryth, Vendetta (the
//   first picks of it on smaller teams). Every other map plays as v155.
// PRISM_CLUSTER_VALUE: a gem cluster's gems count this many times in a group's value, and gold counts at GOLD_WEIGHT, so
//   gem clusters come first and gold clusters (still GEMLESS_VALUE) last.
// Prism takes every gem reward and holds her bag until tributed + bag reaches WIN_GEMS. Two safety starts latch the hold
//   off: the tribute would otherwise miss PRISM_ROUND_LIMIT, or the enemy's pace (over PRISM_RACE_ROUNDS to
//   2 x PRISM_RACE_ROUNDS) reaches WIN_GEMS before it ends. Her walk counts as nearest altar x PRISM_CASH_WALK.
// PRISM_CASH_GEMS: a cash-in of this bag or more calls the team to guard her altar (the pair's target bits carry it).
// PRISM_CASH_STAY_HEALTH: damage does not break a tribute, so an enemy in reach only cancels hers under this health.
inline constexpr double PRISM_TEAM_BAR = 0.70;
inline constexpr double PRISM_CLUSTER_VALUE = 4;
inline constexpr int GEM_CLUSTER_GEMS = 7;
inline constexpr int PRISM_ROUND_LIMIT = 2000, PRISM_RACE_ROUNDS = 50, PRISM_CASH_WALK = 2, PRISM_CASH_MARGIN = 30;
inline constexpr int PRISM_CASH_GEMS = 40;
// ---- v180 (PRISM_GUARD): the allies bodyguard her cash-in instead of guard()'s camp farming and hold. ----
// v179's guard() never looked at enemies and ran before the ping answer and the fight step: on adgato-gemcombat4 vs
//   v158-ref the four allies stood still for ~1,200 turns each while an enemy hit her channel (63 restarts, never
//   finished, lost 84-1008 at 34-3 kills); ladder: Tozo's gemcombat4 (r710-1000 frozen), JukeFall gemcombat3.
// In every mode, they hit any enemy within PRISM_GUARD_REACH of her (of her altar while she is out of sight), never
//   following one further; else clear an enemy lock on her altar; else stand within PRISM_GUARD_RANGE of her.
inline constexpr bool PRISM_GUARD = true;
inline constexpr int PRISM_GUARD_REACH = 6, PRISM_GUARD_RANGE = 2;
// v180 (PRISM_STRICT): she tributes only a bag that wins: tributed + her bag >= WIN_GEMS (the teammate's 1000+ plan).
//   The race start and PRISM_EARLY_CASH are off, and the team's carried gems (win_tribute) no longer end her hold;
//   only the PRISM_ROUND_LIMIT start remains (holding past it loses the tiebreak on tributed gems).
inline constexpr bool PRISM_STRICT = true;
// v181 (PRISM_CASH_ANY_MODE): once her cash-in is due (she no longer holds) she walks to her altar and tributes whatever the
//   team's mode; before, only outside FIGHT, which KILL_TIME made rare with her bag counted.
inline constexpr bool PRISM_CASH_ANY_MODE = true;
inline constexpr double PRISM_CASH_STAY_HEALTH = 0.35;
// PRISM_STEP_OFF_RANGE: how far an ally looks for a tile off a gem reward held for her, when no neighbour tile is one.
inline constexpr int PRISM_STEP_OFF_RANGE = 4;
// PRISM_STEP_OFF_TURNS: an ally steps off after this many turns in a row on it; one walking through a corridor keeps going
//   (00quadrants: a reward in a corridor sent allies back and forth for 280 turns a game).
inline constexpr int PRISM_STEP_OFF_TURNS = 3;

// ---- v160: Prism team switches. ----
// PRISM_MOVE_WITH: while Prism holds a claim the leader starts no group further than PRISM_WITH_RANGE from her reward, and
//   allies further than that walk to it.
// PRISM_FIGHT_NEAR: a fight with no living Prism within PRISM_WITH_RANGE is left for where she is (her reward, else the team
//   target), unless a melee enemy is on us or we cannot move.
// PRISM_EARLY_CASH: her tribute starts once tributed + bag + PRISM_EARLY_INCOME x the team's gem rate x (walk + channel) reaches
//   WIN_GEMS. Completion tributes her whole bag, and the allies farm and bank meanwhile instead of guarding.
// PRISM_WILLOW: Willow in Vendetta's place; her heal counts Prism's gain PRISM_HEAL_WEIGHT times.
inline constexpr bool PRISM_MOVE_WITH = true, PRISM_FIGHT_NEAR = true, PRISM_EARLY_CASH = true, PRISM_WILLOW = true;
inline constexpr int PRISM_WITH_RANGE = 8, PRISM_NO_GUARD_BIT = 0x20;
inline constexpr double PRISM_EARLY_INCOME = 0.5, PRISM_HEAL_WEIGHT = 2;

// ---- v161: PRISM_PICKERS: Marcella and HoR pick up gem rewards and bank them, so her last tribute is shorter. ----
// Theirs: a group under the map's gem-weighted median, or under twice it while Prism holds a claim elsewhere. They hold private
// claims (the shared one is single), within PRISM_PICKER_RANGE. A reward nobody took in PRISM_PICKER_WAIT rounds is hers again.
inline constexpr bool PRISM_PICKERS = false;  // v167: off, every gem reward is hers first
// v167 PRISM_CLAIM_NEAR: gem rewards go to Prism the way loot goes to a funnelled Vesper: first in line within VESPER_CLAIM_RANGE of
//   the reward, and with her further off or out of sight an ally may take it rather than leave it. Allies stay off it only then.
// PRISM_WILLOW_FOLLOW: while farming, Willow keeps within PRISM_WILLOW_RANGE of Prism (of where she is, out of sight).
inline constexpr bool PRISM_CLAIM_NEAR = true, PRISM_WILLOW_FOLLOW = true;
inline constexpr int PRISM_WILLOW_RANGE = 3;
inline constexpr int PRISM_PICKER_RANGE = 10, PRISM_PICKER_WAIT = 20, PRISM_PICKER_TIMEOUT = 25;

// ---- v168: PRISM_OUR_SIDE: the Prism team clears groups it reaches no later than the enemy first (walking steps from each spawn). ----
// The draft plan anchors on them, and they are its SAFE zone; the rest are at most UNCERTAIN unless ZONE_SIDE makes them UNSAFE. The goal is
// the best of them up within PRISM_SIDE_WAIT rounds of our arrival; with none, the goal is picked anywhere as before. No raid meanwhile.
inline constexpr bool PRISM_OUR_SIDE = true;
inline constexpr int PRISM_SIDE_WAIT = 15;

// ---- v169: Prism team. ----
// PRISM_WILLOW_4: on four picks Willow takes Ryth's place (4 of v168's 6 fight losses had no Willow).
// PRISM_STAY_BACK: while Prism's max health is under TANK_HEALTH (no Durability yet) she is not the champion nearest the enemy in a
//   fight: with no melee ally closer she steps back to the nearest one (or to the team target), and with one only just ahead she holds
//   instead of closing. An enemy within PRISM_SPELL_REACH with a melee ally ahead is cast at as usual.
inline constexpr bool PRISM_WILLOW_4 = true;
inline constexpr bool PRISM_STAY_BACK = true;
inline constexpr int PRISM_SPELL_REACH = 4;

// ---- v171: v165-gold + v170's Prism team. ----
// PRISM_AS_V170: on the Prism team the v156-v165 economy changes are undone, so those maps play as v170: no satellite
//   (it would also take target2, which carries her cash-in altar), GOLD_ONLY_NEAR V170_GOLD_ONLY_NEAR, no EARLY_BANK, no
//   BANK_ON_WAY, INVADE on, no SHOP_NO_TRIBUTE / SHOP_EMPTY_BAG. The 9 Prism maps have no monolith, so the monolith
//   changes do not reach them.
inline constexpr bool PRISM_AS_V170 = true;
inline constexpr int V170_GOLD_ONLY_NEAR = 2;

// ---- v177: from the 00collision VOD vs QS2 (ladder, 2026-11-01): ahead on gems until r1400, lost 923-1055. ----
// ECON_PHASES: the farm values change once the team is built (BUILT_EMBLEMS team emblems per champion, the point where
//   STRONG_FADE_EMBLEMS says elks stop costing extra). Before: gold-only groups count EARLY_GOLD_SCALE of their value
//   (not GEMLESS_VALUE) and may be the team's goal, not only a filler. After: elk gems count ELK_GEM_MULT and gem-cluster
//   gems CLUSTER_GEM_MULT, gold-only groups go back to GEMLESS_VALUE, and best_goal rates every group, not the pocket
//   alone. That game: QS2 cleared their three edge elk groups 34 times, we cleared ours 4 times and our 40-gold
//   clusters 14-16 times each (the pocket is the map's middle line, where the altars are, so the edge elks were never
//   rated). Not on the Prism team (as_v170).
inline constexpr bool ECON_PHASES = true;
inline constexpr double BUILT_EMBLEMS = 2.0;
inline constexpr double EARLY_GOLD_SCALE = 1.0;
inline constexpr double ELK_GEM_MULT = 2.0, CLUSTER_GEM_MULT = 1.5;
inline constexpr int ELK_GEMS = 15;
// Built, a gold-only group is taken on the way only within BUILT_GOLD_ONLY_NEAR steps (GOLD_ONLY_NEAR before).
inline constexpr int BUILT_GOLD_ONLY_NEAR = 2;
// CLOSE_OUT: from CLOSE_OUT_ROUND, while our tributed gems are ahead of theirs, the team farms to the win instead of
//   pressing: no aggressive route, no raid or invasion, no group whose centre is in the enemy's half (nearer their
//   spawn), no contest pricing, CLOSE_OUT_FIGHT_PENALTY off the fight score when the nearest enemy stands in their half,
//   and bags of CLOSE_OUT_BANK are tributed. (A safe-zone-only first try also dropped 00collision's best group, the
//   three-elk pack in our corner: it is in the uncertain zone.) That game: from r1024 our HoR and Vendetta died 8 times
//   in the middle and their half while we led 861-728 (r1300); their kills let them tribute 162 gems at r1496 and win.
inline constexpr bool CLOSE_OUT = true;
inline constexpr int CLOSE_OUT_ROUND = 500, CLOSE_OUT_FIGHT_PENALTY = 15, CLOSE_OUT_BANK = 20;
// The parts that can be switched off (CLOSE_OUT alone keeps: no aggressive route, no raid, bags of CLOSE_OUT_BANK):
// CLOSE_OUT_HALF the enemy-half groups dropped, CLOSE_OUT_FIGHT the fight penalty, CLOSE_OUT_CONTEST no contest pricing.
inline constexpr bool CLOSE_OUT_HALF = false, CLOSE_OUT_FIGHT = false, CLOSE_OUT_CONTEST = false;

// ---- v150 switches (small maps; plan: ~/.claude/plans/velvety-puzzling-heron.md). All off = v149. ----
// LOCK_BUY: buy an altar lock (10 gold, carry one) when shopping: 1 on one-altar maps first, before the build; 2 also
//   on other maps from gold left under one shard after the build. LOCK_PLACE: a lock we carry goes on an unlocked altar
//   within the lock range (2) as we start a tribute there, or whenever we stand that close (it cancels enemy tributes
//   there; our own team still tributes through it).
inline constexpr int LOCK_BUY = 2;
inline constexpr bool LOCK_PLACE = true;
inline constexpr int LOCK_RANGE = 2;
// SMALL_BANK (0 off): on one-altar maps (and, with SMALL_BANK_THREE, on every three-champion map) a bag of SMALL_BANK
//   gems is banked instead of GEM_BANK, and with an enemy in sight (bank.hunted) one of SMALL_HUNTED instead of
//   CARRY_GEMS. Ladder: in single-altar losses we tribute 5 times a game, they 32; deaths take most of what we carry.
inline constexpr int SMALL_BANK = 0;
inline constexpr int SMALL_HUNTED = 0;
inline constexpr bool SMALL_BANK_THREE = false;
// SMALL_FUNNEL_TEAM (0 off): on teams of fewer champions, Vesper collects first only until SMALL_FUNNEL_EMBLEMS emblems
//   (not VESPER_FUNNEL_EMBLEMS; 0: never), and with SMALL_CLAIM_NEUTRAL nobody else collects first either (Marcella,
//   Varg, Prism rank 0), so a reward goes to the nearest ally, the poorer of two near ones (update_capture's key).
//   Ladder: on bad maps our non-Vesper champions collect ~80 gems a game to their ~300, our HoR earns 300 gold to 1,300.
inline constexpr int SMALL_FUNNEL_TEAM = 0;
inline constexpr int SMALL_FUNNEL_EMBLEMS = 4;
inline constexpr bool SMALL_CLAIM_NEUTRAL = false;
// VENDETTA_3 (draft.cpp): three-champion maps draft Heart of Ruin, Vendetta, Vesper (Vendetta in Marcella's place):
//   opponents with that lineup beat our HoR, Marcella, Vesper 60-22 on the ladder.
inline constexpr bool VENDETTA_3 = true;
// HOR_DIVE_FOLLOW (port of v146; 0 off, 1 every map, 2 teams of four or fewer): a dive landing counts only with
//   HOR_FOLLOW_MIN visible allies (fewer if fewer are alive) within HOR_FOLLOW_RANGE of it, so the team can hit what
//   the stun holds. v146 bench alone vs v145: +5.3% (219-189), Vesper deaths -11%.
// HOR_DIVE_HEALTH (0 off): on teams of four or fewer, no dive under this share of max health.
// HOR_HOLD_SMALL: on teams of four or fewer, no staying in a fight under the recall bar (hor_holds).
//   Ladder: our HoR dies 5.8 times a game on bad maps (theirs 1.4), carrying little.
inline constexpr int HOR_DIVE_FOLLOW = 1;
inline constexpr int HOR_FOLLOW_MIN = 2;
inline constexpr int HOR_FOLLOW_RANGE = 3;
inline constexpr double HOR_DIVE_HEALTH = 0;
inline constexpr bool HOR_HOLD_SMALL = false;
// HOR_SMALL_FOUR: HOR_DIVE_HEALTH and HOR_HOLD_SMALL apply to teams of exactly four (v150 bench: both gained on four,
//   lost on three).
inline constexpr bool HOR_SMALL_FOUR = false;
// VENDETTA_KILL_RESET: a Vendetta kill refreshes her cooldowns but not the turn's one cast (probe: 82 of 82 kills after
//   a reset cast left can_cast 0, cooldowns 0). So when her first hit of the turn kills with the cast still unused,
//   the reset goes on a second target in reach: the target group's next camp (a camp kill) or the weakest enemy
//   champion in reach (a champion kill; camps only when no champion is in reach).
inline constexpr bool VENDETTA_KILL_RESET = true;
// SPLIT_SMALL (user: split only once farming together has equipped everyone): the v86 split (V86_SPLIT, so far only
//   Cassi & Issac) on teams of four or fewer, with these members: three, Heart of Ruin alone (Marcella or Vendetta and
//   Vesper stay the main team); four, Ryth or Prism with Willow (SPLIT_PAIR_4 0) or HoR with Willow (1). The pair's lead
//   (Issac's part: picks and writes the pair target) is the first of Ryth, Prism, HoR; Willow captures (Cassi's part);
//   alone, HoR does both. A split opens only when every ally, pair and main team, is alive and owns SPLIT_MIN_EMBLEMS
//   emblems (the leader's emblem record; each member checks its own), quiet for SPLIT_QUIET_SMALL rounds, and the pair
//   group lies SPLIT_APART_SMALL from the main target. Run it with the small funnel (SMALL_FUNNEL_TEAM 5, cap 4,
//   SMALL_CLAIM_NEUTRAL) so the frontline gets gold first.
inline constexpr bool SPLIT_SMALL = false;
inline constexpr int SPLIT_PAIR_4 = 0;
inline constexpr int SPLIT_MIN_EMBLEMS = 2;
inline constexpr int SPLIT_QUIET_SMALL = 5;
inline constexpr int SPLIT_APART_SMALL = 6;
// SPLIT_ZONE_SMALL: the widest zone a small team's split may use, main target and pair group (ZONE_SAFE as in v86;
//   ZONE_UNCERTAIN adds the middle: close-spawn maps have few safe groups, 01hooks none).
inline constexpr int SPLIT_ZONE_SMALL = 0;

// ---- v135 switches (plan: bots/v135a/PLAN.md) ----
// F6 NO_ESCORT: no escorted tribute (774 escorts in 260 audit logs, 43 banked, 535 cancelled within 4 rounds by the
//   carrier's own stale timer). Heavy bags bank by the usual rules; the mode byte's carrier bits go to TANK_ON_DUTY.
inline constexpr bool NO_ESCORT = true;
// F1 CAPTURE_STAGE: nobody but the collector targets a group under capture (23.8k turns / 65 games walked onto its
//   tiles and stepped off: the capture_near table was empty, so pick_target fell back to the capture slot). The pick
//   is redone without capture_near; the others wait off the reward's tiles, at the CAPTURE_NEAR edge toward the next
//   group when the reward is outside the safe zone.
inline constexpr bool CAPTURE_STAGE = true;  // the old ring/edge wait lost (-7.2/-8.6%); on now only to run CAPTURE_MOVE_ON
// CAPTURE_REPICK: the re-pick part of CAPTURE_STAGE (the next group from the whole table); off keeps v134's pick and
//   only stops the walk onto the reward's tiles.
inline constexpr bool CAPTURE_REPICK = true;
// STAGE_PATH: the staging ring leaves the collector's walking path to the reward free (traced down the group's walk
//   field from the collector's tile): staged bodies on it deadlocked captures (02fields r725-792: five idle 65+ rounds).
inline constexpr bool STAGE_PATH = true;
// CAPTURE_MOVE_ON (user): the collector captures alone (the rules need one champion on one tile); everyone else treats
//   the group as done: never on its tiles, no waiting ring or edge wait, the team target moves to the next group
//   (CAPTURE_REPICK when V86_CAPTURE_NEAR's table is empty). With nothing else to farm: off its tiles, stand still.
inline constexpr bool CAPTURE_MOVE_ON = true;  // with V86_CAPTURE_NEAR off: +10.6% vs v134 (3 seeds), control +8.6%
// CLAIM_HANDOFF: a collector refused a step CLAIM_STUCK turns running, off the reward, hands the claim to the visible
//   ally nearest the reward on foot (shared collector byte), else gives it back: a re-claim went to the same blocked
//   collector (10-21 stalls of 10+ turns per 130 games).
inline constexpr bool CLAIM_HANDOFF = true;
inline constexpr int CLAIM_STUCK = 2;
// PASS_REACH: the free hit on a passing cluster skips one whose reward we cannot walk to (02the-pond: Vesper killed a
//   gold cluster on an island of holes from the shore, then her claim on it stalled 10+ rounds again and again).
inline constexpr bool PASS_REACH = true;
// DASH_REACH: "can it get to the reward" counts a dash: walkable, or for a champion with a dash, reachable by hopping
//   thin holes (hop_steps). Used by every claim (leader, Vesper/Marcella/Prism funnel, self claim), the collector's
//   give-back, PASS_REACH and capture staging. 02the-pond: a funnelled Vesper claimed an island cluster no walk or
//   dash reaches (no standable tile within 2) and stood still on it 16 rounds at a time.
inline constexpr bool DASH_REACH = true;
// COLLECT_FIGHT: a fight comes before a reward: a collector gives the claim back when an enemy stands on the reward,
//   or when the team fights with an enemy near and we are not on a reward tile yet (collection ran before the fight
//   step and only an enemy in reach of us stopped it: 01gold-rush, Vesper idle 4 turns holding a claim on a tile an
//   enemy stood on while the team fought it).
inline constexpr bool COLLECT_FIGHT = true;
// HOP_WHEN_CUT: go() plans the hop route whenever walking cannot reach the goal at all and we own a dash (it only did
//   with a thin hole within HOP_NEAR of us: 01shortcut, HoR idle on a reward his dash reaches but the walk does not).
inline constexpr bool HOP_WHEN_CUT = true;
// F4 SAFE_LAND: a dash whose landing hits an area (Heart of Ruin) never lands within that area of a camp group we are
//   not farming now (static spawn tiles, so camps behind walls or in bushes count: 00great-wall monoliths). Farmable
//   now: clusters only, or the farm target / a group we already hit, when allowed (gate, monolith rules). No legal
//   landing: no dash. Area spells keep clear of hidden live groups' tiles too. Audit: 243 off-target HoR landings,
//   17 deaths and 174 camp exits in 65 games.
inline constexpr bool SAFE_LAND = true;
// SOLO_LAND (user's rule): a group is farmable for a landing only if HoR alone clears it (his strength with camp-damage
//   items, plus the landing hit) before its hits (with his camp-damage cuts) take him under FINISH_HEALTH, allies not
//   counted (they may not follow across a gap). Clusters always pass; unseen camps fail. The farm target (or a group
//   we already hit) skips the check (user): it passes on the gate and monolith rules. Off: only that exception.
//   One rule for every area hit (Bot::area_ok): HoR's landing and the area spells' clear list. The solo exception
//   never applies while awake camps already hit us, so an area never stacks a second group's damage on the first.
inline constexpr bool SOLO_LAND = true;
// F5 PRESENT_GATE: the emblem gate counts the champions present, not the roster: free part = allies (and us) within
//   PRESENT_RANGE of us, emblems = theirs only. Before the first hit on an unstarted elk/golem group a champion
//   counts who is within PRESENT_STAGE_RANGE of the group: a failed gate holds it just outside the camps' reach
//   (ladder 00dunes: Ryth took a golem alone r60-71); a passed gate still waits for STAGE_MIN allies there, at most
//   STAGE_ROUNDS rounds, so the team opens it together (audit: the others' first hit came a median 3 rounds late).
inline constexpr bool PRESENT_GATE = true;
inline constexpr int PRESENT_RANGE = 12, PRESENT_STAGE_RANGE = 8, STAGE_MIN = 2, STAGE_ROUNDS = 4;
// F2 TANK_SWAP: sturdy = max health TANK_HEALTH+ (a health gate: Prism grows into it) and over the recall bar. A hurt
//   champion farming with a sturdy ally near the group steps behind it (a sturdy ally strictly nearer to every awake
//   camp reaching the tile) and keeps hitting, leaving only under FINISH_HEALTH, with an enemy near, or when the
//   hits until the swap would take it there. A sturdy ally over TANK_TAKE_HEALTH steps strictly nearer to a camp of
//   our group whose nearest champion is a hurt or fragile ally. Ranged hold back only with no sturdy ally there.
inline constexpr bool TANK_SWAP = true;
inline constexpr double TANK_HEALTH = 1000, TANK_TAKE_HEALTH = 0.5;
// SWAP_HIT: the hurt champion stays behind only on a tile it hits a camp from (v135 bench: idle waits behind a tank,
//   then a recall anyway, lost 175-200; 56 of 628 swap runs idled 12+ turns).
inline constexpr bool SWAP_HIT = true;
// F3 TEAM_ABANDON: the leader drops the farm target while its awake camps hit a hurt ally (under BAND_HEALTH) with
//   no sturdy ally near them: the shared slot moves on, so the walk guard and camp exit take everyone out,
//   and nobody walks back in until the group is seen asleep (or ABANDON_ROUNDS pass unseen). Audit: 49% of exits
//   were hit again within 10 rounds, 1,922 of them walking back into the same farm target.
inline constexpr bool TEAM_ABANDON = false;  // alone +5.0%; with the others -4.6 to -6.2 points (3 seeds, twice)
inline constexpr int ABANDON_ROUNDS = 6;
// BANK_KEEP: an idle-bag bank walk (rule 7, target respawning) keeps going while the bag is over IDLE_BANK: walking
//   toward the altar made the respawn wait on arrival zero, so the walk flipped back to farm.wait and back (v135
//   audit: bank.walk <-> farm.wait loops, 00ambush Marcella r108-118).
// V135_SPARE_FUEL: under this much fuel left in the turn, the optional v135 work is skipped (the sturdy ally's step in
//   front, the area rule's per-group checks in aoe_scan, which then keeps every other group clear as v134 did). v135
//   doubled out-of-fuel turns on lanes / yellow-brick-road, mostly fight turns with more enemies in sight.
inline constexpr std::uint64_t V135_SPARE_FUEL = 30000;
inline constexpr bool BANK_KEEP = false;  // neutral alone (-0.5%, 3 seeds)

// ---- v137 switches: v135 + the mike branch's v135 (v134-move) and v135-golem fixes, ported fuel-light ----
// GOLEM_LAST (v135-golem): a group with a golem leaves every target table (travel: shared goal, loot, fillers,
//   invasions) and allowed() (farm()'s own picks) while a wolf / elk / gem-cluster group in reach is up within
//   GOLEM_IDLE rounds of arrival. Kept: mid-clear, reward lying there, gating every altar. Golems were 1.6% of
//   captures, ~300 champion-turns each (elk 142, wolf 60), 18% of their captures cost a camp death.
//   v142: off (user); golems are held back by GOLEM_GATE's emblems instead of by easier groups being up.
inline constexpr bool GOLEM_LAST = false;
inline constexpr int GOLEM_IDLE = 10;
// VESPER_ESCAPE (v135-golem): Vesper's hurt dodge ranks landings and steps by the enemies' walking steps (field around
//   walls and holes, at most VESPER_ESCAPE_STEPS) and their reach margin; dash first; when no step leaves every
//   enemy's reach she fires at the weakest in reach (+ attack reset). 213 of her 267 champion deaths ended in hurt.dodge.
//   v137: skipped under VESPER_ESCAPE_FUEL left (the field is ~2-3k; the old dodge then runs).
inline constexpr bool VESPER_ESCAPE = true;
inline constexpr int VESPER_ESCAPE_STEPS = 12;
inline constexpr std::uint64_t VESPER_ESCAPE_FUEL = 20000;
// TARGET_HOLD (mike v135): the leader keeps a new shared target TARGET_HOLD_ROUNDS before switching again (not when it
//   went down, the new one is mid-clear, or a kill raid / split starts): 2,249 of 12,162 target changes went back
//   within 6 rounds.
inline constexpr bool TARGET_HOLD = true;
inline constexpr int TARGET_HOLD_ROUNDS = 6;
// INVADE_OWN_KEEP (mike v135): invasions are rated against our current own-side target too (02tangle flipped every round).
inline constexpr bool INVADE_OWN_KEEP = true;
// SHARE_LOOT_TEAM (mike v135): a follower points the team at loot only with an ally within TEAM_RANGE.
inline constexpr bool SHARE_LOOT_TEAM = true;
// SHARE_LOOT_COOL (mike v135): no re-point at loot we pointed at within SHARE_LOOT_COOL_ROUNDS (the leader steered away).
inline constexpr bool SHARE_LOOT_COOL = true;
inline constexpr int SHARE_LOOT_COOL_ROUNDS = 20;
// CAPTURE_NEWS (mike v135): with no claim open, the collector byte carries CAPTURED_MARK | (round & 0x3F) and the claim
//   round byte the slot: "group s started its respawn timer at that round" (respawn records are private).
inline constexpr bool CAPTURE_NEWS = true;
inline constexpr int CAPTURED_MARK = 0x80;
inline constexpr int CAPTURED_ROUND_MASK = 0x3F;
// FINISH_TANK (mike v135): finish_ok's tank test uses the gap we will have while hitting (our reach).
inline constexpr bool FINISH_TANK = true;
// BANK_OPEN (mike v135): bank walks skip altars whose submit tiles in sight are all walls or living camps, and camp tiles
//   are no bank goal (go() seeds its field from camp tiles; 02guardians, 01shortcut walks stopped one step short).
inline constexpr bool BANK_OPEN = true;
// HOP_STICKY (mike v135): a dasher that took a hop route keeps planning hops next turn (02tangle ~400 bounces).
inline constexpr bool HOP_STICKY = true;
// CAPTURE_STAND (mike v135's CAPTURE_WAIT, unclaimed half only): a cleared, unclaimed reward is stood on, not stepped
//   off by FIX_POCKET. The claimed half (wait in a ring) is CAPTURE_STAGE's ring, which deadlocked; CAPTURE_MOVE_ON rules.
inline constexpr bool CAPTURE_STAND = true;

// ---- v138 switches: the chase and the getaway by the enemy's last step ----
// Every enemy's step between two sightings on consecutive rounds is kept (Bot::track_enemies, once a turn, no reads of
// its own). From it, the tiles an enemy will stand on (Bot::lead_path): down its walk field home when the step went
// homeward, else straight on (sliding along walls). No step known (first sighting, stood still): as before.
// CHASE_STEP: a champion target out of reach whose step we know, stepping away or sideways from us, is chased one
//   step ahead (a longer lead along its path home lost -7.9%): at the tiles reaching where that step puts it next, and among equally short
//   steps (and closing-dash landings) the one nearest that tile on the other axis. Moves cost the same in all 8
//   directions, so a target running up with us to its right is chased up-left (the gap shrinks a tile a turn), and one
//   running up-left is followed up-left until a wall stops it. Step unknown, or no move toward it: v137's chase.
inline constexpr bool CHASE_STEP = true;
// CHASE_DASH (user): in a CHASE_STEP chase the dash is never held back (VESPER_KEEP_DASH, JOIN_KEEP_DASH): any
//   dash that gains ground goes, its landing ranked the same way.
inline constexpr bool CHASE_DASH = true;
// CHASE_DASH_VESPER: CHASE_DASH for Vesper too; off, she keeps her dash in chases (VESPER_KEEP_DASH) for VESPER_ESCAPE.
inline constexpr bool CHASE_DASH_VESPER = true;  // off: -3.1% vs v137, against +9.2% on
// CATCH_ACROSS: CATCH_MODEL's "walking never closes on someone walking away" holds only when its step takes it
//   farther from us; a target whose last step kept its gap (across us) can be closed on foot, so it is catchable there.
inline constexpr bool CATCH_ACROSS = true;  // off: -0.4% vs v137, against +9.2% on
// RETREAT_LEAD: the getaway's goal also keeps out of the enemies' reach from where they will be RETREAT_LEAD_STEPS
//   of their steps on (enemies stepping toward us or sideways): not into the side a chaser is cutting off. The test of
//   being out of reach (then farm) is unchanged; a goal emptied by the lead falls back to the old one.
inline constexpr bool RETREAT_LEAD = true;  // b138x: 207-204, -0.7% banked (noise), K/D better, out-of-fuel +12%
inline constexpr int RETREAT_LEAD_STEPS = 2;

// ---- v140 switches: residual vision ----
// GHOST: the champion we last fought (Bot::mark_ghost, at engage) is not forgotten when it leaves our sight (a bush,
//   around a wall): for GHOST_ROUNDS rounds after its last sighting it may be anywhere unseen within its walk (plus
//   GHOST_SLACK for a dash) of where we last saw it, and most likely where its last step leads (Bot::lead_path:
//   straight on, sliding along walls, or down its walk field home). With no enemy near, we walk to the unseen tiles
//   around that guess (Bot::chase_ghost). Dropped once it is seen dead, every tile it could be on is in our sight,
//   the guess is in its home chase ring, the team chase is off, or we are under GHOST_HEALTH.
inline constexpr bool GHOST = true;
inline constexpr int GHOST_ROUNDS = 5;  // b140: 3 / 4 / 5 / 6 rounds +8.6 / +9.0 / +9.9 / +4.3% vs v138
inline constexpr int GHOST_SLACK = 1;
inline constexpr double GHOST_HEALTH = 0.6;  // 0.4: +9.9%, 0.6: +10.0% (kept: same score, no hurt face-checks)
// GHOST_NEAR: only a target we fought within this many tiles is remembered (farther, it walks out of our 8-tile sight
//   rather than hiding). Willow (backline) never chases one.
inline constexpr int GHOST_NEAR = 99;  // 6: +3.1% against +9.0% at 99
// GHOST_WARD: the ward (no action) goes onto the unseen tile in placement reach nearest the guess, before the turn's
//   other ward use, so the fight logic sees what it reveals this turn.
inline constexpr bool GHOST_WARD = true;  // off: +6.2% against +9.0% on
// GHOST_DASH: the ghost chase may dash (route rules); off, it walks.
inline constexpr bool GHOST_DASH = false;  // on: +8.4% against +9.0% off
// GHOST_JOIN: the ghost is chased only with an ally in sight within FIGHT_LOCAL_RANGE (as JOIN_FIRST: not alone).
inline constexpr bool GHOST_JOIN = false;  // on: +7.3% against +9.9% off
#include <cstdint>
#include <span>

namespace bc = cambc;
using bc::vec2;
using Team = bc::team;
using Champ = bc::champion_type;
using Kind = bc::ability_kind;
using Item = bc::item_type;

// ---------------------------------------------------------------------------------------------------------------------
// Builds, in roster order. The shop buys one family at a time until its emblem is done (a family listed twice wants a
// second emblem), then the fallback shards without limit. Each champion has its own pair, so a change touches only it.
// v134 (PROSPECTOR_SWAP): on maps with few clusters the shop swaps Prospector shards for Treasure (Power if the build
// already has Treasure), keeping the stat.
// ---------------------------------------------------------------------------------------------------------------------
// Cassi: v83 RANGED_FOCUS.
inline constexpr Item CASSI_BUILD[] = {Item::treasure_shard_of_focus, Item::monster_shard_of_focus,
    Item::durability_shard_of_focus};
inline constexpr Item CASSI_FALLBACK[] = {Item::power_shard_of_focus};
// Issac: v83 MELEE_STRENGTH.
inline constexpr Item ISSAC_BUILD[] = {Item::treasure_shard_of_strength, Item::jungle_shard_of_strength,
    Item::durability_shard_of_health};
inline constexpr Item ISSAC_FALLBACK[] = {Item::power_shard_of_strength, Item::power_shard_of_health};
// Ryth: the passive turns bonus health into Strength, and Jungle cuts the camp hits he stands in.
inline constexpr Item RYTH_BUILD[] = {Item::jungle_shard_of_health, Item::monster_shard_of_health,
    Item::durability_shard_of_health};
inline constexpr Item RYTH_FALLBACK[] = {Item::bruiser_shard_of_health};
// Vesper Vale: her kit is all basic attacks, so Strength; Treasure first (on top of her 33%), bought on her first trip.
inline constexpr Item VESPER_BUILD[] = {Item::treasure_shard_of_strength, Item::monster_shard_of_strength,
    Item::hunter_shard_of_strength, Item::skirmish_shard_of_strength,
    Item::home_shard_of_strength, Item::durability_shard_of_health,
    Item::collectors_shard_of_strength, Item::bruiser_shard_of_health,
    Item::jungle_shard_of_strength, Item::tribute_shard_of_health,
    Item::capture_shard_of_strength};
inline constexpr Item VESPER_FALLBACK[] = {Item::power_shard_of_health, Item::power_shard_of_strength,
    Item::power_shard_of_strength};
// Prism: v123.
inline constexpr Item PRISM_BUILD[] = {Item::treasure_shard_of_focus, Item::monster_shard_of_focus,
    Item::durability_shard_of_health, Item::prospector_shard_of_focus,
    Item::home_shard_of_health, Item::tribute_shard_of_focus,
    Item::collectors_shard_of_focus, Item::jungle_shard_of_health};
inline constexpr Item PRISM_FALLBACK[] = {Item::power_shard_of_focus, Item::power_shard_of_health};
// Stellar: v70 Power Focus first; now Treasure, Monster, Bruiser Health.
inline constexpr Item STELLAR_BUILD[] = {Item::treasure_shard_of_focus, Item::monster_shard_of_focus,
    Item::bruiser_shard_of_health};
inline constexpr Item STELLAR_FALLBACK[] = {Item::power_shard_of_focus};
// Vendetta.
inline constexpr Item VENDETTA_BUILD[] = {Item::monster_shard_of_strength, Item::jungle_shard_of_strength,
    Item::skirmish_shard_of_health, Item::capture_shard_of_strength, Item::durability_emblem_of_health,
    Item::treasure_shard_of_strength, Item::power_shard_of_strength, Item::hunter_shard_of_health,
    Item::collectors_shard_of_strength, Item::home_shard_of_strength};
inline constexpr Item VENDETTA_FALLBACK[] = {Item::bruiser_shard_of_strength};
// Marcella: v110 tanks camps and hits with Strength; she gains 2 gold per gem tributed.
inline constexpr Item MARCELLA_BUILD[] = {Item::treasure_shard_of_strength, Item::monster_shard_of_strength,
    Item::durability_shard_of_health, Item::tribute_shard_of_strength,
    Item::jungle_shard_of_health, Item::skirmish_shard_of_strength,
    Item::collectors_shard_of_strength, Item::prospector_shard_of_health};
inline constexpr Item MARCELLA_FALLBACK[] = {Item::power_shard_of_strength, Item::power_shard_of_health};
// Tavi: never drafted.
inline constexpr std::span<const Item> TAVI_BUILD{}, TAVI_FALLBACK{};
// Willow: v83 Durability first (she heals and stays alive), then Power.
inline constexpr Item WILLOW_BUILD[] = {Item::monster_shard_of_focus, Item::durability_shard_of_focus};
inline constexpr Item WILLOW_FALLBACK[] = {Item::power_shard_of_health, Item::power_shard_of_focus};
// Ren: v83 Jungle Health first (camps hit him 30% less while he tanks them), then Durability; Bruiser Focus after.
inline constexpr Item REN_BUILD[] = {Item::jungle_shard_of_health, Item::durability_shard_of_health};
inline constexpr Item REN_FALLBACK[] = {Item::bruiser_shard_of_focus};
// Varg. VARG_MARCELLA_BUILD: he buys Marcella's build and fallback instead (v113: 12-14 vs 14-12).
inline constexpr bool VARG_MARCELLA_BUILD = false;
inline constexpr Item VARG_BUILD[] = {Item::prospector_shard_of_focus, Item::jungle_shard_of_strength,
    Item::monster_shard_of_health, Item::capture_shard_of_health, Item::tribute_shard_of_strength};
inline constexpr Item VARG_FALLBACK[] = {Item::power_shard_of_strength, Item::power_shard_of_health};
// Heart of Ruin.
inline constexpr Item HOR_BUILD[] = {Item::capture_shard_of_focus, Item::jungle_shard_of_focus,
    Item::monster_shard_of_health, Item::durability_shard_of_health};
inline constexpr Item HOR_FALLBACK[] = {Item::bruiser_shard_of_focus};

// v158 Prism team builds: Prism tanks; the rest open with Prospector and follow their build less Monster and Jungle.
inline constexpr Item PRISM_TEAM_PRISM[] = {Item::durability_shard_of_health, Item::prospector_shard_of_health,
    Item::tribute_shard_of_health};
inline constexpr Item PRISM_TEAM_PRISM_FALLBACK[] = {Item::power_shard_of_health};
inline constexpr Item PRISM_TEAM_HOR[] = {Item::prospector_shard_of_focus, Item::capture_shard_of_focus,
    Item::durability_shard_of_health};
inline constexpr Item PRISM_TEAM_MARCELLA[] = {Item::prospector_shard_of_health,
    Item::durability_shard_of_health, Item::tribute_shard_of_strength, Item::skirmish_shard_of_strength,
    Item::collectors_shard_of_strength};
inline constexpr Item PRISM_TEAM_RYTH[] = {Item::prospector_shard_of_health, Item::durability_shard_of_health};
inline constexpr Item PRISM_TEAM_VENDETTA[] = {Item::prospector_shard_of_strength, Item::skirmish_shard_of_health,
    Item::capture_shard_of_strength, Item::durability_shard_of_health,
    Item::power_shard_of_strength, Item::hunter_shard_of_health, Item::collectors_shard_of_strength,
    Item::home_shard_of_strength};

inline constexpr Item PRISM_TEAM_WILLOW[] = {Item::prospector_shard_of_focus, Item::durability_shard_of_focus};

inline std::span<const Item> prism_team_order(Champ kind) {
    switch (kind) {
    case Champ::prism: return PRISM_TEAM_PRISM;
    case Champ::heart_of_ruin: return PRISM_TEAM_HOR;
    case Champ::marcella: return PRISM_TEAM_MARCELLA;
    case Champ::ryth: return PRISM_TEAM_RYTH;
    case Champ::vendetta: return PRISM_TEAM_VENDETTA;
    case Champ::willow: return PRISM_TEAM_WILLOW;
    default: return {};
    }
}

inline std::span<const Item> shop_order(Champ kind) {
    switch (kind) {
    case Champ::cassi: return CASSI_BUILD;
    case Champ::issac: return ISSAC_BUILD;
    case Champ::ryth: return RYTH_BUILD;
    case Champ::vesper_vale: return VESPER_BUILD;
    case Champ::prism: return PRISM_BUILD;
    case Champ::stellar: return STELLAR_BUILD;
    case Champ::vendetta: return VENDETTA_BUILD;
    case Champ::marcella: return MARCELLA_BUILD;
    case Champ::tavi: return TAVI_BUILD;
    case Champ::willow: return WILLOW_BUILD;
    case Champ::ren: return REN_BUILD;
    case Champ::varg: return VARG_MARCELLA_BUILD ? std::span<const Item>(MARCELLA_BUILD) : VARG_BUILD;
    case Champ::heart_of_ruin: return HOR_BUILD;
    default: return {};
    }
}

inline std::span<const Item> shop_fallback(Champ kind) {
    switch (kind) {
    case Champ::cassi: return CASSI_FALLBACK;
    case Champ::issac: return ISSAC_FALLBACK;
    case Champ::ryth: return RYTH_FALLBACK;
    case Champ::vesper_vale: return VESPER_FALLBACK;
    case Champ::prism: return PRISM_FALLBACK;
    case Champ::stellar: return STELLAR_FALLBACK;
    case Champ::vendetta: return VENDETTA_FALLBACK;
    case Champ::marcella: return MARCELLA_FALLBACK;
    case Champ::tavi: return TAVI_FALLBACK;
    case Champ::willow: return WILLOW_FALLBACK;
    case Champ::ren: return REN_FALLBACK;
    case Champ::varg: return VARG_MARCELLA_BUILD ? std::span<const Item>(MARCELLA_FALLBACK) : VARG_FALLBACK;
    case Champ::heart_of_ruin: return HOR_FALLBACK;
    default: return {};
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// v134 item passives (passives.h reads each champion's finished emblems from the emblem reads we already make).
// ---------------------------------------------------------------------------------------------------------------------
// ITEM_DAMAGE: camp and champion damage math counts Monster (+15% to camps), Skirmish (+5% to champions), Collectors
//   (+1% per unique finished family), Jungle (-30% from camps) and Durability (-10% from all; stacked by multiplying).
inline constexpr bool ITEM_DAMAGE = true;
// ITEM_CHANNELS: recall is 3 ticks with Home; tribute ticks are cut by a third with Tribute (escorts judged by ticks).
inline constexpr bool ITEM_CHANNELS = true;
inline constexpr int ESCORT_TICKS = 15;
// FIGHT_ITEM_POWER: the fight score's power and health edges compare item-adjusted sides (damage dealt to champions
//   x Skirmish/Collectors, health / Durability's damage taken).
inline constexpr bool FIGHT_ITEM_POWER = true;
// FIGHT_SKIRMISH / FIGHT_HUNTER: + per ally, - per enemy owning the emblem (weighted like the bodies).
inline constexpr bool FIGHT_SKIRMISH = true;
inline constexpr bool FIGHT_HUNTER = true;
inline constexpr double SKIRMISH_SCORE = 1.0;
inline constexpr double HUNTER_SCORE = 0.5;
// CAPTURE_HELP: a Capture emblem owner near an ally's capture steps onto the reward until the timer is cut to 3, then
//   steps off so the collector gets the whole reward. Only within CAPTURE_HELP_STEPS walking steps, with the capturing
//   ally within CAPTURE_HELP_ALLY tiles; never with an enemy near, carrying, or under RECALL_HEALTH.
inline constexpr bool CAPTURE_HELP = true;
inline constexpr int CAPTURE_HELP_STEPS = 3;
inline constexpr int CAPTURE_HELP_ALLY = 6;
inline constexpr int CAPTURE_EMBLEM_TIME = 3;
// CAPTURE_PRICING: the route prices a capture at CAPTURE_EMBLEM_TIME while a living ally owns a Capture emblem.
inline constexpr bool CAPTURE_PRICING = true;
// PROSPECTOR_SWAP: under PROSPECTOR_MIN_CLUSTERS cluster camps (gem + gold) on the map, Prospector shards become
//   Treasure shards (Power if the build already has Treasure), same stat.
inline constexpr bool PROSPECTOR_SWAP = true;
inline constexpr int PROSPECTOR_MIN_CLUSTERS = 20;

// Turn thresholds.
inline constexpr double RECALL_HEALTH = 0.35;
// v117 (RECALL_CANCEL): a recall started for low health is cancelled once health is back to RECALL_CANCEL_HEALTH (a
//   Willow heal, say) unless the purse reaches the shop bar (RECALL_GOLD): then it goes on to shop. Only Willow heals,
//   and she skips recalling allies (WILLOW_HEAL_SKIP_RECALL), so it needs that off. Off: at 0.6 with Willow healing
//   recallers, 3 runs vs v116 = 42-38, 39-41, 38-42 (119/240).
inline constexpr bool RECALL_CANCEL = false;
inline constexpr double RECALL_CANCEL_HEALTH = 0.6;
// A champion under this max health is squishy, and fights less while ahead of every melee ally.
inline constexpr double SQUISHY_HEALTH = 900;
inline constexpr int GEM_BANK = 40;
inline constexpr int WIN_GEMS = 1000;
inline constexpr int THREAT_RANGE = 4;
// While the target is still respawning, bank any bag at least this big.
inline constexpr int IDLE_BANK = 10;
// Recall to shop after a tribute when carrying this much gold.
inline constexpr int RECALL_GOLD = 150;
// Vesper collects most loot, so her purse fills fast; a higher bar keeps her with the team.
// v94: only once her core build is done. Before that she recalls at 150 for the Treasure emblem, then once her purse
// covers the other three core emblems (vesper_recall_gold).
inline constexpr int VESPER_RECALL_GOLD = 600;
// v106 (VESPER_EMBLEM_RECALL): after her core Vesper shops at every emblem's worth (RECALL_GOLD) like everyone else.
inline constexpr bool VESPER_EMBLEM_RECALL = true;
// v70 Ren: recalls to shop at every emblem's worth of gold, and banks only once his bag reaches the cap.
inline constexpr int REN_RECALL_GOLD = 150;
inline constexpr int REN_BANK_GEMS = 150;

// Route targeting: a new planned goal must beat the current one by this factor.
inline constexpr double SWITCH_MARGIN = 1.25;
// Route-group walking fields are built from fuel left at turn end, only while at least this much remains.
// C++ gets 100k fuel per turn (Python 2.5M), so this is the Python 1M reserve scaled by 1/25.
inline constexpr std::uint64_t FIELD_FUEL_RESERVE = 40'000;
// Wait this far from a respawning group; standing on its spawn tiles delays it.
inline constexpr int WAIT_RANGE = 2;
// v98: holes can be dashed over (walls cannot), but paths only ever walked: on concentrated-wealth the team walked
// ~100 steps round a wall each trip where one dash over a one-tile row of holes would do, and on islands most groups
// sit across holes. With FIX_HOP a champion with a dash (Vesper, Ren, Issac, Vendetta, Heart of Ruin; not Varg)
// paths through "thin" holes (a hole with standable ground on both opposite sides, so one 2-tile dash crosses it)
// when that saves at least HOP_SAVE steps plus the dash's remaining cooldown, or walking cannot get there at all. It
// walks to the edge, dashes across when the dash is ready, and otherwise waits at the edge.
inline constexpr bool FIX_HOP = true;
// v99: the first turn ends after setup unless this much fuel is left to play it (see play()).
inline constexpr std::uint64_t SETUP_PLAY_FUEL = 60000;
// v98: on boss-battle the only altar is set in a wall and its one standable neighbour is a monolith spawn tile:
// carriers walked at it all game (1,415 bank.walk turns, ~5 tributes) and the first bank came at ~r1,100. With
// FIX_ALTAR_GATE an altar whose every standable neighbour is a camp spawn tile is gated by that group, and while
// every altar is gated shut the gate group is the team's goal once the monolith rules allow it (MONOLITH_EMBLEMS,
// grouped) without waiting for it to be the last resort. Banking is unchanged: carriers still walk to the shut
// altar, so they stand beside it when it opens (skipping it to farm on banked nothing in two test games).
inline constexpr bool FIX_ALTAR_GATE = true;
// The gate counts as shut only while we saw its group up within GATE_MEMORY rounds (the enemy may kill it unseen).
inline constexpr int GATE_MEMORY = 30;
// v151-f4 (SEALED_DROP): groups our spawn cannot walk to with every monolith tile blocked are dropped from every pick
//   (travel) while a monolith is up (the GATE_MEMORY rule of altar_open). The walk fields treat camp tiles as open, so
//   on 01boss-battle (the monolith pair is the only way between the halves) the leader's invasion target lay across it:
//   all five idled "boxed in" from ~r300 to r2000, earned no gold, so never reached MONOLITH_EMBLEMS_5 to open the altar.
inline constexpr bool SEALED_DROP = true;
// v151-f4 (GATE_TEAM): while every altar's gate group is not known to be down (gates_closed: respawning or its reward
//   waiting; unseen counts as up, unlike altar_open's GATE_MEMORY), the gate monolith is the team's goal once strong
//   and grouped, and a win_tribute bag does not hold the team (r800-2000 every champion held a bag no altar took).
inline constexpr bool GATE_TEAM = true;
// v156 (SHOP_NO_TRIBUTE): a champion at the shop bar waited for a tribute first, so where nobody can tribute nobody
//   shopped: 00contest (gem-starved) 94-0 kills, 0-0 score, ~24k gold unspent; 02guardians (both altars gated)
//   Vesper sat on 2.7k gold. Team emblems never reached monolith_emblems(), so the monolith never opened. It now
//   shops without a tribute on a gem-starved map or while every altar is gated shut; SHOP_EMPTY_BAG also with an
//   empty bag (no tribute can come before its next gem; this one changes every map).
inline constexpr bool SHOP_NO_TRIBUTE = true;
inline constexpr bool SHOP_EMPTY_BAG = true;
// v156 (GATE_ALL_GROUPS): setup kept one gate group per altar (the lowest tile rank); 02guardians' altar is reached
//   only from two monolith tiles, and killing either opens it. Every group on an altar's submit tiles is now a gate.
inline constexpr bool GATE_ALL_GROUPS = true;
// v156 (GATE_GROUP_LOOSE): a gate monolith shutting every altar starts under the gem-starved rule (MONOLITH_KEEP + 1
//   allies near, not every living ally) and keeps going while the team targets it: like a gem-starved map, it is the
//   only way to score. 02guardians: monolith_grouped held all 4 bench games at 0-0.
inline constexpr bool GATE_GROUP_LOOSE = true;
// v156 (MONOLITH_GATHER): drop_monoliths kept a monolith as the goal only once the team was grouped around it, and
//   the team only groups on its goal, so a farming team never did (02guardians: 1-2 of 4 near on every check). The
//   only monolith that scores (sole_monolith: gem-starved map, or a gate shutting every altar) now stays the goal
//   once strong; the hits still wait for allowed()'s grouping (the committed rule: MONOLITH_KEEP allies near).
inline constexpr bool MONOLITH_GATHER = true;
// v157 (MONO_MEMORY): a monolith keeps its damage until it is captured and respawns (probe: 02guardians s11 stayed at
//   15,205 from r956 to r1190; boss-battle 11,328 r865-r918), but mid_clear() only knew of damage in sight, so a team
//   that recalled off a half-dead monolith treated it as fresh (full emblem gate, last resort). Each champion now keeps
//   the health share it last saw on every monolith (own vision; the shared bytes are all taken). A monolith at
//   MONO_DAMAGED of its health or less:
//   - (MONO_RESUME) is a stop even while other groups are up (like mid_clear), and the team heads there ungrouped
//     (like MONOLITH_GATHER; the hits still wait for allowed()'s grouping);
//   - (MONO_SCALE) needs only that share of monolith_emblems() (ceil), and route_rate_steps prices only that share of
//     its clear time, so a monolith at 30% rates as 30% of the work for the full reward.
// v158: MONO_SCALE off (v157 bench: it only fired on enemy-damaged monoliths, 01boss-battle 0-2 vs v156).
inline constexpr bool MONO_MEMORY = true, MONO_RESUME = true, MONO_SCALE = false;
inline constexpr double MONO_DAMAGED = 0.9;
// v158 (MONO_OWN_ONLY): MONO_RESUME only for damage we saw dealt with an ally near it and no enemy (an enemy-damaged
//   monolith in the contested centre drew the team in short of emblems: v157 01boss-battle).
inline constexpr bool MONO_OWN_ONLY = true;
// v158 (MONO_SESSIONS): ladder winners take a monolith in sessions (a ~30-round burst, sometimes 2-3 with breaks;
//   first captures r405-r1132 at 12-37 team emblems, one tank, 1-2 capturers; the earlier "0-4 emblems, 00vault
//   r55/r107" figure counted 00vault's gem clusters, and 00vault has no monolith). A monolith hits only the nearest
//   champion (200 a hit, reach 5), so a tank takes every hit while the rest hit it from behind, and its damage stays
//   when the team leaves (MONO_MEMORY). The emblem gate (MONOLITH_EMBLEMS_5/4/3) is replaced by a session estimate
//   over the champions within PRESENT_RANGE of us (mono_session): damage a round D (Strength with resets, plus damage
//   spells over their 8-round cooldown, Monster/Collectors) and the rounds R the tank pool lasts (the champions'
//   health over the RECALL_HEALTH share, less one MONO_WALK_OUT for the last one out, at 200 a hit less Willow's heal
//   over 8 rounds, Jungle/Durability). A start (or resume) needs D x R >= min(health left, MONO_SESSION_MIN) and
//   MONO_START_NEAR champions near (monolith_grouped); a session in progress keeps the goal while R >= MONO_KEEP_ROUNDS,
//   then the goal moves off it and CAMP_EXIT_ALWAYS walks everyone out (each champion's own swap/recall rules as
//   before); MONO_RESUME brings the team back. MONO_SIDE (v161) puts the emblem gate back on contested monoliths.
inline constexpr bool MONO_SESSIONS = true;
// v159: 3000 -> 2000 (v158's first capture median r739; ladder winners' first captures median ~r730).
inline constexpr double MONO_SESSION_MIN = 2000, MONO_WALK_OUT = 600, MONO_KEEP_ROUNDS = 2;
inline constexpr int MONO_START_NEAR = 3;
// A session passed within MONO_SESSION_GAP rounds keeps going on MONO_KEEP_ROUNDS; otherwise the start rule applies.
inline constexpr int MONO_SESSION_GAP = 5;
// v158: only sturdy champions (TANK_HEALTH max health) count in the tank pool, every monolith of the group counts
//   (health and hits), and a champion carrying MONO_BAG gems or more joins no session (a camp kill destroys the bag)
//   unless no altar can take it (gem-starved, or every altar gated).
inline constexpr int MONO_BAG = 40;
// v159 (MONO_COVER): a fragile champion (under TANK_HEALTH max health) keeps strictly farther from an awake monolith
//   than the nearest sturdy ally in its reach, holding outside our reach rather than level with the tank (v158 bench:
//   Willow 18 camp deaths vs v156's 5). With no sturdy ally there it may hit, and MONO_LEAVE takes it out.
// v160: off. Ablation on the 10 monolith maps vs v155+v152 (40 games): without it 25 wins vs 19, 47 monoliths vs 36,
//   gems destroyed by camps 218 vs 866, 00contest 2 vs 0 (its hold branch idled Willow and Prism out of reach).
inline constexpr bool MONO_COVER = false;
// v159 (MONO_LEAVE): whoever an awake monolith hits (the nearest) leaves once its health is at most the hits it takes
//   before an ally holding its tile is the nearer (or before it is out of reach), plus MONO_LEAVE_MARGIN hits; the next
//   one in line, a carry too, hits on and leaves on the same rule. Out of reach it recalls when nothing breaks it and
//   stays off the monolith until MONO_BACK_HEALTH (or the spawn). Was: the 35% recall bar (v158: Ryth 14, HoR 11
//   camp deaths vs 9 and 7). The session pool counts each champion's health over MONO_POOL_HITS hits.
inline constexpr bool MONO_LEAVE = true;
inline constexpr int MONO_LEAVE_MARGIN = 1;
inline constexpr double MONO_BACK_HEALTH = 0.9, MONO_POOL_HITS = 3;
// v159 (MONO_ENEMY_END): an enemy champion in sight ends our session (the goal moves off the monolith); the fight
//   decision runs as usual, and (MONO_FIGHT_OUT) a fight keeps out of every awake monolith's reach: we step out first,
//   and close on the enemy around it (v158 01boss-battle: carriers killed beside the monolith after the capture).
//   Not for the only monolith that scores (sole_monolith): enemies are always near it (00contest A: 0 captures).
inline constexpr bool MONO_ENEMY_END = true, MONO_FIGHT_OUT = true;
// v161 (MONO_SIDE): early sessions only on monoliths clearly on our side (MONO_SIDE_MARGIN walking steps nearer our
//   spawn than theirs) with four or more champions; a contested one (or any on a team of three or fewer) also needs the
//   team emblem gate (monolith_emblems) to start or resume. v160 vs v156: 00anvil (3 champions, contested) and
//   01boss-battle (the pair is the only way between the halves) 0-2 each, deaths to enemy champions around them;
//   ladder winners take monoliths at 12-37 team emblems (r405-r1132).
inline constexpr bool MONO_SIDE = true;
inline constexpr int MONO_SIDE_MARGIN = 3;
inline constexpr double MONOLITH_HEALTH = 20000;
// v158 (MONO_EARLY): a monolith the session estimate allows is a stop like any group (rated by route_rate_steps)
//   instead of a last resort, and the team gathers on it ungrouped (as MONOLITH_GATHER).
inline constexpr bool MONO_EARLY = true;
inline constexpr int HOP_SAVE = 10;
// The hop field is a second distance field per move, so it is skipped when it cannot pay: a walk under HOP_SAVE + 2
// steps, no thin hole within HOP_NEAR tiles, or under HOP_FUEL fuel left this turn. Ungated, out-of-fuel turns rose
// 65% over v97.
inline constexpr std::uint64_t HOP_FUEL = 30000;
inline constexpr int HOP_NEAR = 12;
// v103: the hop field counted steps only, so routes took 2-3 hops where 1 would do, and a ready dash went on a plain
// 2-step gain just before a hole: each extra hop then waited out the 8-turn cooldown at the next edge (islands v103
// draft: 226 edge-wait turns a game for three dashers, 43 of 48 waits right after a hop). With FIX_HOP_FEW the route
// keeps to the fewest hops (layers built from the goals, HOP_LAYERS deep, cached for HOP_CACHE goal boards, built only
// with HOP_LAYER_FUEL spare) unless any-hop saves more than HOP_EXTRA steps, and the hop route spends the dash on
// plain ground only with no route hole within HOP_KEEP of the landing.
inline constexpr bool FIX_HOP_FEW = true;
inline constexpr int HOP_LAYERS = 4;
inline constexpr std::size_t HOP_CACHE = 6;
inline constexpr std::uint64_t HOP_LAYER_FUEL = 20000;
inline constexpr int HOP_EXTRA = 8;
inline constexpr int HOP_KEEP = 8;
// v97: on islands a gold-cluster pair sits in a pocket ringed by holes. Ranged hits and splash clear it from outside,
// so its reward kept coming back, and the claim went to one walker after another, idle at the edge all game
// (920 collect turns "boxed in", one champion in five). With FIX_WALK_CLAIM a claim only goes to an ally with a
// walking path to the reward, and a collector without one gives it back unless it can dash and is within
// WALK_CLAIM_DASH of the reward. A funnelled Vesper still claims nearby loot herself (vesper_claim) and dashes in.
// Letting any dasher take pocket claims (walking to dash range first) cost islands ~600 gems a game: 30+ pockets.
inline constexpr bool FIX_WALK_CLAIM = true;
inline constexpr int WALK_CLAIM_DASH = 3;
// v97: the same pockets as targets: a dashing leader picks a group the walkers cannot reach (islands: 410 turns of
// "walk to s42" boxed in). With FIX_WALK_TARGET a champion with no walking path to the target (and no dash within
// WALK_CLAIM_DASH) farms the best group it can walk to and that is up, with a load (strength x health) of at most
// WALK_TARGET_LOAD: one or two wolves, an elk, clusters.
inline constexpr bool FIX_WALK_TARGET = true;
inline constexpr double WALK_TARGET_LOAD = 150000;
// v103: a dasher that went into a hole pocket (to collect, bank at a pocket altar, or chase a camp) found no walking
// path back to the team's target, so FIX_WALK_TARGET gave it the pocket's own group instead, and it waited out every
// respawn there: islands v102 mirror, Vesper ~700 turns a game in pockets (two thirds of them respawn waits), Heart of
// Ruin ~570, Ren ~300, while the walkers could not come. With FIX_POCKET a dasher off the main island (where our spawn
// walks) still farms a pocket group that is up, but never waits on one: with nothing up it keeps the team's target,
// and the hop route dashes it back out. When hopping there takes POCKET_RECALL_SLACK more steps than recalling and
// walking from spawn (or no hop gets there), it recalls out instead, if no enemy or camp can break the recall.
inline constexpr bool FIX_POCKET = true;
inline constexpr int POCKET_RECALL_SLACK = 5;
// Every group is a possible stop on the way, except monoliths until the team owns this many emblems.
inline constexpr int MONOLITH_EMBLEMS = 12;
// v106 (MONOLITH_BY_SIZE): the team must own MONOLITH_TEAM_EMBLEMS[team size] emblems (15 on five, 18 on four, 20 on
// three) before anyone hits a monolith, so the team arrives grouped and fed; MONOLITH_EMBLEMS when off.
inline constexpr bool MONOLITH_BY_SIZE = true;
inline constexpr int MONOLITH_EMBLEMS_5 = 15, MONOLITH_EMBLEMS_4 = 18, MONOLITH_EMBLEMS_3 = 20;
// A monolith is a last resort: it is only a target while every other group in reach stays down for
// more than MONOLITH_IDLE rounds after we would get there. It is started only with every living ally
// within MONOLITH_GROUP_RANGE, each at MONOLITH_START_HEALTH of max health, and no enemy in sight. Once
// under attack the team stays on it while at least MONOLITH_KEEP allies (or every living one) remain.
inline constexpr int MONOLITH_IDLE = 10;
inline constexpr int MONOLITH_GROUP_RANGE = 6;
inline constexpr double MONOLITH_START_HEALTH = 0.8;
inline constexpr int MONOLITH_KEEP = 3;
// Visible loot this close is taken even if it is slightly off the path to the planned goal.
inline constexpr int NEAR_LOOT = 8;
// Extra walking tiles allowed for a stop to still count as being on the way.
inline constexpr int ON_WAY_SLACK = 4;
// A champion back from spawn rejoins the team once this close to the shared target; until then it can't lead.
inline constexpr int TEAM_RANGE = 8;
// A capture claim is dropped after this many rounds so a dead or lost collector gets replaced.
inline constexpr int CAPTURE_TIMEOUT = 15;
// Vesper's core build is four emblems; fallback purchases continue after the core is complete.
// v94: loot is funnelled to Vesper until she owns all four (vesper_funnel, vesper_claim); the team turns aggressive
// once she does and the team owns TEAM_AGGRESSIVE_EMBLEMS.
inline constexpr int VESPER_CORE_EMBLEMS = 4;
// v106: loot keeps going to Vesper (she collects first when near) until she owns VESPER_FUNNEL_EMBLEMS emblems
// (v105: VESPER_CORE_EMBLEMS); the aggressive gate still opens at her core build.
inline constexpr int VESPER_FUNNEL_EMBLEMS = 12;
// v165 (VESPER_GOLD_ONLY): no first claim for Vesper on gold-less (gem-only) rewards; the nearest, poorest ally takes them.
//   Ladder 2026-10-31: 25% of our Vesper's captures were gem clusters (Fish's: 8%); she does 21% of our camp damage and
//   takes 57% of captures (Fish's Vesper 38% / 42%).
inline constexpr bool VESPER_GOLD_ONLY = true;
// v165 (COLLECT_SHOOT): a collector on its reward tile basic-attacks the weakest awake camp within its attack range.
inline constexpr bool COLLECT_SHOOT = false;
// v70: Ren rushes REN_CORE_EMBLEMS: he recalls at every emblem's worth of gold (before banking) and banks like
// everyone else. From REN_CORE_EMBLEMS on, Ren banks only at REN_BANK_GEMS, stuns enemy champions before camps, and
// takes a fight only with an ally this close. v94: the loot funnel and the aggressive gate moved to Vesper.
inline constexpr int REN_CORE_EMBLEMS = 2;
inline constexpr int TEAM_AGGRESSIVE_EMBLEMS = 4;
inline constexpr int REN_FIGHT_ALLY_RANGE = 5;
// Vesper takes over a capture claim herself when the cleared group is this close to her.
inline constexpr int VESPER_CLAIM_RANGE = 8;
inline constexpr int CLAIM_ROUND_OFFSET = 6;

// v71 switches, one per fix, so each can be tested alone.
// Camp hits do not stop a capture: a collector at or above COLLECT_CAMP_HEALTH holds its reward under camp
// fire (only an enemy champion in reach sends it away), stands on a reward tile out of awake camp reach when
// the group has one, and drops the claim when an ally is already capturing the reward.
inline constexpr bool FIX_CAMP_COLLECT = true;
inline constexpr double COLLECT_CAMP_HEALTH = 0.5;
// The claim goes to the ally closest to the reward (CLAIM_WORTH_PER_TILE of item worth counts as one tile,
// so the poorer of two near allies still wins), never to one about to recall; Ren takes it only within
// VESPER_CLAIM_RANGE. A dead collector's claim is dropped at once, and a collector driven off by an enemy
// gives the claim back so the next free ally can take it.
inline constexpr bool FIX_CLAIM = true;
inline constexpr int CLAIM_WORTH_PER_TILE = 150;
// Damage nobody visible can have dealt comes from an enemy hidden in a bush: the ward goes into the nearest
// unseen bush in reach to reveal it; otherwise the champion steps out of reach of every unseen bush nearby,
// and a collector gives up its claim. Remembered for HIDDEN_ROUNDS rounds. Spells reach 4, basic attacks 3.
inline constexpr bool FIX_HIDDEN = true;
inline constexpr int HIDDEN_ROUNDS = 2;
inline constexpr int HIDDEN_REACH = 4;
// Camp strength by kind (GameRules table): a camp inside a bush hits us unseen, so its spawn tile counts.
// v94: any kind this SDK does not name is the balance patch's golem (120 strength, reach 4).
inline int camp_kind_strength(bc::camp_type kind) {
    switch (kind) {
    case bc::camp_type::wolf: return 20;
    case bc::camp_type::elk: return 60;
    case bc::camp_type::monolith: return 200;
    case bc::camp_type::gem_cluster: case bc::camp_type::gold_cluster: return 0;
    default: return 120;
    }
}
// v94 (balance patch): reach is per kind, wolf 3, elk 4, golem 4, monolith 5 (was 3 for every camp). A live camp's
// attack_range() is read where one is in sight; this is for camps out of sight.
inline constexpr int MONOLITH_REACH = 5;
inline int camp_kind_reach(bc::camp_type kind) {
    switch (kind) {
    case bc::camp_type::wolf: return 3;
    case bc::camp_type::elk: return 4;
    case bc::camp_type::monolith: return MONOLITH_REACH;
    case bc::camp_type::gem_cluster: case bc::camp_type::gold_cluster: return 0;
    default: return 4;
    }
}
// A ranged champion opens a camp without waiting when a sturdy ally (melee, or over SQUISHY_HEALTH) already
// stands inside the camp's reach and closer to it: the camp hits the nearest champion, so that ally tanks.
inline constexpr bool FIX_ABSORB = true;
// A target whose every reward tile already has an ally capturing it needs nobody else: the rest move on.
inline constexpr bool FIX_TAKEN = true;
// A champion off on its own (over TEAM_RANGE from the target with no ally within TEAM_RANGE) leaves the
// shared decision to someone with the team while the last one is fresh: it would score the team's mode
// and pick its target and captures from what only it can see (Ren, acting first, chasing far away).
inline constexpr bool FIX_LEAD = true;
// Vision is not shared, so with no claim open a champion that sees a reward within SELF_CLAIM_RANGE, with
// no visible ally nearer to it and no enemy near, claims it itself instead of waiting for the leader.
inline constexpr bool FIX_SELF_CLAIM = true;
inline constexpr int SELF_CLAIM_RANGE = 6;
// v72: ladder teams bank small bags early and kill our carriers for theirs. With an altar this close and nothing
// threatening us, any bag of at least NEAR_BANK_GEMS goes in (a built Ren too: he keeps his cap otherwise).
inline constexpr bool FIX_NEAR_BANK = true;
inline constexpr int NEAR_BANK_GEMS = 10;
inline constexpr int NEAR_BANK_RANGE = 6;
// v157 (EARLY_BANK, 0 off): before EARLY_BANK_ROUND the bank bar (GEM_BANK) is EARLY_BANK_GEMS. v161 bench: a champion
//   holds 20 gems by r78 (ladder winners r72) but the first tribute waits for 40, at r162 (bag 42); winners bank 23 at
//   r86, and bank 477 by r600 to our 236.
//   Bench (70 maps x 2 sides vs v156-monolith / v155): 25 to r300 72 / 76 of 140, banked +6.9%; 25 to r600 76 / 77,
//   +10.3%; 20 to r300 72 / 80, +11.7%; 20 to r600 73 / 75, +8.2%; 30 all game 69 / 67; without BANK_ON_WAY 67 / 73.
//   First tribute r162 -> r133, banked by r600 236 -> 252; fuel mean 33.9k (unchanged).
inline constexpr int EARLY_BANK_GEMS = 25;
inline constexpr int EARLY_BANK_ROUND = 600;
// v157 (BANK_ON_WAY): a bag of ON_WAY_GEMS or more goes in at an altar that adds at most ON_WAY_BANK_SLACK walking
//   steps to the walk to the team's target (steps to its submit tiles + its submit tiles to the target - us to the
//   target), not only one within NEAR_BANK_RANGE. Only fields already built are read (altar walks, group_field).
inline constexpr bool BANK_ON_WAY = true;
inline constexpr int ON_WAY_GEMS = 15;
inline constexpr int ON_WAY_BANK_SLACK = 4;  // alone (EARLY_BANK_GEMS 0): 67 / 77 of 140, banked +2.7%
// v151-f3 (ALTAR_FARM_BANK, 0 off): on one-altar maps where ALTAR_FARM_SHARE of the camp gems lie within ALTAR_FARM_RANGE
//   of it (setup: 00anvil, 00breadcrumbs, 01hooks; not 00eight 58%, 02guardians 62%, gemcombat1 67%), a bag of
//   ALTAR_FARM_BANK is banked instead of GEM_BANK, and in a fight or retreat a bag of NEAR_BANK_GEMS to ALTAR_QUICK_GEMS (a one- or two-turn channel; damage does not break it) is tributed where we
//   stand when an altar is in submit range and we are over ALTAR_QUICK_HEALTH. Ladder anvil / breadcrumbs / hooks
//   (gems beside the one altar): they bank bags of 22 / 15 / 24 about 45 / 65 / 34 times a game, we 40 / 29 / 28 about
//   6 / 4 / 11 times, and lose 100 / 242 / 106 carried gems a game to their kills.
inline constexpr int ALTAR_FARM_BANK = 20;
inline constexpr int ALTAR_FARM_RANGE = 10;
inline constexpr double ALTAR_FARM_SHARE = 0.75;
inline constexpr int ALTAR_QUICK_GEMS = 20;
inline constexpr double ALTAR_QUICK_HEALTH = 0.5;
// v72: ladder teams move their ward almost every round; ours sat on fixed posts. With FIX_SCOUT_WARD the ward
// follows its owner each round: into a reachable bush we cannot see into (worth SCOUT_BUSH_SCORE), else ahead
// toward the target. With SCOUT_KEEP_POSTS champions holding a ward spot or bush ward keep it.
inline constexpr bool FIX_SCOUT_WARD = true;
inline constexpr bool SCOUT_KEEP_POSTS = false;
inline constexpr int SCOUT_BUSH_SCORE = 100;
// The ward catches up with us once it is more than this far behind.
inline constexpr int SCOUT_REFRESH = 8;
// v72: a champion carrying CARRY_GEMS or more is a carrier: its death hands the bag to the killer (ladder: ~250
// gems per lost game). With FIX_CARRIER a carrier recalls below CARRY_RECALL_HEALTH, stays behind the front
// below CARRY_FRONT_HEALTH, and banks instead of farming on while an enemy champion is in sight.
inline constexpr bool FIX_CARRIER = true;
inline constexpr int CARRY_GEMS = 60;
inline constexpr double CARRY_RECALL_HEALTH = 0.35;
inline constexpr double CARRY_FRONT_HEALTH = 0.5;
// v72 (FIX_DODGE, off; v133a deleted its code): the hurt-dodge step dashed out, stunned a chaser (Cassi), walked out
// toward an ally within ESCAPE_ALLY_RANGE, and a melee hit back. Off: fewer deaths (-12%) but 16-18 vs v72 without it.
// v130 (ESCAPE_DASH, ported from our v123): the low-health dodge dashes away when the dash is ready: to the tile in dash
// range farthest from the enemies (out of reach first, then nearest our spawn), if it gains distance, even when the
// landing is still in reach; then walks on. v122 mirror: a dash was in 2% of Vesper's and 11% of Heart of Ruin's retreat
// deaths, and the gap to the chaser never grew in 84% (v129: same code, same numbers). On v122: 86-74 +8.3% vs v122,
// 92-68 +14.1% vs v120. Heart of Ruin raises his shield first (one cast a turn): he dashes only while it is not ready.
inline constexpr bool ESCAPE_DASH = true;
// ESCAPE_DASH_HOR: Heart of Ruin escape-dashes too (his dash is also his dive and landing stun). ESCAPE_MIN_GAIN: a dash
// must land out of reach or gain at least this many tiles from the nearest enemy.
inline constexpr bool ESCAPE_DASH_HOR = true;
inline constexpr int ESCAPE_MIN_GAIN = 1;
// v131 (RETREAT_SAFE, from our v106): a team retreat steps out of every near enemy's dash and spell reach (recall_zone) instead
// of the flat THREAT_RANGE, toward the nearest ally already outside it (within RETREAT_ALLY_TILES of it), else to the
// nearest safe tile. Out of reach, it farms around that wider zone. v129 mirror: 32% of champions died within 20 rounds
// after a retreat.out (inside enemy reach) episode; on v105 the switch alone went 71-69, +2.4%.
inline constexpr bool RETREAT_SAFE = true;
inline constexpr int RETREAT_ALLY_TILES = 3;
// v131 (RETREAT_COOL): for RETREAT_COOL_ROUNDS rounds after our last retreat turn, farming keeps out of the visible
// enemies' dash and spell reach (farm() avoids it), instead of walking straight back next to them.
inline constexpr bool RETREAT_COOL = false;
inline constexpr int RETREAT_COOL_ROUNDS = 10;
// v132 (escape tools for champions without a dash, in the low-health dodge; v130 debug: low-health retreats died Ryth 28%,
// Marcella 34%, Willow 30%, Prism 27%, Varg 39%; Ryth and Willow are drafted on 34 of 40 maps, Marcella 19, Prism 9, Varg 4):
// ESCAPE_VARG_SHIELD: Varg raises his shield (2.5x Focus, walking allowed) with an enemy within THREAT_RANGE + 1.
// ESCAPE_MARCELLA_STUN: with an enemy in reach, Marcella hits it, resets and hits again (the enhanced hit stuns it for a
//   round), then walks away as usual.
// ESCAPE_STUN: Ryth (area stun) and Prism (damage + stun) stun the nearest enemy instead of walking when walking cannot get
//   out of its reach (a melee within 1 tile, a ranged within ESCAPE_STUN_RANGED); the stun costs our walk, the chaser
//   loses its turn too.
inline constexpr bool ESCAPE_VARG_SHIELD = true, ESCAPE_MARCELLA_STUN = false, ESCAPE_STUN = true;
inline constexpr int ESCAPE_STUN_RANGED = 3;
// ESCAPE_STUN_BACKED: the Ryth / Prism stun only with a healthy ally within ESCAPE_STUN_ALLY tiles (who can use the
// stunned round) or with a second enemy within 1 tile of the chaser (Ryth's area catches both). Alone it lost: the stun
// went in already-lost spots (62% of Ryth's stun retreats died) and her area stun was gone for the next fight.
inline constexpr bool ESCAPE_STUN_BACKED = true;
inline constexpr int ESCAPE_STUN_ALLY = 4;
// RETREAT_COOL_NEAR: the cool-off keeps out of the near enemies' reach only (within FIGHT_STEPS walking steps), not every
// enemy in sight (one seen through a ward far off blocked our farming).
inline constexpr bool RETREAT_COOL_NEAR = false;
// v72: on hard maps we captured twice as many gold clusters (20 gold, no gems) as ladder teams. With FIX_GEM_FIRST
// a gold-only group is taken on the way only within GOLD_ONLY_NEAR steps.
inline constexpr bool FIX_GEM_FIRST = true;
// v158 (GOLD_NEAR): 2 -> 8. Top 5 take 20.8 gold clusters per 1k rounds (ours 10.8); clusters now have 120 hp, not
// the 500 the v72 rule was tuned on. v156 bench: 160-120 vs 154-126, gold-cluster captures 9.0 -> 11.5. 2 is off.
inline constexpr int GOLD_ONLY_NEAR = 8;
// ...and its value in the draft plan and the route is scaled by GEMLESS_VALUE.
inline constexpr double GEMLESS_VALUE = 0.25;
// v74: a champion on spawn with a bag an off-spawn bank rule would bank banks from there, and only an arrival
// (respawn, recall) sets rejoining. Fixes the spawn.leave / bank.walk loop on corners, ambush, packs, rows.
inline constexpr bool FIX_SPAWN_BANK = true;
// v151-f1 (SPAWN_PLAY_ON): a healed champion whose spawn.leave walk finds no step (the target group touches the spawn,
//   so we already stand beside its live camps) plays the rest of the turn from the spawn instead of idling. Ladder
//   00breadcrumbs 0-13: the leader picked the gold cluster beside our spawn and all three idled 240-760 rounds there.
inline constexpr bool SPAWN_PLAY_ON = true;
// v75: the fight score counts only allies who can join the fight: not channelling (recall, tribute, teleport)
// and within FIGHT_ALLY_RANGE of the enemy nearest the fight.
inline constexpr bool FIX_FIGHT_BODIES = false;
// v123 (FIGHT_LOCAL): vision includes our ward, so the fight score counted every ally in sight (one 20 tiles off seen
//   through the ward, or recalling) and every enemy within FIGHT_STEPS of any of them (an enemy the ward saw beside a
//   far ally deterred us). Now it is local to the champion scoring: allies count only within FIGHT_LOCAL_RANGE tiles of
//   us or of the near enemy closest to us, and not channelling; enemies are near (near_enemies) only within
//   FIGHT_STEPS walking steps of us or of an ally within FIGHT_LOCAL_RANGE of us. Wards still serve vision otherwise.
//   At 8 (FIGHT_ALLY_RANGE): 40-40 / 37-43 vs v122, 58-66 on the non-Prism maps.
//   At 12: 39-41 / 40-40 vs v122, banked +6.6 / +5.2%, teamfights won 236-204 / 271-177; but allies counted at 9-12
//   tiles walked in within 4 rounds only 35% of the time (5-8: 73%), no more often than uncounted allies at 13-16.
//   9 tiles: 73/160, -6.0%. Walking steps 12: 70/160, -10.8%; 9: 77/160, -5.2%; 15: 71/160, -6.1% (README bench 6).
//   Kept: 12 tiles.
// FIGHT_LOCAL_WALK: the range is walking steps (a capped search from us, and from the near enemy only when needed).
inline constexpr bool FIGHT_LOCAL = true, FIGHT_LOCAL_WALK = false;
inline constexpr int FIGHT_LOCAL_RANGE = 12;
// v124 (FIGHT_LOCAL on), each switchable:
// FIGHT_ALLY_WEIGH: an ally our rules keep out of a fight does not count (under RECALL_HEALTH: recalls or dodges; a
//   carrier the front rule sends back); the rest count by JOIN_WEIGHT of its tiles from us or the near enemy closest
//   to us: the share of allies that far who joined a fight within 4 rounds (b123y logs, 1-4 tiles = 1).
// FIGHT_ENEMY_WEIGH: the same for enemies: a channelling (recall, tribute, teleport) or hurt (under RECALL_HEALTH)
//   enemy does not count, the rest by JOIN_WEIGHT of its tiles from the enemy nearest us.
// FIGHT_COMMIT_PING: a champion that commits to a fight (fight.hit) whose score counted an ally over COMMIT_FAR tiles
//   away pings for it, unhurt and in fight mode too; allies answer from PING_ARRIVE_COMMIT tiles (else PING_ARRIVE).
// Vs v122, 160 games (README bench 7): ally 80, +1.0%; enemy 83, +10.5%; ping 68, -12.1% (far allies farming through
//   a fight 29% -> 22%, but the team loses its farming); all three 81, +1.2%. Retuned, ally + enemy trailed enemy alone.
// v128 screen (seed 0, vs v122 + v120, 160 games; the shipped setting 93/160, +21.6%, teamfights 613-344): ally
//   weighting on 81, +2.4%; with FIGHT_THRESHOLD 0: 75, 0.0%; MISSING_BODY_PENALTY 3: 84, +6.3%; OUTNUMBER_BONUS 15:
//   91, +10.1%. Off.
inline constexpr bool FIGHT_ALLY_WEIGH = false, FIGHT_ENEMY_WEIGH = true, FIGHT_COMMIT_PING = false;
// v133 (FIGHT_SKIP_LEAVERS): an ally our own rules are pulling out of the fight (stays_out: under RECALL_HEALTH, so it
// dodges or recalls, or a carrier the front rule sends back) does not count in the fight score - the "stays out" half of
// FIGHT_ALLY_WEIGH without its distance weights. v129 mirror: fight starts with a fleeing ally near lost 28%, won 5%;
// staying in while an ally fled lost 27%, won 9% (all fights 19% / 17%). LEAVERS_KEEP_HOR: Heart of Ruin still counts
// under the bar (HOR_HOLD keeps him in a fight while his health and shields outlast the burst).
inline constexpr bool FIGHT_SKIP_LEAVERS = true, LEAVERS_KEEP_HOR = false;
// v131 (CAMP_WAIT): after stepping out of awake camps (camp_exit), farming keeps out of each camp that was hitting us until it
// sleeps again or targets an enemy (an enemy champion inside its reach, nearer to it than its edge), or for CAMP_WAIT_ROUNDS
// at most. v130 logs: 3,626 times in 80 games a champion left an awake camp and was back in its reach within 10 rounds
// (farm.walk -> camp.exit -> farm.walk -> camp.exit ...). The group we are farming (target or just farmed) never counts.
inline constexpr bool CAMP_WAIT = true;
inline constexpr int CAMP_WAIT_ROUNDS = 20;
// CAMP_WAIT_TARGET_FIGHT: with an enemy champion near, the group we farm waits too (it is only exempt with no enemy near).
inline constexpr bool CAMP_WAIT_TARGET_FIGHT = true;
// v132 (CAMP_WAIT_FIGHT): fighting, a champion that stepped out of awake camps does not walk (or dash, or join an ally)
// back into their reach: it closes around them when the target can be hit from outside, else it waits outside until
// every one sleeps again (or CAMP_WAIT_ROUNDS pass). Nothing exempts a camp: not an ally inside (two of us took turns
// being the nearest - one stepped out, the camp turned on the other, who stepped out as the first walked back in, and
// both were worn down), not an enemy inside, not the group we farm.
inline constexpr bool CAMP_WAIT_FIGHT = true;
// v132b: the fight wait (hold or go around) lasts at most CAMP_WAIT_FIGHT_ROUNDS after our last exit from the camp. v132's
// 20 rounds lost 54-69: 80 of 139 holds ended over 5 rounds after the exit, and 1,891 fight turns walked around camps.
inline constexpr int CAMP_WAIT_FIGHT_ROUNDS = 4;
// v132d/e (CAMP_SHARE): fighting inside an awake camp's reach while it hits an ally (nearer to it than us), with the
// target out of our reach: we are its next target once the ally steps out, so we leave too - to a tile that reaches the
// target outside every awake camp's reach, else just out - and the camp counts as left (the fight wait keeps us out).
inline constexpr bool CAMP_SHARE = true;
// v132d/e (CAMP_WAIT_WALK): bank and carry walks keep out of the camps of the fight wait too: around them, else wait.
inline constexpr bool CAMP_WAIT_WALK = true;
// v132f/g (CAMP_REACT_ANY): reacting to an enemy (every camp_exit stem, and fight()), leave every awake camp's reach we
// stand in - hitting us or an ally - and never step into one: with an enemy near, the walk guard below holds every
// awake camp in sight, tanked or not.
inline constexpr bool CAMP_REACT_ANY = true;
// v132f/g (WALK_GUARD): every walk and dash keeps out of awake camps' reach unless an ally stands in it (tanks it).
// With no enemy near, the group we farm (slot, just farmed, monolith job) is ours to engage and not guarded.
// Asleep or dead camps are never guarded. Not applied while we already stand in a guarded reach (the walk out).
inline constexpr bool WALK_GUARD = true;
// v132h (GUARD_NEAR): only camps whose reach is within GUARD_NEAR tiles of us are guarded (read): v132g read every camp
// in sight and cost ~2k fuel a turn (+6%), ~30% more out-of-fuel turns. A far camp is guarded once we close on it.
inline constexpr int GUARD_NEAR = 4;
// v133a (GO): one mover, go() in actions.h, for every walk, dash and hop. v133a reproduces v132h's movement; each GO_*
// switch below is one of the v133 experiments.
//   GO_URGENT_DASH: getaways (retreat, camp exits, the hurt dodge) spend the dash even under VESPER_KEEP_DASH.
//   GO_HOP_SAVE: steps a hop route must save over walking, on top of the dash's remaining cooldown (v98: HOP_SAVE).
//   GO_HOP_AVOID: walks with a keep-out board (retreat's farm, fight camp waits, Willow's dodge) hop too.
//   GO_ESCAPE_PATH: the hurt dodge's dash lands on the tile with the shortest path out of enemy reach (the farthest
//     from the nearest enemy on a tie), not on the farthest tile in a straight line.
//   GO_GUARD_DASH: the escape and kite dashes keep out of the walk guard too.
//   GO_FINISH_PATH: a closing dash counts steps along the path to the target's ring, not a straight line.
//   GO_SHORT_DASH: a route walk does not dash when its walking step already reaches the goals.
//   GO_EXIT_DASH: leaving awake camps (camp_exit, stray_exit, fight.share) counts a ready dash as this turn's reach
//     (dash range + the walk) when it picks the exit, goes even with the walk spent (dash only), and moves as a getaway.
//   GO_KITE: Vesper's kite: in a fight, fire then dash after a fleeing target (else out of reach as before); in a retreat
//     from enemies without a ready dash, dash out then fire instead of walking (see vesper_retreat).
inline constexpr bool GO_URGENT_DASH = true;
inline constexpr int GO_HOP_SAVE = 2;
inline constexpr bool GO_HOP_AVOID = false;
inline constexpr bool GO_ESCAPE_PATH = false;
inline constexpr int ESCAPE_PATH_STEPS = 12;
inline constexpr bool GO_GUARD_DASH = true;
inline constexpr bool GO_FINISH_PATH = false;
inline constexpr bool GO_SHORT_DASH = false;
inline constexpr bool GO_KITE = true;
inline constexpr bool GO_EXIT_DASH = true;
// v132c (CAMP_WAIT_FARM): farm() no longer keeps out of the camps we left (v131's 20-round keep-out): farming, the only
// reason to leave a camp is a recall. Camps are still remembered on every exit for the fight wait.
inline constexpr bool CAMP_WAIT_FARM = false;
// v134 (step 5: the emblem term, +3 per emblem of ours and ~+41 in an average v129 fight decision, outweighed every other
// term in 69% of fight starts; behind on both health and power, fights started lost 47%, won 3%):
// EMB_DOUBLE_DEFICIT: behind on health AND power and not outnumbering (weighted bodies), the emblem term is dropped.
// EMB_STAY_STRICT: behind on health AND power, staying in a fight needs the normal FIGHT_THRESHOLD (no hysteresis slack).
// EMB_CAP: the emblem term is at most EMB_CAP_VALUE (0: no cap).
inline constexpr bool EMB_DOUBLE_DEFICIT = false, EMB_STAY_STRICT = true;
inline constexpr int EMB_CAP_VALUE = 0;
inline constexpr double JOIN_WEIGHT[] = {1, 1, 1, 1, 1, 0.86, 0.78, 0.68, 0.57, 0.47, 0.39, 0.30, 0.23};
inline constexpr int COMMIT_FAR = 8, PING_ARRIVE_COMMIT = 12;
// v128 (PING_IDLE_ONLY): only a free ally answers a ping (any ping: the byte does not say which kind): not the
//   reward's collector, and no awake or hit camp within its attack range + 1. Farming goes on; idle hands come.
//   With FIGHT_COMMIT_PING: 86/160, +10.9%, teamfights 481-472; far allies joined 44% (34% without), farming through
//   a fight 22.8% (29.0%). Gathers without losing the farming (the plain commit ping: -12.1%), but wins less: off.
inline constexpr bool PING_IDLE_ONLY = false;
// v76: three stuck states from a rushbait_v2 ladder loss: closing from outside on an enemy home in its chase ring
// (then leaving the ring, forever), a ranged champion holding back from a camp for a tank that never comes (at
// most HOLD_LIMIT rounds now), and waiting on spawn for full health while enemies keep hitting us.
inline constexpr bool FIX_STUCK_RING = true, FIX_STUCK_HOLD = true, FIX_STUCK_HEAL = true;
inline constexpr int HOLD_LIMIT = 30;
// v143 (WILLOW_REGROUP, user): Willow never opens a camp herself after HOLD_LIMIT; she goes to find the team: the
//   freshest sighting of an ally (within REGROUP_SEEN rounds, away from her) else our spawn (walk or a safe recall,
//   as for the shop), and waits there. Over once an ally is within TEAM_RANGE, or after REGROUP_MAX rounds.
inline constexpr bool WILLOW_REGROUP = true;
inline constexpr int REGROUP_SEEN = 15, REGROUP_MAX = 60;

// ---- v144 switches: the middle (user; ladder: wins took 110 middle gems to 64 before r300, losses 54 to 98, and losses
//   farmed home more; enemies take ~1-21 of our side's gems a game) ----
// MIDDLE_FIRST: best_goal aims at the centre pocket (cores[1]) from round 0, not only once aggressive; the altar pocket
//   (cores[0]) waited for Vesper's core, 4 team emblems and everyone alive, and every death sent the team back home.
//   Home groups stay on-the-way loot and respawn fillers; the safe-only phase (too few of us) still falls back home.
inline constexpr bool MIDDLE_FIRST = false;  // v144 bench: alone -4.7% (more middle fights, K/D down)
// CONTEST_VALUE: a group's value grows by CONTEST_DENY x contest, contest = 1 at even walking steps from both spawns
//   (the draft's home / enemy steps), falling linearly to 0 at CONTEST_WIDTH off even (share of steps ours = 0.5 +-
//   CONTEST_WIDTH): a middle gem we take is one they do not. Home groups (they almost never take) stay at face value.
inline constexpr bool CONTEST_VALUE = true;  // v144 bench: alone +4.4% (217-188, 3 seeds)
inline constexpr double CONTEST_DENY = 1.0, CONTEST_WIDTH = 0.3;
// CONTEST_ANYWHERE: before aggressive, best_goal rates every group in the table (contest-priced) instead of a pocket.
inline constexpr bool CONTEST_ANYWHERE = false;

// ---- v145 switches: waiting for the tank (user; ladder 01corridors: Willow and Vesper took the first elk hits, Willow
//   died to two elks after five hits in a row; v143 debug corridors: 46 Willow turns of camp damage outside fights) ----
// AWAKE_READ: willow_exposed and under_threat count a camp at full health as awake when the engine says so (damage
//   wakes the whole group: the second elk of a pair hit Willow at full health while she "held" or kept attacking).
inline constexpr bool AWAKE_READ = true;
// RANGED_HOLD: a ranged champion opens a camp that hits only with a melee ally closer and coming (as before), when it
//   wins the group alone (solo_win), or when no melee ally is alive; v144 let it open with no melee in sight.
//   FIX_STUCK_HOLD still opens after HOLD_LIMIT for all but Willow.
inline constexpr bool RANGED_HOLD = false;  // v145 bench: alone +1.7%, with AWAKE_READ +0.2% (adds nothing)
// WAIT_OUTSIDE: a ranged champion waiting on a respawn (its camps out of sight) stands just outside the group's
//   reach instead of WAIT_RANGE off its tiles: a camp in a bush woken by someone else hit the nearest champion, Willow
//   2 off the elks "respawning in 9" (120 a round for 5 rounds).
inline constexpr bool WAIT_OUTSIDE = false;  // v145 bench: alone +0.7% (seeds +3.8 / +5.7 / -7.1); all three -5.1%

// ---- v148-normal switches: farming as a team (user). An awake camp hits the nearest champion in its reach and keeps
//   its target on a tie, so aggro moves when someone stands strictly nearer: a swap is one step, not a walk out of
//   reach and back. v145 debug (v148-145 vs v145, corridors-like map): HoR at 430 walked out of two elks for 6 rounds
//   and died; Ryth at 480/1200 walked out and recalled while Marcella stood level beside him taking the elks; Vesper
//   dashed adjacent to an elk while Ryth stood a tile further and tanked it to 340. ----
// CAMP_STICKY (removed): a tie counted as the other champion's when we lost no health since our last turn. Bench: off
//   +5.6% vs v145 (3 seeds), on +1.2%. Replays then showed the rule is wrong (see CAMP_TIE_ORDER).
// CAMP_TIE_ORDER: an ally level with us counts as the camp's target when it acts before us (priority_of, then draft
//   order = roster order). Measured from 25 replays stepped per unit turn: nearest by Chebyshev 8909 of 8949 hits; ally
//   ties to whoever acts first 1253 of 1254; not the previous target (195 switches). Enemy ties stay ours (unresolved).
//   Used wherever "does this camp hit us" is asked (exit cost, camp heat, foreign hits, swaps, tank steps, finishing).
inline constexpr bool CAMP_TIE_ORDER = true;  // vs v148-normal with sticky off: +5.2% (3 seeds, every seed +)
// TANK_PRIORITY: tanks act before squishies, so they take ties: Varg, Marcella and Vendetta 6 (were 3, 3, 4), Willow 4
//   (was 6, ahead of them).
inline constexpr bool TANK_PRIORITY = false;  // vs tie order alone: with it +2.8%, alone +3.5% (tie order alone +5.2%)
// SWAP_STEP: a hurt champion with a sturdy ally at the camp steps one tile so the ally is the nearer (or level, when the
//   ally acts first: CAMP_TIE_ORDER) and hits on. With no covered tile to hit from it still takes a covered tile one
//   step out (a melee behind the tank) for up to SWAP_IDLE_ROUNDS rounds.
inline constexpr bool SWAP_STEP = true;
inline constexpr int SWAP_IDLE_ROUNDS = 3;
// RANGED_COVER: a ranged champion an awake camp of our group hits (nearest, or level and hit last turn) while a sturdy
//   ally stands in that camp's reach steps to a tile the ally covers that still reaches a camp: nobody tanks for a
//   champion with three tiles of range.
inline constexpr bool RANGED_COVER = true;
// TANK_TAKE_EARLY: a sturdy ally steps in front of a camp hitting an ally under BAND_HEALTH (not only under the recall
//   bar) with less health share than ours, or hitting a ranged ally while we are melee: the swap happens before the
//   tank is too low to stay.
inline constexpr bool TANK_TAKE_EARLY = true;
// STAY_TANKED: a hurt ally under the recall bar still counts as the camp's tank (camp_tanked, should_hold_back) while
//   another sturdy ally stands in that camp's reach to take over: the others keep farming until no tank is left.
inline constexpr bool STAY_TANKED = true;
// FINISH_ORDER: finishing a started group counts, camp by camp in kill order (weakest first), only the camps that hit
//   us (no ally strictly nearer, sticky ties), not every camp for the whole fight. FINISH_RANGED: a ranged champion
//   with no tank keeps hitting a started group it finishes that way above FINISH_HEALTH instead of backing off.
inline constexpr bool FINISH_ORDER = true;
inline constexpr bool FINISH_RANGED = true;
inline constexpr int SPAWN_WAIT_LIMIT = 15;
// v76: an enemy within HOME_CAMP_STEPS walking steps of its spawn heals there every turn: it is not near for the
// fight decision or a target (FIX_HOME_CAMPERS).
inline constexpr bool FIX_HOME_CAMPERS = true;
inline constexpr int HOME_CAMP_STEPS = 1;
// v141 (HOME_STALEMATE): while we stand within HOME_STALE_STEPS walking steps of our spawn, an enemy within as many of
//   its own is no fight either: both of us heal at home before anyone dies. On treasure-box (spawns 4 apart) the team
//   spent ~450 of its 500 turns of rounds 0-99 bouncing at home between fight.leave, spawn.leave and farm.camp.
inline constexpr bool HOME_STALEMATE = true;
inline constexpr int HOME_STALE_STEPS = 3;
// v141 (REL_RING): the chase ring keeps only tiles at least REL_RING_EDGE walking steps nearer their spawn than ours.
//   With the spawns closer than the ring, it held our own spawn and the only way out of it.
inline constexpr bool REL_RING = true;
inline constexpr int REL_RING_EDGE = 1;
// v141 (REL_RING_HOLES): REL_RING's sides count holes as crossable (they stop no attack, spell, sight or dash). On
//   01gold-rush walking sides gave us the band across the holes from their spawn: deaths 217 vs 85, with it 130 vs
//   131. Over 6 seeds vs v140 (840 games): on -1.0%, off +1.6%; seeds alone swing -7..+13%, so the two are level.
inline constexpr bool REL_RING_HOLES = true;
// v141 (RING_FARM): in their chase ring with nobody to chase, farm on instead of walking out of the ring (the walk
//   out and the farm walk back looped). Off: with REL_RING the ring is their side only, and farming on there lost
//   (seed 0 vs v140: +9.0% with it off against +4.7% with it on).
inline constexpr bool RING_FARM = false;
// v141 (SPAWN_RETREAT): inside enemy reach within SPAWN_RETREAT_STEPS walking steps of our spawn, retreat onto a free
//   spawn tile (it heals us to full at turn end) instead of out of reach. Vs v140, 3 seeds, with HOME_STALEMATE and
//   REL_RING: 4 steps (and RING_FARM) -3.9%; off +0.4%; 2 steps +2.1% (in the ~17 games a seed it changes, 11 turned
//   to wins and 6 to losses).
inline constexpr bool SPAWN_RETREAT = true;
inline constexpr int SPAWN_RETREAT_STEPS = 2;
// v77: support ping in shared byte 7, free while the moving ward replaces the fixed ward posts (their mask lived
// there): bits 0-5 = route slot nearest the champion under attack + 1 (0 = none), bits 6-7 = round & 3. A champion
// that lost PING_MIN_LOSS health since its last turn with an enemy champion within PING_REACH sends one; a ping is
// fresh for PING_FRESH rounds and anyone clears an older one. A fresh ping within PING_RANGE of the leader puts the
// team in fight mode, and fight-mode champions with no enemy in sight walk to the pinged slot.
inline constexpr bool FIX_PING = true;
inline constexpr int PING_BYTE = 7;
inline constexpr int PING_MIN_LOSS = 60;
inline constexpr int PING_REACH = 4;
inline constexpr int PING_FRESH = 2;
inline constexpr int PING_RANGE = 12;
static_assert(!FIX_PING || (FIX_SCOUT_WARD && !SCOUT_KEEP_POSTS), "the ping uses the ward-post mask byte");
// v78 ping refinements, each switchable:
// PING_SCORE: a fresh ping within PING_RANGE of the leader adds PING_SCORE_BONUS to the fight score instead of
//   forcing fight mode, so the team only joins fights it would take with that ally at stake.
// PING_ANSWER_LIMIT: only champions within PING_ARRIVE tiles answer (in farm mode too), never a carrier (CARRY_GEMS),
//   one under PING_ANSWER_HEALTH or the pinger itself.
// PING_FILTER: no ping with an ally within PING_ALONE tiles or while the team is already in fight mode.
// PING_LOG: the pinger logs the outcome PING_REPORT rounds later; each answerer logs once per ping.
inline constexpr bool PING_SCORE = true, PING_ANSWER_LIMIT = true, PING_FILTER = true, PING_LOG = true;
inline constexpr int PING_SCORE_BONUS = 5;
inline constexpr int PING_ARRIVE = 10;
inline constexpr double PING_ANSWER_HEALTH = 0.6;
// v80: PING_RALLY off (it moved the shared target, pulling far farmers too); only allies within PING_ARRIVE = 10
// tiles answer, so the rest keep farming.
// v79 rally (switches): PING_RALLY: the leader points the team's target at a fresh ping (not during an escort, not
//   into the enemy chase ring) when RALLY_HELPERS allies who could answer are within PING_ARRIVE of it, so farming
//   champions walk there. PING_REFRESH: the pinger re-sends its ping each turn while still under attack.
// PING_COLLECTOR: a collector who can answer drops its claim and goes: on the ladder the fight is worth more than
//   the reward. A built Ren answers while carrying under REN_ANSWER_GEMS (others under CARRY_GEMS). Farm-mode
//   answering is checked right after the fight step, before any banking.
inline constexpr bool PING_RALLY = false, PING_REFRESH = true, PING_COLLECTOR = true;
inline constexpr int RALLY_HELPERS = 2;
inline constexpr int REN_ANSWER_GEMS = 100;
// v81 (PING_RETREAT): a champion whose own ping is fresh, under PING_RETREAT_HEALTH and outnumbered nearby (more enemy
// champions within PING_REACH + 1 than allies within PING_ALONE, itself included) walks toward help: the nearest
// visible ally, else the team's target, else our spawn; a melee still hits what is in reach on the way. Ladder: 23%
// of pingers died within 5 rounds, and help that walks over mostly arrives after the fight is decided.
inline constexpr bool PING_RETREAT = true;
inline constexpr double PING_RETREAT_HEALTH = 0.75;
inline constexpr int PING_ALONE = 3;
inline constexpr int PING_REPORT = 5;
inline constexpr int FIGHT_ALLY_RANGE = 8;
// 0: count an ally within FIGHT_ALLY_RANGE tiles; 1: within FIGHT_ALLY_RANGE of attack reach (distance minus its
// attack range); 2: soft, full weight within SOFT_NEAR tiles fading to none at SOFT_FAR.
inline constexpr int FIGHT_WEIGHTING = 0;
inline constexpr int SOFT_NEAR = 4, SOFT_FAR = 12;
inline constexpr float ESCAPE_ALLY_RANGE = 3;

// v81 mages (Cassi and Stellar): their spells reach MAGE_CAST_RANGE, one past a wolf's reach (3; v94: elks and golems 4), and their basic
// attacks are weak, so they keep their health for the fights.
// MAGE_STANDOFF: on a camp that hits, a mage casts from outside its reach and basic attacks only once a sturdy ally
//   tanks it (FIX_ABSORB's rule) or the finish is cheap: the hits it would take killing the camp with basic attacks
//   (camp health over the damage of us and allies in reach, times the camp's strength) are at most MAGE_FINISH_SHARE
//   of max health. It no longer waits for a melee in sight before opening a camp: the spells start it.
// MAGE_LEAVE: a mage inside an awake camp's reach that may not basic attack steps out to the cast ring (a tank left,
//   or it walked into a camp).
// MAGE_KITE: in a fight a mage casts a ready spell from wherever it stands, walks toward the fight only outside enemy
//   basic-attack reach (a melee's walk-and-hit, MELEE_THREAT; a ranged shot, RANGED_THREAT), steps back out of that
//   reach while its spells cool, and basic attacks from outside it. Enemy spells reach 4 whether we stand at 3 or 4,
//   so they do not move us. (v81 first draft: cast only from a safe tile and basic attack only out of every spell's
//   reach: 56% of mage fight turns were steps, 11% casts, champion damage -28%.)
// v156-monolith (Bug C): MAGE_MONOLITH (mages take monoliths on their own job, melees guard) is deleted: it needed
//   the monolith's reach (5) under the spells' (4), never true since the v94 balance patch, so none of it ran.
inline constexpr bool MAGE_STANDOFF = true, MAGE_LEAVE = true, MAGE_KITE = true;
inline constexpr double MAGE_FINISH_SHARE = 0.20;
inline constexpr int MAGE_CAST_RANGE = 4;
inline constexpr int MELEE_THREAT = 2, RANGED_THREAT = 3;
// v90 (MAGE_KILL_FOLLOWUP): when a mage's fight spell kills its target and it stands outside enemy reach, the unspent
// basic attack goes to the next living enemy in reach.
inline constexpr bool MAGE_KILL_FOLLOWUP = true;
// v94 (MAGE_OUTRANGE): since the balance patch a spell (MAGE_CAST_RANGE 4) outranges only wolves (3); elks and golems
// hit at 4, the monolith at 5. The standoff (mage_camp) is used only on a group whose every hitting camp in sight it
// outranges (outranged); on any other a mage holds back for a tank and hits like any ranged champion.
inline constexpr bool MAGE_OUTRANGE = true;
inline bool is_mage(Champ kind) { return kind == Champ::cassi || kind == Champ::stellar; }
// v82 (RECALL_SPELL_CAUTION): any damage cancels a recall, and a spell reaches past a basic attack: Cassi, Tavi, Ren
// and Marcella hit at 4, Stellar, Prism and Ryth at 5 (a radius-1 area around a tile 4 off). The old checks used 4
// for everyone to start a recall and the basic attack range (Stellar 3) to keep one going, so a Stellar 4-5 tiles off
// broke recalls. Now a recall starts only beyond a caster's spell reach + RECALL_SPELL_STEP (spells exclude walking:
// she walks one turn and casts the next), a hurt champion steps out that far first, and a running recall is judged
// against spell reach. Tributes keep the old check: damage does not break them.
inline constexpr bool RECALL_SPELL_CAUTION = true;
inline constexpr int RECALL_SPELL_STEP = 1;
// How an enemy's reach is judged for recalls (RECALL_SPELL_CAUTION on): 0 casters by name (spell_reach), basic range
// for the rest; 1 from the enemy's own kit, every dash and spell taken as ready (a dash adds its range to a basic
// attack: Issac, Vendetta, Ren 1 + 2, Vesper 3 + 2; one cast a turn, so never dash and spell together); 2 the same with
// live cooldowns: a dash or spell counts only if it is ready by the enemy's next turn (cooldown <= RECALL_READY_TICKS).
inline constexpr int RECALL_REACH_MODE = 2;
inline constexpr int RECALL_READY_TICKS = 1;
inline int spell_reach(Champ kind) {
    switch (kind) {
    case Champ::stellar: case Champ::prism: case Champ::ryth: return 5;
    case Champ::cassi: case Champ::tavi: case Champ::ren: case Champ::marcella: return 4;
    default: return 0;
    }
}

// v86 (Mike's farming tree): farm fast where it is safe, as a team where it is not, and on the enemy's side only
// right after a kill. One switch per part so each can be turned off alone.
// ZONES: the draft labels every camp group by walking steps to each spawn: SAFE when steps from ours x ZONE_SIDE <=
//   steps from theirs, UNSAFE when the reverse holds, UNCERTAIN otherwise. The labels (and both step counts, and
//   whether a pair can clear the group alone) go to draft memory, so no champion pays for the fields.
inline constexpr bool V86_ZONES = true;
inline constexpr double ZONE_SIDE = 1.5;
inline constexpr int ZONE_SAFE = 0, ZONE_UNCERTAIN = 1, ZONE_UNSAFE = 2;
// The leader's phase (shared byte 5, bits 6-7) sets which zones are targets: GROUP safe only, CENTRE safe and
// uncertain, SPLIT safe only (Cassi & Issac farm a second safe group), RAID every zone (one unsafe group).
inline constexpr int PHASE_GROUP = 0, PHASE_CENTRE = 1, PHASE_SPLIT = 2, PHASE_RAID = 3;
inline constexpr const char* PHASE_NAMES[] = {"group", "centre", "split", "raid"};
// CENTRE: uncertain groups are targets while at least CENTRE_MIN_BODIES of us are alive and we are fewer than
// CENTRE_DOWN champions down (life reads every champion; the leader's sight undercounted: 20-14 vs 22-12 without the
// gate). CENTRE_DOWN or more down, the centre needs CENTRE_DOWN_BODIES together (CENTRE_KEEP_BODIES to stay open):
// visible, alive, not channelling, at or over RECALL_HEALTH, within TEAM_RANGE of the leader, or last seen beside a
// bush within ALLY_GRACE rounds. The emblem advantage opens it too: CENTRE_EMBLEM_EDGE more emblems per champion
// than them (theirs remembered from sightings; until every enemy has been seen, the champions in sight compared).
inline constexpr bool V86_CENTRE = true;
inline constexpr int CENTRE_MIN_BODIES = 2, CENTRE_KEEP_BODIES = 2, CENTRE_DOWN = 2, CENTRE_DOWN_BODIES = 3;
inline constexpr double CENTRE_EMBLEM_EDGE = 2.0;
inline constexpr int ALLY_GRACE = 3;
// Enemy emblems are read on sight and then every ENEMY_EMBLEM_EVERY rounds while in sight (an item read costs fuel).
inline constexpr int ENEMY_EMBLEM_EVERY = 5;
// Our own emblem record is refreshed every EMBLEM_READ_EVERY rounds (emblems only change at a shop).
inline constexpr int EMBLEM_READ_EVERY = 4;
// RAID: an unsafe group is a target only within RAID_RECENT rounds of an enemy death, with more of us together than
// enemies alive, every one of us together at RAID_HEALTH or more, and nothing safe or uncertain up within RAID_IDLE
// rounds. The group is reached, cleared and captured RAID_MARGIN rounds before the first dead enemy respawns (its
// death round, read by every champion from the roster, + the respawn time). One group, then back.
inline constexpr bool V86_RAID = true;
inline constexpr int RAID_RECENT = 15, RAID_IDLE = 8, RAID_MARGIN = 5;
inline constexpr double RAID_HEALTH = 0.6;
// v87: the man advantage from life reads (more of us alive than them) with RAID_MIN_BODIES healthy ones together, instead
// of more of us together than they have alive.
inline constexpr bool RAID_BY_ALIVE = true;
inline constexpr int RAID_MIN_BODIES = 3;
// v87: raid when the unsafe group pays better per round than the normal target, not only when nothing else is up.
inline constexpr bool RAID_BY_RATE = true;
// v104 INVADE: once the aggressive route is on (Vesper's core, TEAM_AGGRESSIVE_EMBLEMS, all alive) and the centre is
// open, the leader prices every enemy-side (unsafe) gem group: gems a round as for ours (travel, respawn wait, clear,
// capture) x (1 + INVADE_DENY, what taking it denies them), divided by 1 + INVADE_RISK per enemy champion seen within
// INVADE_RISK_RANGE of it (fading over INVADE_RISK_MEMORY rounds). It invades (RAID phase: unsafe groups open) only
// when that is at least INVADE_MARGIN x the normal pick's rate; the current invasion target gets INVADE_KEEP, and a
// group being cleared is finished.
// Revert: kills and deaths from roster life and both teams' banked gems are global, so every champion keeps the same
// count without shared memory (enemy gold cannot be read; banked gems stand in for it). From the round the aggressive
// route first comes on, REVERT_TRADE more deaths than kills, or the enemy banking REVERT_GEMS more than us, turns
// invading off for REVERT_HOLD rounds; then a new baseline starts.
// INVADE_TIMERS: aggressive, best_goal rates every open group with its respawn wait, not only the centre pocket.
// INVADE_AMBUSH: with more of us alive than them, an enemy group respawning within INVADE_AMBUSH_SLACK rounds of our
// arrival gets INVADE_AMBUSH_BONUS (their farmers come for it).
// v158 (INVADE off): zdiag (12 maps x 2 sides) put 28% of enemy-side capture claims on invasions. Off: enemy-side
// captures 15.5% -> 8.5% (top 5: 7.7%), 153 vs 149 of 280 vs v155 + v152, 75-65 vs INVADE on, fuel 34.0k -> 33.1k.
// Gates tried instead: invade only ahead in bodies 155/280, INVADE_MARGIN 1.5 142/280.
inline constexpr bool INVADE = false, INVADE_TIMERS = true, INVADE_AMBUSH = true;
inline constexpr double INVADE_DENY = 0.5, INVADE_RISK = 0.5, INVADE_MARGIN = 1.0, INVADE_KEEP = 1.25;
inline constexpr int INVADE_RISK_RANGE = 10, INVADE_RISK_MEMORY = 60;
inline constexpr int INVADE_AMBUSH_SLACK = 5;
inline constexpr double INVADE_AMBUSH_BONUS = 0.5;
inline constexpr int REVERT_TRADE = 2, REVERT_GEMS = 150, REVERT_HOLD = 300;
// v104 (after the 00ambush replay): an enemy-side group still down INVADE_MAX_WAIT rounds after we would arrive is not
// an invasion target (the team camped a captured enemy group while their farmers were elsewhere). FIX_FILL: a down goal
// is filled by the group whose gems most exceed what the detour costs the goal (pick_target).
inline constexpr int INVADE_MAX_WAIT = 8;
inline constexpr bool FIX_FILL = true;
// SPLIT: early on, while the main target is a safe group and the leader has seen no enemy for SPLIT_QUIET rounds,
// Cassi & Issac farm another safe group at least SPLIT_APART tiles from it that a pair can clear: its camps' total
// strength x rounds to clear at PAIR_DAMAGE a round is at most PAIR_TAKE. Issac picks the pair's target (shared byte 5,
// bits 0-5) every SPLIT_REPICK rounds or once it is down; Cassi captures, and Issac opens the next pair group meanwhile
// if one is up within CAPTURE_NEAR of the reward. Any champion that sees a near enemy ends the split, as does a pair
// member whose partner has been out of sight for SPLIT_PARTNER_LOST rounds or who carries ESCORT_GEMS. A new split
// waits SPLIT_COOLDOWN rounds after one ends.
inline constexpr bool V86_SPLIT = true;
inline constexpr int SPLIT_APART = 10, SPLIT_QUIET = 10, SPLIT_REPICK = 4, SPLIT_PARTNER_LOST = 3, SPLIT_COOLDOWN = 30;
inline constexpr int PAIR_DAMAGE = 90, PAIR_TAKE = 1000;
// A new split needs its first pair group within SPLIT_MAX_WALK tiles of the leader (the pair is with the team then).
inline constexpr int SPLIT_MAX_WALK = 16;
// v159 (SATELLITE, PLAN.md step 2): a one-champion split on the v86 pair machinery. On teams of SAT_MIN_TEAM or more
// without Cassi & Issac, one fixed champion (the first of SAT_KINDS drafted; never Vesper, the gold funnel) farms its
// own solo-safe group while the team farms the main target, as top teams do (31-34% parallel captures, ours 9%).
// - The leader opens the split while the centre is open, the mode is FARM, no enemy was seen for SAT_QUIET rounds, no
//   ping is up, SAT_COOLDOWN rounds have passed since the last split, and the satellite (roster reads) is alive, not
//   the collector or carrier, carries under SAT_BAG and, in sight, has SAT_HEALTH of its health. The main team keeps
//   the centre open and leaves the satellite's group (target2) alone.
// - Satellite groups: clusters only, or wolves only within SAT_STATIC_TAKE (strength x health / SAT_STATIC_DAMAGE, the
//   leader's check); in the SAT_ZONE zone; SAT_APART to SAT_RANGE tiles from the main target; up on arrival. The
//   satellite rates them by values[] over steps + its own clear time + capture (the route's own currency), and skips
//   any whose camps would deal more than SAT_TAKE of its health over its own clear (strength + SAT_FOCUS_SHARE x focus
//   a round). Its current group gets SAT_KEEP.
// - The satellite rejoins (end_split) on an enemy in sight, the team's mode turning FIGHT, its health under SAT_HEALTH,
//   a bag of SAT_BAG, or no group left.
inline constexpr bool SATELLITE = true;
inline constexpr int SAT_MIN_TEAM = 4;
inline constexpr Champ SAT_KINDS[] = {Champ::marcella, Champ::ryth, Champ::heart_of_ruin};
inline constexpr double SAT_HEALTH = 0.6, SAT_TAKE = 0.5, SAT_KEEP = 1.25, SAT_FOCUS_SHARE = 0.25;
inline constexpr int SAT_BAG = 20, SAT_STATIC_DAMAGE = 60, SAT_STATIC_TAKE = 500;
inline constexpr int SAT_APART = 6, SAT_RANGE = 20, SAT_ZONE = ZONE_SAFE, SAT_QUIET = 3, SAT_COOLDOWN = 10;
// v159 (SAT_EDGE): a satellite goes only for a group worth at least SAT_EDGE x its share of the main target (the main
// target's rate / champions alive), both rated by values[] a round (route_rate_steps; the satellite's with its own
// clear). Without it the satellite took gold and gem clusters while the main team, one short, cleared elks slower:
// 141 and 137 of 280 vs 152 off, banked@600 -9%, gems collected lower from r150.
// Stage 1 result (one pick a turn, SAT_EDGE 1.0): 130 of 280 vs 152 off (v155 + v152), 66-74 head-to-head vs off;
// parallel captures 8.9% -> 10.5%, banked@600 243 -> 239, deaths and fuel unchanged. Works, but no gain yet.
inline constexpr double SAT_EDGE = 1.0;
// v159 stage 2 (SAT_MIDDLE): satellite groups may lie in the middle (ZONE_UNCERTAIN) too; 70-82% of the gems on
// treasure-box, stalactites, lanes and islands sit there, and top-5 satellites take 19% of theirs in the middle.
inline constexpr bool SAT_MIDDLE = true;
// v159 stage 2 (SAT_ELK): single-elk groups (one elk, otherwise clusters) for a satellite owning SAT_ELK_EMBLEMS (the
// leader's emblem record, then its own count), within SAT_ELK_TAKE of its health (Jungle counted). A satellite that
// started its group's clear finishes it down to SAT_FLOOR health instead of rejoining at SAT_HEALTH.
// Off: a solo elk costs ~1,800 health (30 rounds x 60), over SAT_ELK_TAKE of a 1,200-health satellite, so it almost
// never fired (satellite elk captures unchanged); with it 153 of 280 and 74-66 vs SATELLITE off, without 155 and 75-65.
// Stage 2 (SAT_MIDDLE) vs SATELLITE off: banked@600 243 -> 251, tributes 15.4 -> 16.4, parallel 8.9% -> 11.3%.
inline constexpr bool SAT_ELK = false;
// v159 stage 3 (SAT_BANK_OWN): with SAT_BANK gems the satellite tributes at the nearest own-side or middle altar (enemy
// side: SAT_BANK_SIDE or more walking steps nearer their spawn) within SAT_BANK_RANGE and farms on, instead of rejoining;
// top-5 champions bank their own captures about every trip (median bag 21, 4.3 tributers a game; ours 3.4). Not while
// holding a reward it captures. The leader then sends a satellite out whatever its bag.
// Off: neutral (70-70 vs stage 2, tributes 16.4 both). Splits are short (5-40% of the satellite's turns), bags seldom
// reach SAT_BANK in one, and the satellite already banks small bags through bank.near. Worth retrying with longer splits.
inline constexpr bool SAT_BANK_OWN = false;
inline constexpr int SAT_BANK = 20, SAT_BANK_SIDE = 4, SAT_BANK_RANGE = 20;
inline constexpr int SAT_ELK_EMBLEMS = 2;
inline constexpr double SAT_ELK_TAKE = 0.75, SAT_FLOOR = 0.35;
inline constexpr int NO_TARGET2 = 63;
// Capture spacing: while a reward outside the safe zone is being captured, the rest only move on to groups within
// CAPTURE_NEAR tiles of it, so the collector stays in sight of the team.
inline constexpr bool V86_CAPTURE_NEAR = false;  // v135: off with CAPTURE_MOVE_ON (kept: +7.9%, off: +10.6%)
inline constexpr int CAPTURE_NEAR = 6;
// The 35-60% band (BAND_HEALTH to RECALL_HEALTH), off a fight: a bag of NEAR_BANK_GEMS goes to an altar within
// BAND_BANK_RANGE; with an ally in sight the champion farms on with the team; alone, it farms a safe group up within
// BAND_SAFE_RANGE tiles (the target stays if it is safe) or recalls.
inline constexpr bool V86_BAND = true;
inline constexpr double BAND_HEALTH = 0.6;
inline constexpr int BAND_BANK_RANGE = 10, BAND_SAFE_RANGE = 12;
// Finish a started camp: under RECALL_HEALTH a champion with no enemy champion in sight keeps hitting a camp of the
// target group that is already hurt while the hits it would take finishing it leave it at FINISH_HEALTH or more
// (never with a bag of CARRY_GEMS: a camp kill destroys the bag).
inline constexpr bool V86_FINISH = true;
inline constexpr double FINISH_HEALTH = 0.2;
// Gold: recall to shop at RECALL_GOLD without banking first (recall keeps the bag), while the target is safe or we
// are alone; a short walk home replaces the recall (recall_to_shop). Off: 14-20 on vs 21-13 off (unlimited vs v82),
// the extra trips home cost more farming than the earlier items gave.
inline constexpr bool V86_GOLD_RECALL = false;
// Mages basic attack camps like any ranged champion (and tank them when nobody else does), and cast from outside a
// camp's reach (MAGE_STANDOFF) only farming alone (or as the split pair) in the safe zone under BAND_HEALTH, or on a
// camp already started outside the safe zone.
inline constexpr bool V86_MAGE_RULE = true;
// v91 (V91_MAGE_NO_TANK): the rule above sends a mage to hit like a ranged only while a sturdy ally tanks the camp
// (camp_tanked); with nobody tanking it stands off and casts (MAGE_STANDOFF's mage_camp), so it never tanks.
// v91 (V91_TANK_HURT): camp_tanked and coming_to_camp skip an ally under RECALL_HEALTH, who is about to step away and
// recall (Willow and the mages kept hitting from inside the reach while Issac stepped out to recall).
inline constexpr bool V91_MAGE_NO_TANK = true, V91_TANK_HURT = true;
// v93 (V93_CAMP_EXIT): a champion counts as hurt (recall, or step out first) once its health is at most what walking out
// of the awake camps hitting it costs (camp_exit_cost), not only under the recall bar.
inline constexpr bool V93_CAMP_EXIT = true;
// v105 (CAMP_EXIT_REACT): on hooks the team reacted to enemies (fight, retreat, hurt dodge, ping retreat) from inside
// the packed elk box, stepping out of enemy reach only, so the elks kept hitting it while it walked about or toward
// the enemy. Now a champion reacting to an enemy while an awake camp hits it (it is the nearest champion in that
// camp's reach) leaves the camps first. Each tile in reach is judged on three tests: out of every awake camp's reach
// (stops the damage), farther from every near enemy than we are (away from them), and at most one step more than the
// quickest way out (fast). It walks to the nearest tile passing all three, else out and away, else out fast, else
// away fast (no way out within CAMP_EXIT_STEPS). Melee still hit what is in reach on the way; Willow keeps her own
// backline step. In a fight, a champion no camp is hitting approaches the enemy around awake camps' reach, not
// through it (straight through only when no other way reaches the enemy).
inline constexpr bool CAMP_EXIT_REACT = true;
inline constexpr int CAMP_EXIT_STEPS = 6;
// v129 (CAMP_EXIT_ALWAYS): the exit above ran only when reacting to an enemy, so awake camps we were not farming (woken
// by an enemy, an ally or a passing fight) kept hitting us on every other turn: walks to bank, to a ping, to an ally.
// Now any champion an awake camp hits leaves first, enemy near or not, unless the camp is ours: its group is the
// team's farm target, the group we farmed (engaged in farm(), the on-the-way groups too) within FARMED_ROUNDS, or
// the mages' monolith job. A champion with one of our camps in its attack range leaves it to farm()'s STRAY_EXIT
// (one step that still reaches our camp); a collector to collect().
inline constexpr bool CAMP_EXIT_ALWAYS = true;
inline constexpr int FARMED_ROUNDS = 3;
// v114 (HARD_GATE): a group is only a target once the team's visible emblems reach ELK_EMBLEMS per elk plus
//   GOLEM_EMBLEMS per golem in it (wolves and clusters need none; monoliths keep their own gate). When no reachable
//   group passes, the ones needing the fewest emblems stay in, so the team never stands idle. It also stops the
//   passing cluster hit and camp targeting in farm() (can_take_camp).
// v114 (PASS_FIX): a cluster in reach is a free hit only when its whole group is clusters: one inside an elk group
//   wakes the elks (damage wakes the whole group) and earns nothing until they die too.
inline constexpr bool HARD_GATE = true, PASS_FIX = true;
inline constexpr double ELK_EMBLEMS = 1.0, GOLEM_EMBLEMS = 3.0;
// v114: the first HARD_FREE emblems of a group's need are free (a bare team clears two elks): need = elks x ELK_EMBLEMS
//   + golems x GOLEM_EMBLEMS - HARD_FREE. Fixed free parts all lost to no gate (47-33 on 40 maps): the gated packs hold
//   17-100% of a map's gems. Kept: the free part is the team size (HARD_FREE_TEAM), so on five only 6+ elk packs and
//   golems wait (starfish's centre needs 1 emblem): 46-34, banked +20.3% vs +16.9%; the 4 maps it fires on +1907.
inline constexpr int HARD_FREE = 0;
// v114 leniency: HARD_FREE_TEAM adds the team size (3-5) to the free part; HARD_DECAY_ROUNDS (0: off) frees one more
//   emblem every that many rounds, so every gate opens by mid-game.
inline constexpr bool HARD_FREE_TEAM = true;
inline constexpr int HARD_DECAY_ROUNDS = 0;
// v142 (GOLEM_GATE, user): per golem in a group, the champions present (PRESENT_GATE's count) need this many emblems
//   among them: alone GOLEM_SOLO_MELEE / GOLEM_SOLO_RANGED by our own attack, then GOLEM_PRESENT_EMBLEMS by count (5+
//   use the last). No free part; elks keep ELK_EMBLEMS each less the free part, added on top. Golem groups never pass
//   on hard_floor's fallback (monoliths never did). Model (5,000 hp, 120 a hit, v141 builds, Vesper funded first,
//   finish over FINISH_HEALTH): solo HoR / Varg 5, Ryth 6, Marcella 8, Vesper 10; pairs 1-6; three or more 0-1.
//   v141 asked 3 per golem less one per champion present (2 alone, 1 for two).
inline constexpr bool GOLEM_GATE = true;
inline constexpr int GOLEM_SOLO_MELEE = 6, GOLEM_SOLO_RANGED = 10;
inline constexpr int GOLEM_PRESENT_EMBLEMS[] = {0, 0, 6, 5, 4, 4};  // by champions present; 0 and 1 unused
// v142 (NOTHING_ROAM, user): with nothing we may farm in the target table (every group gated or closed), pick_target
//   no longer keeps the old (gated) target: it heads for the group nearest the map centre, one we may farm and is up
//   first, as the aggressive route roams. Gold there, and enemies to meet; fights are still taken on the score.
inline constexpr bool NOTHING_ROAM = true;
// v105 (STRONG_EARLY): strong camps (elks, golems: strength STRONG_CAMP_STRENGTH or more) cost the team more health
// early, when it has few items. The health part of their price (draft::DAMAGE_COST x strength x clear rounds) is
// multiplied by 1 + STRONG_EARLY_EXTRA at no emblems, fading linearly to 1 once the team owns STRONG_FADE_EMBLEMS
// emblems per champion.
inline constexpr bool STRONG_EARLY = false;
inline constexpr int STRONG_CAMP_STRENGTH = 60;
inline constexpr double STRONG_EARLY_EXTRA = 3.0;
inline constexpr double STRONG_FADE_EMBLEMS = 2.0;

// Escorted tributes: a bag this big is only carried to an altar with the team around it.
inline constexpr int ESCORT_GEMS = 150;
// Escorts within this range of the carrier count; this many (or every living ally if fewer) must be there.
inline constexpr int ESCORT_RANGE = 3;
inline constexpr int ESCORT_MIN = 2;
// Waiting carriers stand this far from the escort altar, and give up waiting after this many turns.
inline constexpr int STAGING_RANGE = 5;
inline constexpr int ESCORT_WAIT = 12;
// While the carrier channels, escorts farm free camps this close to the altar.
inline constexpr int PROTECT_RANGE = 6;
// A carrier below this health stays behind the team instead of fighting at the front.
inline constexpr double FRONT_HEALTH = 800;
// An escort is dropped after this many rounds, in case the carrier died out of everyone's sight.
inline constexpr int ESCORT_TIMEOUT = 60;
// Altars this close to the map centre are contested, so heavy bags only go there escorted.
inline constexpr int CENTRAL_ALTAR_RANGE = 10;

// Wards: spots near the map centre and on a ring around the enemy spawn, at most one ward per spot.
inline constexpr int ANTI_WARD_RADIUS = 8;
inline constexpr int CENTRAL_WARDS = 2;
inline constexpr int ENEMY_SPAWN_WARDS = 3;
// Ring distance from the enemy spawn; ward vision is 6, so the ring sees the spawn exits.
inline constexpr int ENEMY_WARD_GAP = 7;
// A ward still covers its spot when it lands this close to it.
inline constexpr int WARD_SLACK = 1;
// Bush wards: a champion whose ward is still unused spends it once, on a bush it cannot see into in a
// key area: near an altar first, then near the map centre, then near a route camp. Never on a fight
// turn or with an enemy in sight, and never within BUSH_WARD_GAP of a visible allied ward.
inline constexpr int BUSH_ALTAR_RANGE = 6;
inline constexpr int BUSH_CENTER_RANGE = 10;
inline constexpr int BUSH_CAMP_RANGE = 4;
inline constexpr int BUSH_WARD_GAP = 4;
// A followed ward goes on a bush this close to the new target's centre.
inline constexpr int FOLLOW_BUSH_RANGE = 6;
// Fuel set aside for building the key-bush boards at a turn end.
inline constexpr std::uint64_t BUSH_BOARD_FUEL = 15'000;
// Walk to an uncovered spot this far away when the route leaves time; further on the aggressive route.
inline constexpr int WARD_DETOUR_RANGE = 6;
inline constexpr int AGGRESSIVE_WARD_DETOUR_RANGE = 12;

// Fight score.
// Only enemies within this many walking steps of us or a visible ally count for fight decisions;
// one further off (say, seen through a ward) is information only.
inline constexpr int FIGHT_STEPS = 10;
// Indecision guard: once fighting, the team only retreats when the score falls this far below the
// threshold, and a switch between fight and retreat holds for at least this many rounds.
// FIGHT_THRESHOLD, FIGHT_HYSTERESIS, MIN_HOLD, OUTNUMBER_BONUS, MISSING_BODY_PENALTY: src/evo_constants.h.
// Prefer farming during the respawn window after enough enemy champions are down (team size -> dead).
inline int kill_window_dead_enemies(std::size_t team) { return team == 3 || team == 4 ? 2 : team == 5 ? 3 : -1; }
inline constexpr int KILL_WINDOW_FIGHT_PENALTY = 20;
inline constexpr bool KILL_WINDOW = false;
// v86 (Mike's test): added to the fight score for every dead enemy champion; 0 for none.
inline constexpr int DEAD_ENEMY_SCORE = 0;
// Within this many tiles of the enemy spawn, only enemies that die before reaching it are chased.
inline constexpr int SPAWN_CHASE_RANGE = 8;
// v116 (CHASE_PATIENCE): an enemy out of reach that we have not got closer to (our best gap to it) for CHASE_PATIENCE
//   rounds is dropped as a target for CHASE_GIVEUP rounds, unless it channels; with every enemy dropped we farm.
//   Off: 38-42 vs v115, deaths +20% (one champion quitting mid-fight is turned on or leaves the rest short).
inline constexpr bool CHASE_PATIENCE = false;
// v117 (TEAM_CHASE): the team's gap (fewest tiles from any of us, not channelling, to any near enemy) not improved for
//   TEAM_CHASE_ROUNDS rounds turns fight into farm for TEAM_CHASE_GIVEUP rounds, for the leader's shared mode and every
//   champion's own rescore alike, so the team breaks off together. Never while engaged: an enemy within a step of one
//   of us reaching it, an enemy channelling, or an enemy within TEAM_CHASE_GUARD of an ally channelling. It reads gaps,
//   not the spawns, so maps with the spawns side by side behave the same (an enemy walking at us shrinks the gap).
inline constexpr bool TEAM_CHASE = true;
//   3 runs vs v116 (one unseeded, two fuel-burn seeds): 43-37, 42-38, 44-36 = 129/240; banked ~0, K/D a little worse.
//   8 rounds: 43-37 (one run). With RECALL_CANCEL: 36-44.
inline constexpr int TEAM_CHASE_ROUNDS = 5, TEAM_CHASE_GIVEUP = 15, TEAM_CHASE_GUARD = 6;
// v116 (CHASE_LINE_ALL): outside the chase ring too, an enemy out of reach and not channelling is only chased if it dies
//   before it walks home (chase_line); with none left we farm. Off: 36-44 vs v115 (mid-map nearly nobody qualifies).
// SPAWN_CHASE_RANGE sweep vs v115 (8): 11 38-42, 14 41-39, 18 42-38, 22 39-41; no trend, so 8 stays.
inline constexpr bool CHASE_LINE_ALL = false;
inline constexpr int CHASE_PATIENCE_ROUNDS = 4, CHASE_GIVEUP = 20;
// v119 (CATCH_MODEL): every champion walks one tile a turn, so a walking chase never closes; only a ready dash (its
//   range, then the walk), a ready stun with the enemy in its range, or the enemy stopping closes it, and the enemy's
//   own ready dash takes its range back. chase_line counted the whole gap as closable. With the switch, chase_line's
//   approach is what our dash and stun leave of the gap, and the team chase also drops once, for CATCH_DROP_ROUNDS
//   rounds in a row, nobody in sight can close on any near enemy and the team gap did not shrink.
//   Alone vs v117: 39-41. With JOIN_FIRST: 42-38, 42-38, 40-40 = 124/240, banked +3.5%, kills +7-18%, deaths -6-14%.
inline constexpr bool CATCH_MODEL = true;
inline constexpr int CATCH_DROP_ROUNDS = 2;
// v119 (JOIN_FIRST): in a fight, a champion closing on an enemy (out of reach, not channelling) with no ally in sight
//   within JOIN_RANGE, and itself outside every enemy's reach, walks to the nearest ally instead, for at most JOIN_WAIT
//   rounds of a contact. A squishy (ranged, or max health up to SQUISHY_HEALTH) also waits until a melee ally is
//   closer to the target than it is. Willow already keeps back. Debug v117 vs v116, 40 maps: fights begun with no
//   ally within 4 died 10% of the time against 6% together; 46% of deaths to champions had no ally within 4.
//   JOIN_KEEP_DASH: while waiting or closing on champions, the dash is not spent on the walk (VESPER_KEEP_DASH for all).
//   Alone vs v117 (debug, 40 maps both sides): 42-38, 40-40, 40-40, banked +3.4 / +2.5 / +5.4%, kills +15-20%, deaths
//   -11-16%. JOIN_WAIT 2: 37-43; 5: 34-46. JOIN_KEEP_DASH: 41-39 (-0.5%).
inline constexpr bool JOIN_FIRST = true, JOIN_KEEP_DASH = false;
inline constexpr int JOIN_RANGE = 4, JOIN_WAIT = 3;
// v151-f2b (CHASE_LEASH, 0 off): on teams of LEASH_TEAM or fewer, closing on an enemy also needs an ally within
//   CHASE_LEASH tiles more of the target than we are (no running ahead of the team), and the wait lasts up to
//   LEASH_WAIT rounds of a contact (not JOIN_WAIT). Ladder anvil/hooks: our HoR dies ~10 times a game; 9 of 14 HoR
//   deaths in a local anvil game came after closing on an enemy ahead of the team (1 v 3 by the time he got there).
inline constexpr int CHASE_LEASH = 3, LEASH_TEAM = 4, LEASH_WAIT = 6;
// With no ally in sight (44% of v119j's join turns): 0 holds, 1 closes anyway, 2 farms instead of closing alone.
//   1: 41-39 (+0.4%), 2: 40-40 (-4.4%); holding stays.
inline constexpr int JOIN_ALONE = 0;
// v119 (PING_GATHER): a champion that first sees a near enemy champion with no ally within PING_ALONE pings at once,
//   before it is hit, so the allies in PING_ARRIVE walk over while JOIN_FIRST holds it.
//   With JOIN_FIRST: 41-39, deaths +10%: off.
inline constexpr bool PING_GATHER = false;
// v120 (AOE_CLEAN): an area hit (Ryth's stun, Stellar's area, Varg's splash, Heart of Ruin's landing on clusters) never
//   covers a camp of another group that hits back: a hit wakes that whole group, which then hits whoever is nearest.
//   Ryth's stun aimed at the centre covering the most camps of any group, so it woke the group next door. On a camp the
//   cast only counts the target's group; in a fight the centre keeps clear of every such camp.
// v120 (STRAY_EXIT): farming, a champion that awake camps of another group hit (nearest in their reach) steps to a tile
//   still in reach of its own group and out of theirs; with none it steps out of their reach, so they sleep. The
//   farm walk keeps out of their reach too.
inline constexpr bool AOE_CLEAN = true, STRAY_EXIT = true;
// v122 shuffle fixes (v120 logs: 4% of turns were a step there and a step back, about 400 runs of 5+ turns).
// SPAWN_RING: a group that is up but whose spawn tiles are all in sight and empty is walked to the ring around them
//   (WAIT_RANGE), never onto them: standing on a spawn tile holds the whole group down, so "walk to it" and "step off"
//   took turns (2.7k turns). The ring for every out-of-sight group lost 36-44 (-10.6%): beside bushes it idled.
// BANK_STEPS: an altar counts as near (bank.near, spawn.bank.near, band.bank) by walking steps to a submit tile, not by
//   straight distance: behind a wall the walk left the range, farming walked back in (great-wall: a 77-turn run).
// BANK_STICKY: once bank.near walks to an altar, it keeps that altar for BANK_STICKY_ROUNDS while the bag and the
//   conditions hold, even out of range.
// FARM_KEEP: the group whose camps we last saw while farming stays our farm target for FARM_KEEP_ROUNDS after they
//   drop out of sight (914 of 1,043 camp/walk flip turns switched groups as a camp left and re-entered sight).
// Bench vs v120 (debug, 40 maps both sides, two runs each): BANK_STEPS 46-34 / 43-37 (+13.4 / +9.8%); + FARM_KEEP
//   49-31 / 40-40 (+15.4 / +3.1%); all three 37-43 / 39-41 (+9.4 / +4.2%, shuffling 4.0% -> 2.6%); SPAWN_RING alone 43-37
//   (+3.8%); FARM_KEEP alone 42-38 (+1.9%); BANK_STICKY alone 43-37 (+0.4%). Only BANK_STEPS ships.
inline constexpr bool SPAWN_RING = false, BANK_STEPS = true, BANK_STICKY = false, FARM_KEEP = false;
inline constexpr int BANK_STICKY_ROUNDS = 12, FARM_KEEP_ROUNDS = 3;
// Straight distance past which no walk can be in range (the field read is skipped).
inline constexpr int BANK_STEPS_SLACK = 0;
// v119 (RING_SCALE): the chase ring's radius is the spawn gap / RING_DIV, within RING_MIN..RING_MAX (else
//   SPAWN_CHASE_RANGE everywhere).
//   RING_DIV 4: 40-40, banked -2.7%: off (TEAM_CHASE_ROUNDS 3 likewise: 41-39, -2.3%).
inline constexpr bool RING_SCALE = false;
inline constexpr int RING_DIV = 4, RING_MIN = 6, RING_MAX = 14;
// v124 (with FIGHT_ENEMY_WEIGH): HEALTH_EDGE 5 -> 2: 93/160 vs v122 + v120 (seed 0), banked +21.6%, teamfights
//   613-344 (enemy weighting at 5: 83/160, +10.7%, 554-486). README bench 7.
// v128 screen: HEALTH_EDGE 1: 78/160, -1.7%; 0: 79, +3.0%; GRADED_EDGES (EDGE_WEIGHT 5): 81, +4.4%;
//   FIGHT_THRESHOLD 10: 83, +9.1%. 2 is a sharp peak (5: 83/160): part of its edge may be this seed.
// HEALTH_EDGE, POWER_EDGE, EMBLEM_BONUS, DISTANCE_PENALTY, OVEREXTEND_PENALTY: src/evo_constants.h.
// v115 (EMBLEM_RELATIVE, off): the emblem term is EMBLEM_BONUS x (our fighters' emblems - the near enemies' emblems); it
//   is ours alone, so a lone built Vesper scores +30 and stays in 1v2 fights (fragment). On it lost 34-46 vs v114 (alone)
//   and 28-52 with the Vesper switches: the team retreats more and gets run down (Vesper died more, not less).
inline constexpr bool EMBLEM_RELATIVE = false;
// v115 (GRADED_EDGES, off): health and power count EDGE_WEIGHT x (ours - theirs) / the larger side, instead of
//   +-HEALTH_EDGE and +-POWER_EDGE however close the sides are. With EMBLEM_RELATIVE: 23-57 vs v114.
inline constexpr bool GRADED_EDGES = false;
inline constexpr double EDGE_WEIGHT = 15.0;
// ---- v181: a fight score that compares both sides (bots/v177-light/DEATHS_AND_FIXES.md P1, F1/F2/F4). ----
// Ladder data (21,638 contacts, 250 games): our death rate within 20 rounds of a contact by local emblem gap: -16 56%,
//   -8 41%, -4 31%, 0 13%, +4 8%, +16 4%; our emblems alone predict deaths at AUC 0.59, the gap 0.76, health ratio 0.78.
//   On Prism maps, enemy / our deaths per contact near our Prism: bag 0-99 0.17 / 0.71, 600+ 0.54 / 0.14 (87 games).
// EMBLEM_GAP: the emblem term is EMBLEM_GAP_BONUS x (our fighters' emblems - the near enemies', both weighted as the
//   bodies), capped at +EMBLEM_GAP_CAP emblems (death rate stops falling past +4; uncapped, v115's lone built Vesper
//   scored +30); the deficit side is not capped.
// KILL_TIME: health and power edges become KILL_WEIGHT x log(the rounds they need to kill us / the rounds we need to kill
//   them), clamped to +-KILL_CAP. Rounds to kill = effective health (health + shields, / damage taken) over damage per
//   round (round_damage: strength with resets, Focus x the kit's spell multiplier / 8, x damage dealt). Prism's Focus
//   includes her bag, so it counts; so does an enemy Prism's. Behind on it counts as behind on both (EMB_STAY_STRICT).
// FIGHT_RECENTRE: without the ~+41 our emblems added to an average fight, the bar is FIGHT_BAR and staying in takes
//   FIGHT_STAY_SLACK below it (an honest score needs less hold).
// Local bench (70 maps x 2 sides vs v158-ref + v176, v180 147-133): all three 133-147; gap only 123; kill time only 129
//   (Prism maps +97 a game vs v180's +72); with the old bar (FIGHT_RECENTRE off): kill time 130, kill time at -14/-20 133,
//   gap + kill time 153-127 (Prism maps 24/36, +251 a game, deaths 7.0 -> 4.6). Head-to-head that one vs v180: 65-75
//   (Prism maps 9/18). Every recentred bar cost kills; our local bots are never far apart in emblems, so the ladder decides.
// v182: EMBLEM_GAP off. Ladder (v181 = gap + kill time, 100 games, 20 groups vs v180): 30-70 vs 35-65, our kills 7.4 ->
//   6.5, their deaths 8.4 -> 7.5, gold@600 -6%: against teams ~13 emblems ahead the gap made every fight a retreat.
inline constexpr bool EMBLEM_GAP = false, KILL_TIME = true, FIGHT_RECENTRE = false;
inline constexpr double EMBLEM_GAP_BONUS = 3.0, EMBLEM_GAP_CAP = 4.0;
inline constexpr double KILL_WEIGHT = 10.0, KILL_CAP = 20.0;
inline constexpr int FIGHT_BAR = -8, FIGHT_STAY_SLACK = 6;
// v115 (VESPER_SAFE_DASH): against champions Vesper only dashes or walks to a tile in reach of her target where her
//   health less VESPER_BURST_TURNS x the enemies' next-turn burst there (hor_burst) stays over VESPER_BUFFER; with no
//   such tile she keeps out of enemy reach. She dashes to two tiles off the weakest enemy wherever it stood. Off: 1.5 / 100
//   39-41, lethal only (1.0 / 0) 40-40, both with more of her deaths than v114's in the same games.
inline constexpr bool VESPER_SAFE_DASH = false;  // v133a deleted its code (vesper_close_safely)
// v115 (VESPER_KEEP_DASH): with an enemy champion in sight, Vesper's walks never spend the dash (move_towards_closest
//   dashed whenever it was ready, so she reached fights with nothing to escape on). Alone vs v114: 40-40, banked +5.5%,
//   team deaths -8%, kills +11%, her deaths 316 vs v114's Vesper 340.
inline constexpr bool VESPER_KEEP_DASH = true;
inline constexpr double VESPER_BURST_TURNS = 1.5;
inline constexpr int VESPER_BUFFER = 100;
// Standing on our own spawn is safe: it heals us, so a fight scores higher.
inline constexpr int SPAWN_BONUS = 50;

// Highest acts first, from the round after it is set.
inline int priority_of(Champ kind) {
    switch (kind) {
    case Champ::ren: return 7;  // v70: Ren acts first, so his stun lands before the team's damage.
    case Champ::cassi: return 6;
    // v85: Willow acts third (ties with Cassi go by draft order, so Cassi first): her heal and her retreat step
    // come before more enemy champions have acted.
    case Champ::willow: return TANK_PRIORITY ? 4 : 6;
    // v101: Heart of Ruin's dash stun lands before the rest of our damage (after Ren's, ties by draft order).
    case Champ::heart_of_ruin: return 6;
    case Champ::issac: return 5;
    case Champ::vendetta: return TANK_PRIORITY ? 6 : 4;
    case Champ::varg: return TANK_PRIORITY ? 6 : 3;
    // v110: Ryth's area stun lands before the team's damage (Ren's old slot); Marcella takes Varg's.
    case Champ::ryth: return 7;
    case Champ::marcella: return TANK_PRIORITY ? 6 : 3;
    // v123: Prism's stun-hit lands with the melee damage, before Vesper's shots.
    case Champ::prism: return 3;
    case Champ::vesper_vale: return 2;
    default: return 0;
    }
}

// Ranged bodies hold at least this far from a camp while no melee is coming in to tank it.
// A camp whose own reach is longer pushes the ring out past that reach (v94: elks and golems 5, the monolith 6).
inline constexpr int RANGED_CAMP_STANDOFF = 4;

// v94 Varg (switches): VARG_SPLASH: on a camp, he hits the camp in reach of his group whose radius-1 splash covers the
//   most other living camps (clusters sit side by side). VARG_CAMP_SHIELD: his shield (2.5 x Focus for five turns) also
//   goes up while awake camps hit him for at least VARG_SHIELD_CAMP_HIT a round (an elk or worse; the shield costs that
//   turn's attack reset).
inline constexpr bool VARG_SPLASH = true, VARG_CAMP_SHIELD = true;
inline constexpr int VARG_SHIELD_CAMP_HIT = 60;
// v101 Heart of Ruin (hor.cpp; switches): he takes Stellar's pick on five, and Varg's (HOR_SMALL_SWAP 0) or Ren's (1)
//   on three and four. HOR_DIVE: in a fight his dash (2 tiles, 1.75 x Focus and a one-turn stun in radius one) lands
//   where it hits the most enemies, a channelling one (tribute, recall) counting HOR_CHANNEL_WEIGHT, one already stunned
//   none, and only while his health and shields outlast what the enemies there can deal in a turn plus HOR_BUFFER.
//   HOR_SHIELD: the shield (1.5 x Focus, five turns) is held until his health falls to that burst plus HOR_BUFFER (his
//   damage grows as health falls; shields do not count as health), or awake camps hit him for HOR_SHIELD_CAMP_HIT a
//   round under HOR_CAMP_SHIELD_HEALTH. HOR_HOLD: under the recall bar in a fight the score likes, he stays while
//   health, shield up and shield ready outlast that burst plus HOR_BUFFER, with a bag under CARRY_GEMS. HOR_CAMP_DASH:
//   with no enemy seen for HOR_CAMP_QUIET rounds the dash goes into the target's group where it deals and stuns at
//   least HOR_CAMP_DASH_VALUE (clusters die to it). His shields count as health in the fight score.
inline constexpr int HOR_SMALL_SWAP = 0;
inline constexpr bool HOR_DIVE = true, HOR_SHIELD = true, HOR_HOLD = true, HOR_CAMP_DASH = true;
inline constexpr int HOR_BUFFER = 100;
inline constexpr int HOR_CHANNEL_WEIGHT = 3;
inline constexpr int HOR_SHIELD_CAMP_HIT = 60;
inline constexpr double HOR_CAMP_SHIELD_HEALTH = 0.7;
inline constexpr int HOR_CAMP_QUIET = 10;
inline constexpr int HOR_CAMP_DASH_VALUE = 150;
// v110 Marcella (kits.cpp marcella_cast; switches): she tanks camps (1200 health, Jungle) and hits with Strength; her
//   spell is 2 x Focus (Focus 60, +15 a shard) at range 4 and excludes walking, so it goes off only from where she
//   already stands: beside the target, or in a fight when the target is out of a step's reach. MARCELLA_STUN_RESET: in
//   a fight beside the target the cast is the enhanced reset (second hit stuns), not the spell. MARCELLA_CLAIM: she
//   collects second, after a funnelled Vesper: she gains 2 gold per gem she tributes.
inline constexpr bool MARCELLA_STUN_RESET = true, MARCELLA_CLAIM = true;
// v111: she collects second only while she owns fewer than MARCELLA_FUNNEL_EMBLEMS emblems (0: no cap, as in v110).
inline constexpr int MARCELLA_FUNNEL_EMBLEMS = 0;
// v113 (draft.cpp): on a five-player map with at least VARG_WOLVES wolves and VARG_GEM_CLUSTERS gem cluster camps,
//   Varg takes Marcella's pick (his splash clears packed wolves and clusters). Swapping on every map with 45+ cluster
//   camps lost (36/78 vs v111's 44/78 on those 13 maps): Varg loses gold-cluster maps and the three-player the-farm.
//   VARG_CLAIM: he then collects second like her (marcella_collects). VARG_MARCELLA_BUILD sits with the builds.
inline constexpr int VARG_WOLVES = 18, VARG_GEM_CLUSTERS = 12;
inline constexpr bool VARG_CLAIM = true;
// v123 Prism (draft.cpp; kits.cpp prism_cast; route.cpp prism_collects, prism_holds; switches): she takes Marcella's
//   pick on five (Varg's on Varg maps); never on four or three. A ranged champion who gains 1.5 Focus per
//   carried gem: her stun-hit (1.5 x Focus, range 4) and area (1 x Focus, radius one) both exclude walking.
// PRISM_CLAIM: gem-only rewards (gems, no gold: gem clusters) go to her first, Vesper second; rewards with gold go to a
//   funnelled Vesper first and to her second (Marcella's place). Once Vesper's funnel ends she is first for everything.
// PRISM_HOLD: she never tributes until round PRISM_CASH_ROUND or until our tributed gems plus her bag (or the team's
//   bags, win_tribute) reach WIN_GEMS. She still recalls to shop with her bag (a recall keeps it).
// She plays like Vesper, not like a mage: is_mage leaves her out (no standoff casting); she casts from where she
//   stands and still basic attacks (engage_ranged). Under SQUISHY_HEALTH (700 bare) she is a squishy like anyone,
//   until her Health emblems lift her past it.
inline constexpr bool PRISM_CLAIM = true, PRISM_HOLD = true;
inline constexpr int PRISM_CASH_ROUND = 1800;
// v123 cash-ins (bench vs v122, first version: 49 of 68 Prism games ended before round 1800, median r1405, with her
//   still holding 58 gems a game): PRISM_CASH_ENEMY_GEMS: once the enemy has tributed this many she stops holding for
//   the rest of the game (0: off). PRISM_BAG_CAP: a bag this big is banked whole (the normal bank rules), then she
//   holds again from zero (0: off).
// PRISM_CARRY_FIGHT: her bag never changes how she fights: no stepping back from the front with a heavy or hurt bag
//   (turn.cpp), and she answers pings whatever she carries (can_answer). Her bag is her Focus.
inline constexpr bool PRISM_CARRY_FIGHT = true;
inline constexpr int PRISM_CASH_ENEMY_GEMS = 750;
inline constexpr int PRISM_BAG_CAP = 300;
// v123: on five-player Varg maps (VARG_WOLVES, VARG_GEM_CLUSTERS) Prism takes the pick instead of Varg.
inline constexpr bool PRISM_OVER_VARG = true;
// v123: Prism is drafted only where the map's consistent camp gems (draft.cpp prism_gems: wolves 4, elks 15, gem
//   clusters 7; no golems or monoliths) reach the bar for its team size; else Marcella (Varg) on five, Ryth on four.
//   Sweep vs v122 (3 runs, 6 games a map): five-player bars 364-460 all score 79-80/156 (never: 78, always: 72); 380
//   drafts her on 9 maps; fresh seeds there 19-17, +1.4%. Four-player: she lost 46 of 48 games, on every map: never.
inline constexpr int PRISM_MIN_GEMS_5 = 380, PRISM_MIN_GEMS_4 = 100000;
// v85 Willow (switches): WILLOW_HEAL_STEP: heal at the start of every turn (her heal allows walking), not only
//   while attacking; a cheap check on cached vision runs before the ability is read. WILLOW_HEAL_PICK: heal the ally
//   that gains most: effective heal (missing health, at most the heal), doubled for an ally under HEAL_LOW health or
//   with an enemy champion within HEAL_DANGER tiles; any hurt ally can still be healed. WILLOW_DODGE: under recall
//   health and in reach, Willow steps out of reach toward the nearest visible ally. Priority: Willow acts third.
inline constexpr bool WILLOW_HEAL_STEP = true, WILLOW_HEAL_PICK = true, WILLOW_DODGE = true;
inline constexpr double HEAL_LOW = 0.5;
inline constexpr int HEAL_DANGER = 5, HEAL_REACH = 4;
inline constexpr float WILLOW_DODGE_RANGE = 3;
// v101 Willow combat (switches). She died ~4 times a game (30 of 40 to champions, mostly mid-fight or fleeing; 10 to
// camps, 7 of them while fleeing), and a dead Willow heals nothing.
// WILLOW_BACKLINE: in a fight she heals and stands out of enemy and awake-camp reach (threat_zone) within HEAL_REACH
//   of the ally nearest the enemy (the one taking hits), else within WILLOW_BACK_SLACK of it, else anywhere out of
//   reach. She no longer walks in to attack champions (her damage was ~760 a game). This also covers stepping back
//   after a heal.
// WILLOW_SAFE_DODGE: fleeing toward an ally only along a path that stays out of reach; otherwise the nearest safe tile.
// WILLOW_SAFE_CLAIM: Willow is not given (or keeps) a claim outside the safe zone or with an enemy champion in sight,
//   unless nobody else in sight can take it.
inline constexpr bool WILLOW_BACKLINE = true, WILLOW_SAFE_DODGE = true, WILLOW_SAFE_CLAIM = false;
inline constexpr float WILLOW_BACK_SLACK = 6;
// WILLOW_CAMP_SAFETY (camps killed her ~1.1 times a game): (a) waiting for a tank she stands out of every visible
// camp's reach, not only the target's; (b) a camp counts as tanked only by an ally at WILLOW_TANK_HEALTH or more, so
// she backs off before the tank leaves to recall; (c) a recall broken by damage from nothing in sight is not
// restarted on the same tile: she steps toward an ally (or the spawn) first.
inline constexpr bool WILLOW_CAMP_SAFETY = true;
// WILLOW_HEAL_SKIP_RECALL: never heal an ally who is recalling or on our spawn (both end at full health anyway).
inline constexpr bool WILLOW_HEAL_SKIP_RECALL = true;
// WILLOW_HEAL_THRESHOLD: heal an ally only when it misses at least HEAL_MIN_SHARE of her heal (3 x Focus): half her
// heals restored under half a heal and put the heal on its 8-turn cooldown (only 51% of her heal amount landed).
inline constexpr bool WILLOW_HEAL_THRESHOLD = true;
inline constexpr double HEAL_MIN_SHARE = 0.25;
// WILLOW_CLAIM_DROP: Willow gives a claim back as soon as an enemy champion is in her sight (23 of her 171 champion
// deaths were on a reward), and the leader does not hand her one while it sees an enemy champion (if someone else
// in sight can take it).
inline constexpr bool WILLOW_CLAIM_DROP = true;
inline constexpr double WILLOW_TANK_HEALTH = 0.5;

inline constexpr int FARM = 0, FIGHT = 1, RETREAT = 2, TRIBUTE = 3;
inline constexpr const char* MODE_NAMES[] = {"farm", "fight", "retreat", "tribute"};

// Shared memory: mode, target slot, capture slot, collector champion + 1, round stamp, v86 phase byte, capture claim
// round (low byte only; it is just aged against CAPTURE_TIMEOUT), then the ward byte the leader never rewrites.
// v86: the stamp is one byte, round % 255 + 1 (stamp_of), so zeroed memory reads as unwritten; it is only compared with
// this round's and last round's. Byte 5 holds the phase (bits 6-7, written by the leader; anyone may end a split) and
// the split pair's target (bits 0-5, NO_TARGET2 for none, written by Issac). Capture slot NO_CAPTURE means nobody is
// collecting.
// The mode byte's top bit records that Vesper has finished her build (VESPER_DONE_FLAG); only she can see her own items.
// The next bit records whether the leader put the team on the aggressive route this round.
// Bits 2-5 hold the escorted carrier's champion + 1 (0 for none); the carrier sets and clears it itself.
inline constexpr int SHARED_BYTES = 7;
// Bit i is set once ward spot i is covered, so nobody else spends a ward inside its anti-ward radius.
inline constexpr int WARD_MASK_BYTE = 7;
inline constexpr int NO_CAPTURE = 255;
inline constexpr int SLOT_BYTE = 1;
inline constexpr int CAPTURE_SLOT_BYTE = 2;
inline constexpr int MODE_BYTE = 0;
inline constexpr int STAMP_BYTE = 4, PHASE_BYTE = 5;
inline constexpr int PHASE_SHIFT = 6, TARGET2_MASK = 0x3F;
// v86: the one-byte round stamp, never 0.
inline int stamp_of(int round) { return (round % 255 + 255) % 255 + 1; }
// v94: the mode byte's top bit records that Vesper finished her core build (Ren's bit from v70 to v93): only she can
// read her own items.
inline constexpr int VESPER_DONE_FLAG = 0x80;
inline constexpr int AGGRESSIVE_FLAG = 0x40;
inline constexpr int MODE_MASK = 0x03;
inline constexpr int CARRIER_SHIFT = 2;
inline constexpr int CARRIER_MASK = 0x3C;

// Items alternate shard, emblem: every odd item index is an emblem, every even one a shard.
inline constexpr int ITEM_COUNT = static_cast<int>(Item::enum_type::count);
inline bool is_emblem(int item) { return item % 2 == 1; }
// An emblem is five 30-gold shards.
inline constexpr int EMBLEM_WORTH = 150;

inline int champ_id(Champ kind) { return static_cast<int>(kind); }
