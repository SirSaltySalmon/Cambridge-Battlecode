// Tunables, shop builds, and the shared-memory layout.
#pragma once
#include <cambc.h>
#include <cstdint>
#include <span>

namespace bc = cambc;
using bc::vec2;
using Team = bc::team;
using Champ = bc::champion_type;
using Kind = bc::ability_kind;
using Item = bc::item_type;

// Turn thresholds.
inline constexpr double RECALL_HEALTH = 0.35;
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
inline constexpr int VESPER_RECALL_GOLD = 600;
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
// Every group is a possible stop on the way, except monoliths until the team owns this many emblems.
inline constexpr int MONOLITH_EMBLEMS = 12;
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
inline constexpr int VESPER_CORE_EMBLEMS = 4;
// v70: Ren rushes REN_CORE_EMBLEMS: until then loot is funnelled to him, he recalls at every emblem's worth
// of gold (before banking) and banks like everyone else. The team turns aggressive once Ren owns
// REN_CORE_EMBLEMS and the team TEAM_AGGRESSIVE_EMBLEMS. From REN_CORE_EMBLEMS on, Ren banks only at
// REN_BANK_GEMS, stuns enemy champions before camps, and takes a fight only with an ally this close.
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
inline int camp_kind_strength(bc::camp_type kind) {
    switch (kind) {
    case bc::camp_type::wolf: return 20;
    case bc::camp_type::elk: return 60;
    case bc::camp_type::monolith: return 200;
    default: return 0;
    }
}
// v81: every camp, the monolith included, hits at 3 (the SDK; the handbook table's 5 for the monolith is wrong).
inline constexpr int CAMP_REACH = 3, MONOLITH_REACH = 3;
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
// v72: the hurt-dodge step (over half of our ladder deaths with gems) dashes out of reach when it can, stuns a
// chaser if the stun allows walking (Cassi), walks out toward an ally within ESCAPE_ALLY_RANGE, and a melee
// hits back on the way. Off: fewer deaths (-12%) but 16-18 vs v72 without it and 86-50 vs v71/v70/v64-bushward/v64 (91-45).
inline constexpr bool FIX_DODGE = false;
// v72: on hard maps we captured twice as many gold clusters (35 gold, no gems) as ladder teams. With FIX_GEM_FIRST
// a gold-only group is taken on the way only within GOLD_ONLY_NEAR steps.
inline constexpr bool FIX_GEM_FIRST = true;
inline constexpr int GOLD_ONLY_NEAR = 2;
// ...and its value in the draft plan and the route is scaled by GEMLESS_VALUE.
inline constexpr double GEMLESS_VALUE = 0.25;
// v74: a champion on spawn with a bag an off-spawn bank rule would bank banks from there, and only an arrival
// (respawn, recall) sets rejoining. Fixes the spawn.leave / bank.walk loop on corners, ambush, packs, rows.
inline constexpr bool FIX_SPAWN_BANK = true;
// v75: the fight score counts only allies who can join the fight: not channelling (recall, tribute, teleport)
// and within FIGHT_ALLY_RANGE of the enemy nearest the fight.
inline constexpr bool FIX_FIGHT_BODIES = false;
// v76: three stuck states from a rushbait_v2 ladder loss: closing from outside on an enemy home in its chase ring
// (then leaving the ring, forever), a ranged champion holding back from a camp for a tank that never comes (at
// most HOLD_LIMIT rounds now), and waiting on spawn for full health while enemies keep hitting us.
inline constexpr bool FIX_STUCK_RING = true, FIX_STUCK_HOLD = true, FIX_STUCK_HEAL = true;
inline constexpr int HOLD_LIMIT = 30;
inline constexpr int SPAWN_WAIT_LIMIT = 15;
// v76: an enemy within HOME_CAMP_STEPS walking steps of its spawn heals there every turn: it is not near for the
// fight decision or a target (FIX_HOME_CAMPERS).
inline constexpr bool FIX_HOME_CAMPERS = true;
inline constexpr int HOME_CAMP_STEPS = 1;
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

