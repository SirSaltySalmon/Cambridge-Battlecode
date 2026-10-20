#!/bin/sh
# Run benches one after another: each line of QUEUE is "BOT OPP [--unlimited] [--release]".
# Summaries go to results/results-BOT-vs-OPP-<normal|unlimited>.txt.
# usage: sh tools/queue.sh QUEUE
PY=/c/Users/Admin/AppData/Local/Programs/Python/Python314/python.exe
cd "$(dirname "$0")/.."
while read -r bot opp flags; do
    [ -z "$bot" ] && continue
    kind=normal
    case "$flags" in *--unlimited*) kind=unlimited ;; esac
    case "$flags" in *--release*) kind="$kind-release" ;; esac
    out="results/results-$bot-vs-$opp-$kind.txt"
    [ -s "$out" ] && continue
    $PY -I tools/bench.py run "$bot" "$opp" $flags > "$out.tmp" 2>&1 && mv "$out.tmp" "$out"
    tail -1 "$out"
done < "$1"
