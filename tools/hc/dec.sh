#!/bin/sh
# usage: tools/hc/dec.sh REPLAY.repl26a OUT.jsonl  (decode a local replay bundle)
T=$(mktemp -d); unzip -qo "$1" replay.repl26a -d "$T"; node "$(dirname "$0")/../replay/replay.mjs" "$T/replay.repl26a" > "$2"; rm -rf "$T"
