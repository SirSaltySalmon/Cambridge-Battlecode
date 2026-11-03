#!/bin/bash
# usage: mkvar.sh NAME HYG PT SHOP PREARM -- copies bots/v217-farm/src into bots/NAME with Nic's v202 switches set.
#   HYG = CAPTURE_HYGIENE, PT = PT_GATE + PT_GATE_MONO + PT_COMMIT + PT_SCAR, SHOP = SHOP_CADENCE, PREARM = PREARM_READ.
set -e
cd "$(dirname "$0")/.."
n=$1
rm -rf "$n"; mkdir -p "$n"; cp -R v217-farm/src "$n/"
sed -i '' -e "s/^inline constexpr bool CAPTURE_HYGIENE = .*/inline constexpr bool CAPTURE_HYGIENE = $2;/" \
          -e "s/^inline constexpr bool PT_GATE = .*/inline constexpr bool PT_GATE = $3;/" \
          -e "s/^inline constexpr bool PT_GATE_MONO = .*/inline constexpr bool PT_GATE_MONO = $3;/" \
          -e "s/^inline constexpr bool PT_COMMIT = .*/inline constexpr bool PT_COMMIT = $3;/" \
          -e "s/^inline constexpr bool PT_SCAR = .*/inline constexpr bool PT_SCAR = $3;/" \
          -e "s/^inline constexpr bool SHOP_CADENCE = .*/inline constexpr bool SHOP_CADENCE = $4;/" \
          -e "s/^inline constexpr bool PREARM_READ = .*/inline constexpr bool PREARM_READ = $5;/" "$n/src/constants.h"
sed "s#bots/v216-focus-v#bots/$n#g" v216-focus-v/compile_commands.json > "$n/compile_commands.json"
grep -c "^inline constexpr bool \(CAPTURE_HYGIENE\|PT_GATE\|PT_GATE_MONO\|PT_COMMIT\|PT_SCAR\|SHOP_CADENCE\|PREARM_READ\) = $2\|= $3\|= $4\|= $5;" "$n/src/constants.h" >/dev/null
grep -n "^inline constexpr bool \(CAPTURE_HYGIENE\|PT_GATE\|PT_GATE_MONO\|PT_COMMIT\|PT_SCAR\|SHOP_CADENCE\|PREARM_READ\) " "$n/src/constants.h" | sed "s/^/$n: /"
