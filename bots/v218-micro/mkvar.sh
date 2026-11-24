#!/bin/sh
# usage: bots/v218-micro/mkvar.sh <suffix> NAME=VALUE ...   -> bots/v218-micro-<suffix> (src copied from bots/v218-micro)
set -e
cd "$(dirname "$0")/.."
dst="v218-micro-$1"; shift
rm -rf "$dst"; mkdir "$dst"; cp -R v218-micro/src "$dst/src"
for kv in "$@"; do
  k="${kv%%=*}"; v="${kv#*=}"
  grep -q "inline constexpr [a-z:_0-9]* $k = " "$dst/src/constants.h" || { echo "no constant $k"; exit 1; }
  sed -i '' -E "s/(inline constexpr [a-z:_0-9]+ $k = )[^;]+;/\1$v;/" "$dst/src/constants.h"
  grep -n "inline constexpr [a-z:_0-9]* $k = " "$dst/src/constants.h"
done
