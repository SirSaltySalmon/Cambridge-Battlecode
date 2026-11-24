# v213 (v210 + FILL_WIDE)

The team stood beside a respawning group instead of farming: with our side's groups down, FIX_FILL found no filler
because the phase's zones dropped every group that was up (12.5% of champion turns in farm.wait over 10 maps).

- FILL_WIDE (route.cpp pick_target): with no filler in the phase's zones, the same gain test over every zone, skipping
  groups within 6 of an enemy seen in the last 10 rounds and not while 2+ emblems a champion behind. Prism team too.
- Bench vs v210 (86 maps x2 vs proxy-farmer, v206, v185): 302 vs 291 of 516, gems +1.6%, deaths -0.7%,
  fuel 37.1k vs 36.8k mean, out-of-fuel turns +5%.
