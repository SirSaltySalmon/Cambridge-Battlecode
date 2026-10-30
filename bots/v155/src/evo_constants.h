// Fight-score numbers tools/evotune varies (moved out of constants.h; included from there, same values).
// Every `inline constexpr int|double NAME = number;` here is tuned; `// evo: delta= min= max=` sets its step and range.
// Deltas: about a fifth of each term's usual size in a fight score; ranges keep each term's sign (a 0 cap would mean off).
// EMBLEM_BONUS is a double so it can move by half an emblem point (lround(EMBLEM_BONUS x emblems) as before).
#pragma once
// evo: delta=2 min=-25 max=20
inline constexpr int FIGHT_THRESHOLD = -14;  // Jack v150 (Optuna fight trial 43); v149 -5
// evo: delta=2 min=0 max=30
inline constexpr int FIGHT_HYSTERESIS = 15;
// evo: delta=1 min=1 max=8
inline constexpr int MIN_HOLD = 1;
// evo: delta=2 min=0 max=30
inline constexpr int OUTNUMBER_BONUS = 0;
// evo: delta=1 min=0 max=15
inline constexpr int MISSING_BODY_PENALTY = 7;
// evo: delta=1 min=0 max=10
inline constexpr int HEALTH_EDGE = 7;
// evo: delta=1 min=0 max=15
inline constexpr int POWER_EDGE = 2;
// evo: delta=0.5 min=0 max=8
inline constexpr double EMBLEM_BONUS = 3.0;
// evo: delta=0.1 min=0 max=2
inline constexpr double DISTANCE_PENALTY = 0.1;
// evo: delta=2 min=0 max=30
inline constexpr int OVEREXTEND_PENALTY = 20;
