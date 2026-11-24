#!/bin/zsh
cd "/Users/baotran/Battlecode 2026/Cambridge-Battlecode"
B=bots/v216-contract/bench
until mkdir .bench.lock 2>/dev/null; do sleep 5; done
echo "C $(date) stack s0 + contract s1" > .bench.lock/owner
trap 'rm -rf .bench.lock' EXIT
P=.venv/bin/python
POOL="bots/v210 bots/v206 tuner/work/pool/v208 tuner/work/pool/v185"
$P tuner/evaluate.py bots/v216-contract-stack --vs ${=POOL} --jobs 1 --out $B/stack-s0 > $B/stack-s0.txt 2>&1
$P tuner/evaluate.py bots/v216-contract-s1 --vs ${=POOL} --jobs 1 --out $B/on-s1 > $B/on-s1.txt 2>&1
echo done
