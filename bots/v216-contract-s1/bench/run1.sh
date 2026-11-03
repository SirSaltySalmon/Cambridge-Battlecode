#!/bin/zsh
cd "/Users/baotran/Battlecode 2026/Cambridge-Battlecode"
B=bots/v216-contract/bench
until mkdir .bench.lock 2>/dev/null; do sleep 20; done
echo "C $(date) contract screen" > .bench.lock/owner
trap 'rm -rf .bench.lock' EXIT
P=.venv/bin/python
$P tuner/evaluate.py bots/v213 --vs bots/v210 --jobs 1 --fuel-quantum 100000000 --out $B/q-v213 > $B/q-v213.txt 2>&1
$P tuner/evaluate.py bots/v216-contract-w --vs bots/v210 --jobs 1 --fuel-quantum 100000000 --out $B/q-w > $B/q-w.txt 2>&1
$P tuner/evaluate.py bots/v216-contract-off --vs bots/v210 --jobs 1 --out $B/off-v210 > $B/off-v210.txt 2>&1
$P tuner/evaluate.py bots/v216-contract --vs bots/v210 --jobs 1 --out $B/on-v210 > $B/on-v210.txt 2>&1
$P tuner/evaluate.py bots/v216-contract --vs bots/v213 --jobs 1 --out $B/on-v213 > $B/on-v213.txt 2>&1
echo done
