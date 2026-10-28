// EVO COPY of v148-normal: the fight-score numbers tools/evotune varies (moved out of constants.h, same values).
// Every `inline constexpr int|double|float NAME = literal;` here is tuned. Optional comment on the line above:
//   // evo: delta=D min=A max=B   (default delta: 20% of the value, ints at least 1; default range 1/4x..4x)
#pragma once

inline constexpr int FIGHT_THRESHOLD = 5;
inline constexpr int FIGHT_HYSTERESIS = 10;
inline constexpr int MIN_HOLD = 3;
inline constexpr int OUTNUMBER_BONUS = 10;
inline constexpr int MISSING_BODY_PENALTY = 5;
inline constexpr int HEALTH_EDGE = 2;
inline constexpr int POWER_EDGE = 5;
inline constexpr int EMBLEM_BONUS = 3;
inline constexpr double DISTANCE_PENALTY = 0.5;
inline constexpr int OVEREXTEND_PENALTY = 10;
