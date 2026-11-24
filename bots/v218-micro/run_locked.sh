#!/bin/sh
# Chat F bench queue: waits for the shared bench lock, runs each "BOT OPP TAG [extra args]" line of $1 in order at
# --jobs 1 (release unless the extra args say otherwise), writes bots/v216-micro/bench/<TAG>.txt, then releases the lock.
cd "$(dirname "$0")/../.." || exit 1
queue="$1"
until mkdir .bench.lock 2>/dev/null; do sleep 20; done
echo "F $(date) micro queue $queue" > .bench.lock/owner
trap 'rm -rf .bench.lock' EXIT
while read -r bot opp tag extra; do
  [ -z "$bot" ] && continue
  case "$bot" in \#*) continue;; esac
  echo "$(date) start $tag" >> bots/v216-micro/bench/queue.log
  # shellcheck disable=SC2086
  .venv/bin/python tuner/evaluate.py "$bot" --vs "$opp" --jobs 1 --out "bots/v216-micro/bench/out-$tag" $extra \
    > "bots/v216-micro/bench/$tag.txt" 2>&1
  echo "$(date) done $tag $(head -1 bots/v216-micro/bench/$tag.txt)" >> bots/v216-micro/bench/queue.log
done < "$queue"