// v81 mages (Cassi and Stellar): their spells reach MAGE_CAST_RANGE, one past every camp's reach (3), and their basic
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
// MAGE_MONOLITH: melees never hit a monolith (its 200 a hit cost them more than it gives); mages take it with spells
//   from outside its reach while the rest of the team farms and holds the area. Monoliths leave the shared route.
inline constexpr bool MAGE_STANDOFF = true, MAGE_LEAVE = true, MAGE_KITE = true, MAGE_MONOLITH = true;
inline constexpr double MAGE_FINISH_SHARE = 0.10;
inline constexpr int MAGE_CAST_RANGE = 4;
inline constexpr int MELEE_THREAT = 2, RANGED_THREAT = 3;
// A monolith is near the team's target (or near us while that target respawns) within MONOLITH_JOB_RANGE. One we
// worked on within MONOLITH_RESUME rounds is resumed wherever the team is.
inline constexpr int MONOLITH_JOB_RANGE = 10;
inline constexpr int MONOLITH_RESUME = 40;
// On a gem-starved map the monolith is the only gem source, and two mages' spells alone take it too slowly (contest:
// ~36 a round, 20,000 -> 8,760 in the last 850 rounds). With MONOLITH_TEAM_STARVED the whole team still hits it
// there (v80-5's rule), the mages from outside its reach unless a tank holds it (MAGE_STANDOFF).
inline constexpr bool MONOLITH_TEAM_STARVED = true;
// Melees hold this ring around a monolith the mages are on: outside its reach, close enough to answer an enemy.
inline constexpr int MONOLITH_GUARD_RANGE = 5;
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
inline constexpr int NO_TARGET2 = 63;
// Capture spacing: while a reward outside the safe zone is being captured, the rest only move on to groups within
// CAPTURE_NEAR tiles of it, so the collector stays in sight of the team.
inline constexpr bool V86_CAPTURE_NEAR = true;
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
inline constexpr int FIGHT_THRESHOLD = 5;
// Indecision guard: once fighting, the team only retreats when the score falls this far below the
// threshold, and a switch between fight and retreat holds for at least this many rounds.
inline constexpr int FIGHT_HYSTERESIS = 10;
inline constexpr int MIN_HOLD = 3;
inline constexpr int OUTNUMBER_BONUS = 10;
inline constexpr int MISSING_BODY_PENALTY = 5;
// Prefer farming during the respawn window after enough enemy champions are down (team size -> dead).
inline int kill_window_dead_enemies(std::size_t team) { return team == 3 || team == 4 ? 2 : team == 5 ? 3 : -1; }
inline constexpr int KILL_WINDOW_FIGHT_PENALTY = 20;
// Within this many tiles of the enemy spawn, only enemies that die before reaching it are chased.
inline constexpr int SPAWN_CHASE_RANGE = 8;
inline constexpr int HEALTH_EDGE = 5;
inline constexpr int POWER_EDGE = 5;
inline constexpr int EMBLEM_BONUS = 3;
inline constexpr double DISTANCE_PENALTY = 0.5;
inline constexpr int OVEREXTEND_PENALTY = 10;
// Standing on our own spawn is safe: it heals us, so a fight scores higher.
inline constexpr int SPAWN_BONUS = 50;

// Highest acts first, from the round after it is set.
inline int priority_of(Champ kind) {
    switch (kind) {
    case Champ::ren: return 7;  // v70: Ren acts first, so his stun lands before the team's damage.
    case Champ::cassi: return 6;
    // v85: Willow acts third (ties with Cassi go by draft order, so Cassi first): her heal and her retreat step
    // come before more enemy champions have acted.
    case Champ::willow: return 6;
    case Champ::issac: return 5;
    case Champ::vendetta: return 4;
    case Champ::varg: return 3;
    case Champ::vesper_vale: return 2;
    default: return 0;
    }
}

// Ranged bodies hold at least this far from a camp while no melee is coming in to tank it.
// A camp whose own reach is longer would push the ring out past that reach (every camp hits at 3).
inline constexpr int RANGED_CAMP_STANDOFF = 4;

// One family at a time until its emblem is done; the fallback shard is bought without limit.
inline constexpr Item MELEE_STRENGTH[] = {Item::jungle_shard_of_strength, Item::monster_shard_of_strength,
    Item::durability_shard_of_health};
inline constexpr Item VARG_STRENGTH[] = {Item::jungle_shard_of_strength, Item::monster_shard_of_strength,
    Item::bruiser_shard_of_strength};
inline constexpr Item VENDETTA_STRENGTH[] = {Item::jungle_shard_of_strength, Item::monster_shard_of_strength,
    Item::power_shard_of_strength};
inline constexpr Item MELEE_FOCUS[] = {Item::jungle_shard_of_focus, Item::bruiser_shard_of_focus,
    Item::durability_shard_of_focus};
// v70 Ren: Bruiser Health, Durability Health, then Power Focus without limit.
// v83: Jungle Health first (camps hit Ren 30% less while he tanks them), then Durability; Bruiser Focus stacks after.
inline constexpr Item REN_BUILD[] = {Item::jungle_shard_of_health, Item::durability_shard_of_health};
// v70 Stellar: Power Focus, then Bruiser Health, then Power Focus without limit.
inline constexpr Item STELLAR_BUILD[] = {Item::power_shard_of_focus, Item::bruiser_shard_of_health};
inline constexpr Item RANGED_FOCUS[] = {Item::power_shard_of_focus, Item::monster_shard_of_focus,
    Item::durability_shard_of_focus};
