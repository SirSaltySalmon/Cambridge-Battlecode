"""vesper_logic.py - Tactical tribute, danger management, and combat for Vesper Vale."""
from typing import Optional
import cambc as bc

# Reusable bitboards: zero per-turn heap allocation & no GC pressure
_ENEMY_MASK = bc.Bitboard()
_ATTACK_RANGE_MASK = bc.Bitboard()
_DASH_RANGE_MASK = bc.Bitboard()
_SCRATCH_MASK = bc.Bitboard()
_SURROUND_MASK = bc.Bitboard()


def _is_adjacent_to_altar(pos: bc.Vec2) -> Optional[bc.Vec2]:
    """Returns adjacent altar tile if within Chebyshev distance 1."""
    _SURROUND_MASK.clear()
    _SURROUND_MASK.draw_chebyshev(pos, 1)
    _SURROUND_MASK &= bc.game_map.altars()
    return _SURROUND_MASK.positions()[0] if not _SURROUND_MASK.is_empty() else None


def _find_lowest_hp_enemy(mask: bc.Bitboard) -> Optional[bc.Champion]:
    """Finds the lowest HP visible enemy within mask without allocating/sorting lists."""
    target: Optional[bc.Champion] = None
    min_hp = 1e9
    for pos in mask.positions():
        try:
            enemy = bc.vision.champion_at(pos)
            hp = enemy.health()
            if hp < min_hp:
                min_hp = hp
                target = enemy
        except Exception:
            continue
    return target


def _execute_defensive_dash(me: bc.Champion, my_pos: bc.Vec2, slot1: bc.Ability) -> bool:
    """Kites away from all adjacent melee threats simultaneously (allows jumping holes)."""
    if not slot1.is_ready() or not me.can_cast():
        return False

    slot1.mask_into(_DASH_RANGE_MASK)

    # Dashes may jump over holes, but cannot land on walls, holes, altars, or enemy units
    landing_blocked = (
        bc.game_map.walls()
        | bc.game_map.holes()
        | bc.game_map.altars()
        | _ENEMY_MASK
    )
    _DASH_RANGE_MASK &= ~landing_blocked
    dash_candidates = _DASH_RANGE_MASK.positions()
    if not dash_candidates:
        return False

    threats = _SCRATCH_MASK.positions()
    best_tile = None
    max_min_dist = -1

    # Maximize distance relative to ALL adjacent melee threats
    for candidate in dash_candidates:
        min_dist = min(candidate.chebyshev_distance(t) for t in threats)
        if min_dist > max_min_dist:
            max_min_dist = min_dist
            best_tile = candidate

    if best_tile and max_min_dist > 1:
        try:
            return slot1.use(best_tile)
        except Exception:
            return False
    return False


def play_vesper_tribute_and_combat(navigator=None) -> bool:
    """Evaluates danger, combat retaliation, and gem tributing for Vesper Vale.

    Returns True if an action or dedicated movement was taken, allowing
    external state machines to fall back to general roaming/farming otherwise.
    """
    me = bc.me()
    if not me.is_alive():
        return False

    my_pos = me.position()
    enemy_team = bc.Team.opponent(me.team())

    # 1. Capture visible enemies into bitboard
    bc.vision.champion_tiles_into(_ENEMY_MASK, enemy_team)
    enemy_count = _ENEMY_MASK.count()

    abilities = me.abilities()
    slot0, slot1 = abilities[0], abilities[1]

    # =========================================================================
    # DANGER OVERRIDE: 2 or more enemies in sight
    # =========================================================================
    if enemy_count >= 2:
        # Abort tribute channel immediately to unfreeze attack/ability allowances
        if me.is_submitting_gems():
            me.cancel_submit_gems()

        # Find lowest HP enemy within attack range (Chebyshev <= 3)
        _ATTACK_RANGE_MASK.clear()
        _ATTACK_RANGE_MASK.draw_chebyshev(my_pos, 3)
        _ATTACK_RANGE_MASK &= _ENEMY_MASK

        target = _find_lowest_hp_enemy(_ATTACK_RANGE_MASK) if not _ATTACK_RANGE_MASK.is_empty() else None

        # Basic Attack 1
        if target and me.can_attack():
            try:
                me.attack(target)
            except Exception:
                pass

        # Burst / Reset: Slot 0 restores attack allowance (or allows double attack)
        if slot0.is_ready() and me.can_cast():
            try:
                slot0.use()
                if target and me.can_attack():
                    me.attack(target)
                    return True
            except Exception:
                pass

        # Defensive Kiting: Dash away if any enemy entered melee range (<= 1)
        _SCRATCH_MASK.clear()
        _SCRATCH_MASK.draw_chebyshev(my_pos, 1)
        _SCRATCH_MASK &= _ENEMY_MASK
        if not _SCRATCH_MASK.is_empty():
            if _execute_defensive_dash(me, my_pos, slot1):
                return True

        return target is not None

    # =========================================================================
    # SAFE TRIBUTE: < 2 enemies in sight
    # =========================================================================
    carried_gems = me.gem_count()
    if carried_gems > 0:
        if me.is_submitting_gems():
            return True  # Actively channeling tribute; hold position

        altar_tile = _is_adjacent_to_altar(my_pos)
        # Opportunistic tribute: if already touching an altar, deposit any gems
        if altar_tile:
            try:
                me.submit_gems(altar_tile)
                return True
            except Exception:
                pass

        # Game win condition: instant tribute if carried gems complete the 1,000 gem win
        can_win_now = (bc.global_submitted_gems(me.team()) + carried_gems) >= 1000

        # Minimum threshold of 40 gems required to initiate travel to an altar
        GEM_MINIMUM = 40
        if (carried_gems >= GEM_MINIMUM or can_win_now) and navigator and me.can_move():
            delta = navigator.step_towards_goals(bc.game_map.altars())
            if delta:
                try:
                    return me.move(delta)
                except Exception:
                    pass

    # =========================================================================
    # SINGLE-ENEMY COMBAT SKIRMISH
    # =========================================================================
    if enemy_count == 1:
        _ATTACK_RANGE_MASK.clear()
        _ATTACK_RANGE_MASK.draw_chebyshev(my_pos, 3)
        _ATTACK_RANGE_MASK &= _ENEMY_MASK
        if not _ATTACK_RANGE_MASK.is_empty():
            target = _find_lowest_hp_enemy(_ATTACK_RANGE_MASK)
            if target and me.can_attack():
                try:
                    me.attack(target)
                except Exception:
                    pass
            if slot0.is_ready() and me.can_cast():
                try:
                    slot0.use()
                    if target and me.can_attack():
                        me.attack(target)
                except Exception:
                    pass
            return True

    return False  # Yields control back to your team's progression/farming logic