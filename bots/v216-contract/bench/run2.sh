#!/bin/zsh
cd "/Users/baotran/Battlecode 2026/Cambridge-Battlecode"
B=bots/v216-contract/bench
until mkdir .bench.lock 2>/dev/null; do sleep 10; done
echo "C $(date) contract pool+gather" > .bench.lock/owner
trap 'rm -rf .bench.lock' EXIT
P=.venv/bin/python
POOL="bots/v210 bots/v206 tuner/work/pool/v208 tuner/work/pool/v185"
$P tuner/evaluate.py bots/v216-contract-off --vs bots/v210 --jobs 1 --fuel-quantum 100000000 --out $B/q-off > $B/q-off.txt 2>&1
$P tuner/evaluate.py bots/v216-contract --vs ${=POOL} --jobs 1 --out $B/on-s0 > $B/on-s0.txt 2>&1
$P tuner/evaluate.py bots/v216-contract-g --vs bots/v213 bots/v210 --jobs 1 --out $B/g-s0 > $B/g-s0.txt 2>&1
$P tuner/evaluate.py bots/v216-contract-s1 --vs ${=POOL} --jobs 1 --out $B/on-s1 > $B/on-s1.txt 2>&1
$P tuner/evaluate.py bots/v216-contract-s2 --vs ${=POOL} --jobs 1 --out $B/on-s2 > $B/on-s2.txt 2>&1
echo done