// v83: Willow builds Durability first (10% less damage taken), then Power, instead of Monster (more damage to camps): she heals and
// stays alive rather than farming camps. Cassi keeps RANGED_FOCUS.
// v85 Willow (switches): WILLOW_HEAL_STEP: heal at the start of every turn (her heal allows walking), not only
//   while attacking; a cheap check on cached vision runs before the ability is read. WILLOW_HEAL_PICK: heal the ally
//   that gains most: effective heal (missing health, at most the heal), doubled for an ally under HEAL_LOW health or
//   with an enemy champion within HEAL_DANGER tiles; any hurt ally can still be healed. WILLOW_DODGE: under recall
//   health and in reach, Willow steps out of reach toward the nearest visible ally. Priority: Willow acts third.
inline constexpr bool WILLOW_HEAL_STEP = true, WILLOW_HEAL_PICK = true, WILLOW_DODGE = true;
inline constexpr double HEAL_LOW = 0.5;
inline constexpr int HEAL_DANGER = 5, HEAL_REACH = 4;
inline constexpr float WILLOW_DODGE_RANGE = 3;
inline constexpr Item WILLOW_FOCUS[] = {Item::durability_shard_of_focus, Item::power_shard_of_focus};
// Ryth's passive turns bonus health into Strength, and Jungle cuts the camp hits he stands in.
inline constexpr Item RYTH_HEALTH[] = {Item::jungle_shard_of_health, Item::durability_shard_of_health,
    Item::bruiser_shard_of_health};
// Vesper Vale's kit is all basic attacks, so she builds Strength only
inline constexpr Item VESPER_STRENGTH[] = {Item::monster_shard_of_strength, Item::power_shard_of_strength,
    Item::durability_shard_of_health, Item::power_shard_of_health};

inline std::span<const Item> shop_order(Champ kind) {
    switch (kind) {
    case Champ::vendetta: return VENDETTA_STRENGTH;
    case Champ::varg: return VARG_STRENGTH;
    case Champ::issac: return MELEE_STRENGTH;
    case Champ::ren: return REN_BUILD;
    case Champ::stellar: return STELLAR_BUILD;
    case Champ::cassi: return RANGED_FOCUS;
    case Champ::willow: return WILLOW_FOCUS;
    case Champ::ryth: return RYTH_HEALTH;
    case Champ::vesper_vale: return VESPER_STRENGTH;
    default: return {};
    }
}

inline constexpr Item FALLBACK_VENDETTA[] = {Item::power_shard_of_strength};
inline constexpr Item FALLBACK_MELEE[] = {Item::power_shard_of_strength, Item::power_shard_of_health};
inline constexpr Item FALLBACK_REN[] = {Item::bruiser_shard_of_focus};
inline constexpr Item FALLBACK_RANGED[] = {Item::power_shard_of_focus};
inline constexpr Item FALLBACK_RYTH[] = {Item::bruiser_shard_of_health};
inline constexpr Item FALLBACK_VESPER[] = {Item::power_shard_of_strength, Item::power_shard_of_strength,
    Item::power_shard_of_health};

inline std::span<const Item> shop_fallback(Champ kind) {
    switch (kind) {
    case Champ::vendetta: return FALLBACK_VENDETTA;
    case Champ::varg: case Champ::issac: return FALLBACK_MELEE;
    case Champ::ren: return FALLBACK_REN;
    case Champ::cassi: case Champ::willow: case Champ::stellar: return FALLBACK_RANGED;
    case Champ::ryth: return FALLBACK_RYTH;
    case Champ::vesper_vale: return FALLBACK_VESPER;
    default: return {};
    }
}

inline constexpr int FARM = 0, FIGHT = 1, RETREAT = 2, TRIBUTE = 3;
inline constexpr const char* MODE_NAMES[] = {"farm", "fight", "retreat", "tribute"};

// Shared memory: mode, target slot, capture slot, collector champion + 1, round stamp, v86 phase byte, capture claim
// round (low byte only; it is just aged against CAPTURE_TIMEOUT), then the ward byte the leader never rewrites.
// v86: the stamp is one byte, round % 255 + 1 (stamp_of), so zeroed memory reads as unwritten; it is only compared with
// this round's and last round's. Byte 5 holds the phase (bits 6-7, written by the leader; anyone may end a split) and
// the split pair's target (bits 0-5, NO_TARGET2 for none, written by Issac). Capture slot NO_CAPTURE means nobody is
// collecting.
// The mode byte's top bit records that Vesper has finished her build; only she can see her own items.
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
// v70: the mode byte's top bit records that Ren finished his core build (Vesper's old bit): only Ren can
// read his own items.
inline constexpr int REN_DONE_FLAG = 0x80;
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
